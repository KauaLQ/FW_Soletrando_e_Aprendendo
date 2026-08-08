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
# Pode ser sobrescrita por variável de ambiente (recomendado em produção).
# Formato: postgresql://usuario:senha@host:porta/nome_do_banco
DB_URL = os.getenv("DB_URL")

# Validação para evitar erros silenciosos
if not DB_URL:
    raise ValueError(f"A variável DB_URL não foi encontrada. Verifique se o arquivo existe em: {ENV_PATH}")

# echo=False evita poluir o console com o SQL gerado; troque para True se
# precisar debugar as queries.
engine = create_engine(DB_URL, echo=False)


def criar_tabelas():
    """Cria todas as tabelas que ainda não existem no banco."""
    SQLModel.metadata.create_all(engine)

def obter_sessao() -> Session:
    """Retorna uma nova sessão do banco (usar com 'with')."""
    return Session(engine)

if __name__ == "__main__":
    # python3 database.py -> cria as tabelas manualmente
    print(f"[DB] Conectando em: {DB_URL}")
    criar_tabelas()
    print("[DB] Tabelas criadas/verificadas com sucesso.")
