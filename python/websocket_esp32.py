import os
import random
import wave
from datetime import datetime, timezone
from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from sqlmodel import Session, select
from database import engine
from models import Aluno, Sessao, Tentativa, Pareamento
from asr import normalize_string, levenshtein, transcrever_fala, carregar_palavras
from config import AUDIO_DIR
from dashboard_ws import gerenciador

router = APIRouter()

SAMPLE_RATE = 16000
SAMPLE_WIDTH = 2  # 2 bytes = 16-bit PCM

def _resolver_aluno_pareado(db: Session, pin: str):
    """Retorna o Aluno pareado atualmente com esse PIN, ou None se não houver pareamento ativo."""
    if not pin:
        return None
    pareamento = db.exec(
        select(Pareamento).where(Pareamento.pin == pin, Pareamento.ativo == True)  # noqa: E712
    ).first()
    if not pareamento:
        return None
    return db.get(Aluno, pareamento.aluno_id)

def _salvar_audio_permanente(caminho_origem: str, sessao_id: int) -> str:
    """Copia o voz.wav temporário para /audios, com nome único (sessão + timestamp)."""
    nome_arquivo = f"sessao{sessao_id}_{datetime.now(timezone.utc).strftime('%Y%m%d%H%M%S%f')}.wav"
    destino = os.path.join(AUDIO_DIR, nome_arquivo)
    with open(caminho_origem, "rb") as origem, open(destino, "wb") as dest:
        dest.write(origem.read())
    return f"audios/{nome_arquivo}"

@router.websocket("/ws/esp32")
async def websocket_esp32(websocket: WebSocket):
    await websocket.accept()
    print(f"[NET] Cliente conectado: {websocket.client}")

    # ---------- Estado desta conexão (equivalente às variáveis locais do backend.py antigo) ----------
    expected_norm = ""
    palavra_atual = ""
    nivel_atual = 1
    audio_buffer = bytearray()
    pin_atual = ""
    sessao_atual_id = None  # id da Sessao (ciclo de 3 palavras) em andamento
    caminho_voz = os.path.join(os.path.dirname(os.path.abspath(__file__)), "voz.wav")

    try:
        while True:
            mensagem = await websocket.receive()
            if mensagem.get("type") == "websocket.disconnect":
                break

            # ---------- 1. Mensagens de texto (comandos de controle) ----------
            texto = mensagem.get("text")
            if texto is not None:
                comando = texto.strip()

                if comando.startswith("pedir_palavra"):
                    # Formato esperado: "pedir_palavra <nivel> <pin>"
                    # (o "<pin>" é opcional -- firmware antigo continua funcionando,
                    # só que sem persistência, já que _resolver_aluno_pareado("") -> None)
                    partes = comando.split()
                    nivel_atual = 1
                    if len(partes) > 1:
                        try:
                            nivel_atual = int(partes[1])
                        except ValueError:
                            pass
                    pin_atual = partes[2] if len(partes) > 2 else ""

                    palavras = carregar_palavras(nivel_atual)
                    palavra_atual = random.choice(palavras)
                    expected_norm = normalize_string(palavra_atual)

                    with Session(engine) as db:
                        aluno = _resolver_aluno_pareado(db, pin_atual)
                        # Nível 1 sempre abre um novo ciclo (nova Sessao de até 3 palavras)
                        if aluno and (sessao_atual_id is None or nivel_atual == 1):
                            nova_sessao = Sessao(
                                aluno_id=aluno.id,
                                nivel_atingido=nivel_atual,
                                pin_pareamento=pin_atual,
                            )
                            db.add(nova_sessao)
                            db.commit()
                            db.refresh(nova_sessao)
                            sessao_atual_id = nova_sessao.id
                        elif not aluno:
                            sessao_atual_id = None

                    print(f"[INFO] Nivel {nivel_atual} | PIN '{pin_atual}' | palavra: '{palavra_atual}' -> '{expected_norm}'")
                    # Protocolo com a ESP32 não muda: só a palavra normalizada volta
                    await websocket.send_text(expected_norm)

                elif comando.startswith("start_audio"):
                    print("[INFO] ESP32 sinalizou início de gravação. Limpando buffer...")
                    audio_buffer.clear()

                elif comando.startswith("stop_audio"):
                    # Formato esperado: "stop_audio <pin>" (pin opcional, mesma lógica acima)
                    partes = comando.split()
                    if len(partes) > 1:
                        pin_atual = partes[1]

                    print(f"[INFO] Gravação encerrada. Amostras binárias: {len(audio_buffer)} bytes")

                    if len(audio_buffer) == 0:
                        await websocket.send_text("incompreensivel")
                        continue

                    with wave.open(caminho_voz, "wb") as wf:
                        wf.setnchannels(1)
                        wf.setsampwidth(SAMPLE_WIDTH)
                        wf.setframerate(SAMPLE_RATE)
                        wf.writeframes(audio_buffer)

                    recognized_norm = transcrever_fala(caminho_voz)

                    if recognized_norm in ("incompreensivel", "erro", ""):
                        to_send = recognized_norm
                        acertou = False
                        lev = -1
                    else:
                        lev = levenshtein(recognized_norm, expected_norm)
                        acertou = (recognized_norm == expected_norm or lev <= 1)
                        to_send = expected_norm if acertou else recognized_norm
                        print(f"[INFO] {'Aceito' if acertou else 'Nao aceito'} (lev={lev})")

                    # Feedback pra ESP32 -- idêntico ao comportamento antigo
                    await websocket.send_text(to_send)

                    # ---------- Persistência (só se o PIN estiver pareado a um aluno) ----------
                    tempo_audio = len(audio_buffer) / (SAMPLE_RATE * SAMPLE_WIDTH)
                    with Session(engine) as db:
                        aluno = _resolver_aluno_pareado(db, pin_atual)
                        if aluno is None or sessao_atual_id is None:
                            print(f"[PAREAMENTO] PIN '{pin_atual}' sem pareamento ativo -- tentativa descartada")
                            await gerenciador.transmitir({"tipo": "pin_incorreto", "pin": pin_atual})
                        else:
                            audio_salvo = _salvar_audio_permanente(caminho_voz, sessao_atual_id)
                            tentativa = Tentativa(
                                sessao_id=sessao_atual_id,
                                palavra_esperada=expected_norm,
                                transcricao_audio=recognized_norm,
                                resultado=acertou,
                                distancia_levenshtein=max(lev, 0),
                                tempo_audio=round(tempo_audio, 2),
                                audio=audio_salvo,
                            )
                            db.add(tentativa)
                            sessao_db = db.get(Sessao, sessao_atual_id)
                            if sessao_db:
                                sessao_db.nivel_atingido = nivel_atual
                                db.add(sessao_db)
                            db.commit()

                            await gerenciador.transmitir({
                                "tipo": "nova_tentativa",
                                "aluno_id": aluno.id,
                                "sessao_id": sessao_atual_id,
                            })

            # ---------- 2. Mensagens binárias (chunks de áudio PCM da ESP32) ----------
            dados_binarios = mensagem.get("bytes")
            if dados_binarios is not None:
                audio_buffer.extend(dados_binarios)

    except WebSocketDisconnect:
        print(f"[NET] Conexão encerrada: {websocket.client}")