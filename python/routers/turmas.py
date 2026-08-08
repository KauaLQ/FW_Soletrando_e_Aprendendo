from typing import List
from fastapi import APIRouter, Depends, HTTPException
from sqlmodel import Session, select, func
from database import obter_sessao
from models import Professor, Turma, Aluno, Sessao, Tentativa
from schemas import TurmaCriar, TurmaResumo, AlunoCriar, AlunoResumo
from auth import obter_professor_atual

router = APIRouter(prefix="/turmas", tags=["Turmas"])

def _obter_turma_do_professor(turma_id: int, sessao: Session, professor: Professor) -> Turma:
    turma = sessao.get(Turma, turma_id)
    if not turma or turma.professor_id != professor.id:
        raise HTTPException(status_code=404, detail="Turma não encontrada")
    return turma

@router.get("", response_model=List[TurmaResumo])
def listar_turmas(
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    turmas = sessao.exec(select(Turma).where(Turma.professor_id == professor.id)).all()
    resultado = []
    for turma in turmas:
        total_alunos = sessao.exec(
            select(func.count(Aluno.id)).where(Aluno.turma_id == turma.id)
        ).one()
        total_sessoes = sessao.exec(
            select(func.count(Sessao.id))
            .join(Aluno, Aluno.id == Sessao.aluno_id)
            .where(Aluno.turma_id == turma.id)
        ).one()
        total_palavras = sessao.exec(
            select(func.count(Tentativa.id))
            .join(Sessao, Sessao.id == Tentativa.sessao_id)
            .join(Aluno, Aluno.id == Sessao.aluno_id)
            .where(Aluno.turma_id == turma.id)
        ).one()
        resultado.append(TurmaResumo(
            id=turma.id,
            nome=turma.nome,
            ano_letivo=turma.ano_letivo,
            total_alunos=total_alunos,
            total_sessoes=total_sessoes,
            total_palavras_gravadas=total_palavras,
        ))
    return resultado

@router.post("", response_model=TurmaResumo)
def criar_turma(
    dados: TurmaCriar,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    turma = Turma(nome=dados.nome, ano_letivo=dados.ano_letivo, professor_id=professor.id)
    sessao.add(turma)
    sessao.commit()
    sessao.refresh(turma)
    return TurmaResumo(
        id=turma.id, nome=turma.nome, ano_letivo=turma.ano_letivo,
        total_alunos=0, total_sessoes=0, total_palavras_gravadas=0,
    )

@router.get("/{turma_id}/alunos", response_model=List[AlunoResumo])
def listar_alunos(
    turma_id: int,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    _obter_turma_do_professor(turma_id, sessao, professor)
    alunos = sessao.exec(select(Aluno).where(Aluno.turma_id == turma_id)).all()
    return [AlunoResumo(id=a.id, nome=a.nome, turma_id=a.turma_id) for a in alunos]

@router.post("/{turma_id}/alunos", response_model=AlunoResumo)
def criar_aluno(
    turma_id: int,
    dados: AlunoCriar,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    _obter_turma_do_professor(turma_id, sessao, professor)
    aluno = Aluno(nome=dados.nome, turma_id=turma_id)
    sessao.add(aluno)
    sessao.commit()
    sessao.refresh(aluno)
    return AlunoResumo(id=aluno.id, nome=aluno.nome, turma_id=aluno.turma_id)