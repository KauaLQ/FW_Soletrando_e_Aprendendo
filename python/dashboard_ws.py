import json
from fastapi import APIRouter, WebSocket, WebSocketDisconnect, Query
from jose import jwt, JWTError
from auth import SECRET_KEY, ALGORITHM

router = APIRouter()

class GerenciadorConexoes:
    """Mantém a lista de dashboards (abas do professor) conectados e permite
    transmitir um evento para todos eles ao mesmo tempo."""

    def __init__(self):
        self.conexoes: list[WebSocket] = []

    async def conectar(self, websocket: WebSocket):
        await websocket.accept()
        self.conexoes.append(websocket)

    def desconectar(self, websocket: WebSocket):
        if websocket in self.conexoes:
            self.conexoes.remove(websocket)

    async def transmitir(self, evento: dict):
        """Envia o evento (serializável em JSON) para todos os dashboards
        conectados. Conexões mortas são descartadas silenciosamente."""
        mensagem = json.dumps(evento)
        mortas = []
        for websocket in self.conexoes:
            try:
                await websocket.send_text(mensagem)
            except Exception:
                mortas.append(websocket)
        for websocket in mortas:
            self.desconectar(websocket)

# Instância única, compartilhada por quem precisar transmitir eventos
# (as rotas de pareamento e o router da ESP32 importam este objeto).
gerenciador = GerenciadorConexoes()

@router.websocket("/ws/dashboard")
async def websocket_dashboard(websocket: WebSocket, token: str = Query(...)):
    try:
        jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
    except JWTError:
        await websocket.close(code=1008)  # 1008 = policy violation
        return

    await gerenciador.conectar(websocket)
    try:
        while True:
            # Não esperamos nada do cliente só mantemos a conexão viva
            # até o browser fechar a aba ou navegar pra outro lugar.
            await websocket.receive_text()
    except WebSocketDisconnect:
        gerenciador.desconectar(websocket)