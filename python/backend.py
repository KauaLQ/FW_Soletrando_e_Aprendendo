# python3 backend.py 8080
import asyncio
import websockets
import random
import os
import wave
import struct
import sys
import unicodedata
import re
import speech_recognition as sr
from pydub import AudioSegment, effects, silence

# ---------- Configurações de Rede e Áudio ----------
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
HOST = "0.0.0.0"  # Escuta em todas as interfaces de rede locais

# O ideal para processar o áudio do INMP441 capturado pelo ESP32 é 16000Hz (16kHz).
# Se o seu firmware capturar a 8000Hz, ajuste a variável abaixo.
SAMPLE_RATE = 16000  
SAMPLE_WIDTH = 2     # 2 bytes = 16-bit PCM

r = sr.Recognizer()

# ---------- Helpers ----------
def normalize_string(s: str) -> str:
    """
    Normaliza uma string:
    - lowercase, sem acentos, apenas letras a-z.
    - junta sequências de letras separadas (ex: "t e s t e" -> "teste")
    """
    if not s:
        return ""
    s = s.lower().strip()
    s = unicodedata.normalize('NFKD', s)
    s = ''.join(ch for ch in s if not unicodedata.combining(ch))
    s = re.sub(r'[^a-z\s]', '', s)
    
    tokens = s.split()
    merged = []
    i = 0
    while i < len(tokens):
        if len(tokens[i]) == 1:
            j = i
            seq = []
            while j < len(tokens) and len(tokens[j]) == 1:
                seq.append(tokens[j])
                j += 1
            if len(seq) > 1:
                merged.append(''.join(seq))
            else:
                merged.append(seq[0])
            i = j
        else:
            merged.append(tokens[i])
            i += 1
    return ''.join(merged)

def levenshtein(a: str, b: str) -> int:
    """Distância de Levenshtein (edit distance)."""
    if a == b:
        return 0
    n, m = len(a), len(b)
    if n == 0:
        return m
    if m == 0:
        return n
    prev = list(range(m + 1))
    for i in range(1, n + 1):
        cur = [i] + [0] * m
        ai = a[i - 1]
        for j in range(1, m + 1):
            cost = 0 if ai == b[j - 1] else 1
            cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost)
        prev = cur
    return prev[m]

# ---------- Áudio / transcrição ----------
def preprocessar_audio(caminho_entrada):
    """Remove silêncios, normaliza e garante resample para 16000 Hz."""
    audio = AudioSegment.from_file(caminho_entrada, format="wav")
    audio = effects.normalize(audio)
    audio_chunks = silence.split_on_silence(
        audio,
        min_silence_len=500, # Reduzido levemente para soletração, que tem pausas curtas
        silence_thresh=audio.dBFS - 14,
        keep_silence=250
    )
    if not audio_chunks:
        audio = audio.set_frame_rate(16000).set_channels(1)
        caminho_processado = caminho_entrada.replace(".wav", "_proc.wav")
        audio.export(caminho_processado, format="wav")
        return caminho_processado

    audio_final = AudioSegment.silent(duration=200)
    for chunk in audio_chunks:
        audio_final += chunk + AudioSegment.silent(duration=200)
    audio_final = audio_final.set_frame_rate(16000).set_channels(1)
    caminho_processado = caminho_entrada.replace(".wav", "_proc.wav")
    audio_final.export(caminho_processado, format="wav")
    return caminho_processado

def transcrever_fala(caminho_arquivo):
    """Faz a transcrição com Google Speech e retorna versão normalizada."""
    caminho_processado = preprocessar_audio(caminho_arquivo)
    with sr.AudioFile(caminho_processado) as arq_audio:
        try:
            r.adjust_for_ambient_noise(arq_audio, duration=0.3)
            audio = r.record(arq_audio)
            texto = r.recognize_google(audio, language='pt-BR')
            print("[ASR] Original:", texto)
            texto_norm = normalize_string(texto)
            print("[ASR] Normalizado:", texto_norm)
            return texto_norm
        except sr.UnknownValueError:
            print("[ASR] Incompreensível")
            return "incompreensivel"
        except sr.RequestError as e:
            print(f"[ASR] Erro serviço: {e}")
            return "erro"

# ---------- Arquivos de palavras ----------
def carregar_palavras(nivel):
    """Carrega lista de palavras do nível."""
    base_dir = os.path.dirname(os.path.abspath(__file__))
    letras = nivel + 4
    caminho = os.path.join(base_dir, "..", "database", f"palavras_{letras}.txt")
    if not os.path.exists(caminho):
        print(f"[WARN] {caminho} não encontrado. Utilizando palavras_5.txt")
        caminho = os.path.join(base_dir, "..", "database", "palavras_5.txt")
    with open(caminho, "r", encoding="utf-8") as f:
        return [linha.strip() for linha in f if linha.strip()]

# ---------- Manipulação do WebSocket ----------
async def handle_connection(websocket):
    print(f"[NET] Cliente conectado: {websocket.remote_address}")
    caminho_voz = os.path.join(os.path.dirname(os.path.abspath(__file__)), "voz.wav")
    
    try:
        expected_norm = ""
        audio_buffer = bytearray()
        
        async for message in websocket:
            # 1. Trata mensagens de Texto (comandos de controle)
            if isinstance(message, str):
                comando = message.strip()
                
                if comando.startswith("pedir_palavra"):
                    partes = comando.split()
                    nivel = 1
                    if len(partes) > 1:
                        try:
                            nivel = int(partes[1])
                        except ValueError:
                            pass

                    palavras = carregar_palavras(nivel)
                    palavra = random.choice(palavras)
                    expected_norm = normalize_string(palavra)
                    
                    print(f"[INFO] Nível {nivel} | palavra escolhida: '{palavra}' -> '{expected_norm}'")
                    # Envia a palavra escolhida de volta para o ESP32
                    await websocket.send(expected_norm)
                
                elif comando == "start_audio":
                    print("[INFO] ESP32 sinalizou início de gravação. Limpando buffer...")
                    audio_buffer.clear()
                    
                elif comando == "stop_audio":
                    print(f"[INFO] Gravação encerrada pelo ESP32. Amostras binárias: {len(audio_buffer)} bytes")
                    
                    if len(audio_buffer) == 0:
                        await websocket.send("incompreensivel")
                        continue
                    
                    # Salva o buffer em formato .wav PCM Linear 16-bit
                    with wave.open(caminho_voz, "wb") as wf:
                        wf.setnchannels(1)
                        wf.setsampwidth(SAMPLE_WIDTH)
                        wf.setframerate(SAMPLE_RATE)
                        wf.writeframes(audio_buffer)
                    
                    # Roda o processamento síncrono do ASR em uma thread secundária para não travar o loop de eventos assíncronos
                    loop = asyncio.get_event_loop()
                    recognized_norm = await loop.run_in_executor(None, transcrever_fala, caminho_voz)
                    
                    # Lógica de validação (Levenshtein)
                    if recognized_norm in ("incompreensivel", "erro", ""):
                        to_send = recognized_norm
                    else:
                        lev = levenshtein(recognized_norm, expected_norm)
                        if recognized_norm == expected_norm or lev <= 1:
                            print(f"[INFO] Aceito (lev={lev}). Enviando palavra correta: '{expected_norm}'")
                            to_send = expected_norm
                        else:
                            print(f"[INFO] Não aceito (lev={lev}). Enviando transcrição: '{recognized_norm}'")
                            to_send = recognized_norm
                    
                    # Envia o feedback de acerto/erro de volta ao ESP32
                    await websocket.send(to_send)

            # 2. Trata mensagens Binárias (Chunks de Áudio PCM do ESP32)
            elif isinstance(message, bytes):
                # Se o seu ESP32 estiver lendo em 32-bit PCM (nativo do INMP441) e você quiser converter
                # no Python para 16-bit, descomente as linhas abaixo. Caso contrário, se o ESP32 já estiver
                # enviando em PCM 16-bit (2 bytes por amostra), o append direto é o ideal.
                
                # --- Exemplo de conversão se o ESP32 enviar 32-bit PCM ---
                # for i in range(0, len(message), 4):
                #     sample_32 = int.from_bytes(message[i:i+4], byteorder='little', signed=True)
                #     # Reduz amplitude de 32-bit para 16-bit (shift de 16 bits)
                #     sample_16 = sample_32 >> 16
                #     audio_buffer.extend(struct.pack('<h', sample_16))
                
                # --- Envio padrão em 16-bit ---
                audio_buffer.extend(message)

    except websockets.exceptions.ConnectionClosed as e:
        print(f"[NET] Conexão encerrada com {websocket.remote_address}: {e}")
    except Exception as e:
        print(f"[ERRO] Ocorreu uma exceção: {e}")

# ---------- Main ----------
async def main():
    print(f"[INFO] Iniciando Servidor WebSocket em ws://{HOST}:{PORT} ...")
    async with websockets.serve(handle_connection, HOST, PORT):
        await asyncio.Future()  # Mantém o servidor rodando infinitamente

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[INFO] Servidor finalizado pelo usuário.")