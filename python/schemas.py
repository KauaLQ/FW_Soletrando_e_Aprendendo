from datetime import datetime
from typing import List, Optional
from pydantic import BaseModel, EmailStr, Field

# ---------- Autenticação ----------
class ProfessorCriar(BaseModel):
    nome: str
    email: EmailStr
    senha: str = Field(min_length=6)

class ProfessorLogin(BaseModel):
    email: EmailStr
    senha: str

class ProfessorResposta(BaseModel):
    id: int
    nome: str
    email: EmailStr

class TokenResposta(BaseModel):
    access_token: str
    token_type: str = "bearer"
    professor: ProfessorResposta

# ---------- Turmas ----------
class TurmaCriar(BaseModel):
    nome: str
    ano_letivo: int

class TurmaResumo(BaseModel):
    id: int
    nome: str
    ano_letivo: int
    total_alunos: int
    total_sessoes: int
    total_palavras_gravadas: int

# ---------- Alunos ----------
class AlunoCriar(BaseModel):
    nome: str

class AlunoResumo(BaseModel):
    id: int
    nome: str
    turma_id: int

class TentativaResposta(BaseModel):
    id: int
    palavra_esperada: str
    transcricao_audio: str
    resultado: bool
    distancia_levenshtein: int
    tempo_audio: float
    audio: Optional[str] = None
    criado_em: datetime

class SessaoResposta(BaseModel):
    id: int
    nivel_atingido: int
    data_inicio: datetime
    tentativas: List[TentativaResposta] = []

class AlunoDetalhe(BaseModel):
    id: int
    nome: str
    turma_id: int
    total_acertos: int
    total_erros: int
    tempo_medio_soletracao: float
    sessoes: List[SessaoResposta] = []

# ---------- Pareamento ----------
class PareamentoCriar(BaseModel):
    pin: str = Field(min_length=4, max_length=4)
