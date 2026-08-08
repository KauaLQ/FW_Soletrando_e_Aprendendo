import os
from pathlib import Path

# Raiz do projeto (um nível acima da pasta python/)
BASE_DIR = Path(__file__).resolve().parent.parent

# Pasta onde os áudios de cada tentativa ficam salvos permanentemente,
# pra poderem ser servidos pro frontend (botão "ouvir áudio").
AUDIO_DIR = BASE_DIR / "audios"
AUDIO_DIR.mkdir(exist_ok=True)
