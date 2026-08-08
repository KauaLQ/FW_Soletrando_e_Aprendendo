import os
import bcrypt
from datetime import datetime, timedelta
from fastapi import Depends, HTTPException, status
from fastapi.security import OAuth2PasswordBearer
from jose import JWTError, jwt
from passlib.context import CryptContext
from sqlmodel import Session
from database import obter_sessao
from models import Professor

# Em produção, defina JWT_SECRET no seu .env (mesma pasta do DB_URL)
SECRET_KEY = os.getenv("JWT_SECRET", "troque-essa-chave-em-producao")
ALGORITHM = "HS256"
EXPIRA_MINUTOS = 60 * 12  # token válido por 12h

pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/auth/login")

def hash_senha(senha: str) -> str:
    # Converte a string para bytes, gera o salt e faz o hash
    pwd_bytes = senha.encode('utf-8')
    salt = bcrypt.gensalt()
    hashed = bcrypt.hashpw(pwd_bytes, salt)
    return hashed.decode('utf-8')

def verificar_senha(senha: str, senha_hash: str) -> bool:
    # Verifica se a senha informada bate com o hash salvo
    return bcrypt.checkpw(senha.encode('utf-8'), senha_hash.encode('utf-8'))

def criar_token(professor_id: int) -> str:
    expira = datetime.utcnow() + timedelta(minutes=EXPIRA_MINUTOS)
    payload = {"sub": str(professor_id), "exp": expira}
    return jwt.encode(payload, SECRET_KEY, algorithm=ALGORITHM)

def obter_professor_atual(
    token: str = Depends(oauth2_scheme),
    sessao: Session = Depends(obter_sessao),
) -> Professor:
    """Dependency usada em toda rota protegida: valida o Bearer token e devolve o Professor."""
    excecao = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Credenciais inválidas",
        headers={"WWW-Authenticate": "Bearer"},
    )
    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
        professor_id = int(payload.get("sub"))
    except (JWTError, TypeError, ValueError):
        raise excecao

    professor = sessao.get(Professor, professor_id)
    if not professor:
        raise excecao
    return professor