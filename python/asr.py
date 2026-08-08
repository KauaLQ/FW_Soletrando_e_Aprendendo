import os
import unicodedata
import re
import speech_recognition as sr
from pydub import AudioSegment, effects, silence

r = sr.Recognizer()

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

def preprocessar_audio(caminho_entrada):
    """Remove silêncios, normaliza e garante resample para 16000 Hz."""
    audio = AudioSegment.from_file(caminho_entrada, format="wav")
    audio = effects.normalize(audio)
    audio_chunks = silence.split_on_silence(
        audio,
        min_silence_len=500,
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