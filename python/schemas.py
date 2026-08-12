from datetime import datetime
from typing import List, Optional, Literal
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

class PareamentoStatus(BaseModel):
    pin: Optional[str] = None

# ---------- Relatório de IA pedagógica ----------
class ExemploErro(BaseModel):
    palavra_esperada: str
    transcrito: str

class PadraoIdentificado(BaseModel):
    tipo: Literal["fonetico", "ortografico"]
    descricao: str
    exemplos: List[ExemploErro]
    frequencia: int

class PalavraRecomendada(BaseModel):
    palavra: str
    motivo: str

class MetodologiaSugerida(BaseModel):
    nome: str
    descricao: str
    como_aplicar: str

class ResumoDesempenho(BaseModel):
    total_tentativas: int
    total_acertos: int
    total_erros: int
    taxa_acerto: float

class RelatorioIA(BaseModel):
    resumo_desempenho: ResumoDesempenho
    padroes_identificados: List[PadraoIdentificado]
    palavras_recomendadas: List[PalavraRecomendada]
    metodologias_sugeridas: List[MetodologiaSugerida]
    observacoes_gerais: str

class RelatorioIAResposta(BaseModel):
    relatorio: RelatorioIA
    gerado_em: datetime
    do_cache: bool