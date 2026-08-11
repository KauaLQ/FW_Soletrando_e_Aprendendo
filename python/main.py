# python3 -m uvicorn main:app --host 0.0.0.0 --port 8080 --reload
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from database import criar_tabelas
from config import AUDIO_DIR
from routers import auth_router, turmas, alunos
from websocket_esp32 import router as esp32_router
from dashboard_ws import router as dashboard_router

app = FastAPI(title="Soletrando API")

# Libere aqui só o domínio do seu frontend React quando for pra produção
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---------- Rotas REST (professor logado, via JWT) ----------
app.include_router(auth_router.router)
app.include_router(turmas.router)
app.include_router(alunos.router)

# ---------- WebSocket da ESP32 (mesma porta, path novo: /ws/esp32) ----------
app.include_router(esp32_router)
app.include_router(dashboard_router) # WebSocket do dashboard

# ---------- Áudios das tentativas, servidos estaticamente pro botão "ouvir áudio" ----------
app.mount("/audios", StaticFiles(directory=str(AUDIO_DIR)), name="audios")

@app.on_event("startup")
def ao_iniciar():
    criar_tabelas()

@app.get("/")
def status():
    return {"status": "ok"}