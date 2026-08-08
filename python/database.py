import os
from pathlib import Path
from dotenv import load_dotenv
from sqlmodel import SQLModel, Session, create_engine
# import necessário para o SQLModel "enxergar" todas as tabelas antes de criar
import models  # noqa: F401

BASE_DIR = Path(__file__).resolve().parent.parent
ENV_PATH = BASE_DIR / ".env"

# Carrega as variáveis especificando o caminho correto
load_dotenv(dotenv_path=ENV_PATH)

# ---------- String de conexão ----------
DB_URL = os.getenv("DB_URL")

if not DB_URL:
    raise ValueError(f"A variável DB_URL não foi encontrada. Verifique se o arquivo existe em: {ENV_PATH}")

engine = create_engine(DB_URL, echo=False)

def criar_tabelas():
    """Cria todas as tabelas que ainda não existem no banco."""
    SQLModel.metadata.create_all(engine)

def obter_sessao():
    """
    Dependency do FastAPI (Depends(obter_sessao)): entrega uma Session e
    garante o fechamento no final da requisição, mesmo se der exceção.
    """
    with Session(engine) as sessao:
        yield sessao

if __name__ == "__main__":
    # python3 database.py -> cria as tabelas manualmente
    print(f"[DB] Conectando em: {DB_URL}")
    criar_tabelas()
    print("[DB] Tabelas criadas/verificadas com sucesso.")