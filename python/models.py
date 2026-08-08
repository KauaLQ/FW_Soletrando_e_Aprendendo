from datetime import datetime
from typing import List, Optional
from sqlmodel import SQLModel, Field, Relationship

# ---------- Professores ----------
class Professor(SQLModel, table=True):
    __tablename__ = "professores"

    id: Optional[int] = Field(default=None, primary_key=True)
    nome: str
    email: str = Field(unique=True, index=True)
    senha_hash: str
    criado_em: datetime = Field(default_factory=datetime.utcnow)

    turmas: List["Turma"] = Relationship(back_populates="professor")

# ---------- Turmas ----------
class Turma(SQLModel, table=True):
    __tablename__ = "turmas"

    id: Optional[int] = Field(default=None, primary_key=True)
    nome: str
    ano_letivo: int
    professor_id: int = Field(foreign_key="professores.id")

    professor: Optional[Professor] = Relationship(back_populates="turmas")
    alunos: List["Aluno"] = Relationship(back_populates="turma")

# ---------- Alunos ----------
class Aluno(SQLModel, table=True):
    __tablename__ = "alunos"

    id: Optional[int] = Field(default=None, primary_key=True)
    nome: str
    turma_id: int = Field(foreign_key="turmas.id")

    turma: Optional[Turma] = Relationship(back_populates="alunos")
    sessoes: List["Sessao"] = Relationship(back_populates="aluno")

# ---------- Sessoes ----------
class Sessao(SQLModel, table=True):
    __tablename__ = "sessoes"

    id: Optional[int] = Field(default=None, primary_key=True)
    aluno_id: int = Field(foreign_key="alunos.id")
    data_inicio: datetime = Field(default_factory=datetime.utcnow)
    nivel_atingido: int = Field(default=1)
    # PIN de 4 dígitos usado no pareamento com o ESP32 (mesmo conceito do
    # menuObterPinPareamento() do firmware)
    pin_pareamento: str = Field(index=True)

    aluno: Optional[Aluno] = Relationship(back_populates="sessoes")
    tentativas: List["Tentativa"] = Relationship(back_populates="sessao")

# ---------- Tentativas ----------
class Tentativa(SQLModel, table=True):
    __tablename__ = "tentativas"

    id: Optional[int] = Field(default=None, primary_key=True)
    sessao_id: int = Field(foreign_key="sessoes.id")
    palavra_esperada: str
    transcricao_audio: str
    resultado: bool
    distancia_levenshtein: int
    tempo_audio: float  # duração do áudio em segundos
    criado_em: datetime = Field(default_factory=datetime.utcnow)
    # Caminho/nome do arquivo .wav salvo em disco (ex.: "audios/2024-06-01_123.wav").
    # Evitar guardar o binário direto no Postgres deixa o banco mais leve;
    # se quiser guardar o áudio em si, troque para bytes (bytea).
    audio: Optional[str] = Field(default=None)

    sessao: Optional[Sessao] = Relationship(back_populates="tentativas")