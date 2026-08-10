from fastapi import APIRouter, Depends, HTTPException
from sqlmodel import Session, select
from database import obter_sessao
from models import Professor
from schemas import ProfessorCriar, ProfessorLogin, TokenResposta, ProfessorResposta
from auth import hash_senha, verificar_senha, criar_token

router = APIRouter(prefix="/auth", tags=["Autenticação"])

@router.post("/registrar", response_model=TokenResposta)
def registrar(dados: ProfessorCriar, sessao: Session = Depends(obter_sessao)):
    existente = sessao.exec(select(Professor).where(Professor.email == dados.email)).first()
    if existente:
        raise HTTPException(status_code=400, detail="Já existe um professor com esse email")

    professor = Professor(nome=dados.nome, email=dados.email, senha_hash=hash_senha(dados.senha))
    sessao.add(professor)
    sessao.commit()
    sessao.refresh(professor)
    return TokenResposta(
        access_token=criar_token(professor.id),
        professor=ProfessorResposta(id=professor.id, nome=professor.nome, email=professor.email),
    )

@router.post("/login", response_model=TokenResposta)
def login(dados: ProfessorLogin, sessao: Session = Depends(obter_sessao)):
    professor = sessao.exec(select(Professor).where(Professor.email == dados.email)).first()
    if not professor or not verificar_senha(dados.senha, professor.senha_hash):
        raise HTTPException(status_code=401, detail="Email ou senha inválidos")
    return TokenResposta(
        access_token=criar_token(professor.id),
        professor=ProfessorResposta(id=professor.id, nome=professor.nome, email=professor.email),
    )
