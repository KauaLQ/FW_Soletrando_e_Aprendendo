from typing import List
from fastapi import APIRouter, Depends, HTTPException
from sqlmodel import Session, select
from database import obter_sessao
from models import Professor, Aluno, Turma, Sessao, Tentativa, Pareamento
from schemas import AlunoDetalhe, SessaoResposta, TentativaResposta, PareamentoCriar, PareamentoStatus
from auth import obter_professor_atual
from dashboard_ws import gerenciador

router = APIRouter(prefix="/alunos", tags=["Alunos"])

def _obter_aluno_do_professor(aluno_id: int, sessao: Session, professor: Professor) -> Aluno:
    aluno = sessao.get(Aluno, aluno_id)
    if not aluno:
        raise HTTPException(status_code=404, detail="Aluno não encontrado")
    turma = sessao.get(Turma, aluno.turma_id)
    if not turma or turma.professor_id != professor.id:
        raise HTTPException(status_code=404, detail="Aluno não encontrado")
    return aluno

@router.get("/{aluno_id}", response_model=AlunoDetalhe)
def obter_aluno(
    aluno_id: int,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    aluno = _obter_aluno_do_professor(aluno_id, sessao, professor)
    sessoes = sessao.exec(
        select(Sessao).where(Sessao.aluno_id == aluno_id).order_by(Sessao.data_inicio.desc())
    ).all()

    sessoes_resposta = []
    total_acertos = total_erros = 0
    soma_tempo = 0.0
    total_tentativas = 0

    for s in sessoes:
        tentativas = sessao.exec(select(Tentativa).where(Tentativa.sessao_id == s.id)).all()
        for t in tentativas:
            total_tentativas += 1
            soma_tempo += t.tempo_audio
            if t.resultado:
                total_acertos += 1
            else:
                total_erros += 1
        sessoes_resposta.append(SessaoResposta(
            id=s.id,
            nivel_atingido=s.nivel_atingido,
            data_inicio=s.data_inicio,
            tentativas=[TentativaResposta(**t.dict()) for t in tentativas],
        ))

    tempo_medio = (soma_tempo / total_tentativas) if total_tentativas else 0.0

    return AlunoDetalhe(
        id=aluno.id,
        nome=aluno.nome,
        turma_id=aluno.turma_id,
        total_acertos=total_acertos,
        total_erros=total_erros,
        tempo_medio_soletracao=round(tempo_medio, 2),
        sessoes=sessoes_resposta,
    )

# ---------- Pareamento ----------
@router.get("/{aluno_id}/parear", response_model=PareamentoStatus)
def obter_pareamento_ativo(
    aluno_id: int,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    """Consultado ao abrir a página do aluno, pra restaurar o PIN ativo mesmo
    depois de um F5 (o pareamento é persistido no banco, não no navegador)."""
    _obter_aluno_do_professor(aluno_id, sessao, professor)
    ativo = sessao.exec(
        select(Pareamento).where(Pareamento.aluno_id == aluno_id, Pareamento.ativo == True)  # noqa: E712
    ).first()
    return PareamentoStatus(pin=ativo.pin if ativo else None)

@router.post("/{aluno_id}/parear")
async def parear_aluno(
    aluno_id: int,
    dados: PareamentoCriar,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    _obter_aluno_do_professor(aluno_id, sessao, professor)

    antigos = sessao.exec(
        select(Pareamento).where(Pareamento.pin == dados.pin, Pareamento.ativo == True)  # noqa: E712
    ).all()
    for p in antigos:
        p.ativo = False
        sessao.add(p)

    novo = Pareamento(pin=dados.pin, aluno_id=aluno_id, ativo=True)
    sessao.add(novo)
    sessao.commit()

    await gerenciador.transmitir({
        "tipo": "pareamento_alterado", "aluno_id": aluno_id, "pin": dados.pin,
    })
    return {"ok": True, "pin": dados.pin, "aluno_id": aluno_id}

@router.delete("/{aluno_id}/parear")
async def desparear_aluno(
    aluno_id: int,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    _obter_aluno_do_professor(aluno_id, sessao, professor)
    ativos = sessao.exec(
        select(Pareamento).where(Pareamento.aluno_id == aluno_id, Pareamento.ativo == True)  # noqa: E712
    ).all()
    for p in ativos:
        p.ativo = False
        sessao.add(p)
    sessao.commit()

    await gerenciador.transmitir({
        "tipo": "pareamento_alterado", "aluno_id": aluno_id, "pin": None,
    })
    return {"ok": True}