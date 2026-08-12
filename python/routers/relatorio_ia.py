from datetime import datetime, timezone
from typing import Optional
from fastapi import APIRouter, Depends, HTTPException
from sqlmodel import Session, select
from database import obter_sessao
from models import Professor, Aluno, Turma, Sessao, Tentativa
from schemas import RelatorioIA, RelatorioIAResposta
from auth import obter_professor_atual
from ia_pedagogica import gerar_relatorio
from routers.alunos import _obter_aluno_do_professor
from routers.turmas import _obter_turma_do_professor

router = APIRouter(tags=["Relatório IA"])

MINIMO_TENTATIVAS = 5

def _agora_utc():
    return datetime.now(timezone.utc)

def _tentativas_para_dict(tentativas: list[Tentativa]) -> list[dict]:
    return [
        {
            "palavra_esperada": t.palavra_esperada,
            "transcricao_audio": t.transcricao_audio,
            "resultado": t.resultado,
        }
        for t in tentativas
    ]

def _chamar_ia_com_seguranca(tentativas, nome, tipo) -> RelatorioIA:
    """A cota gratuita do Gemini pode falhar por limite de taxa
    convertemos qualquer erro do SDK numa mensagem amigável pro professor."""
    try:
        return gerar_relatorio(tentativas, nome, tipo)
    except Exception:
        raise HTTPException(
            status_code=503,
            detail="Não foi possível gerar a análise agora (serviço de IA indisponível ou "
                   "limite de uso atingido). Tente novamente em alguns instantes.",
        )

# ---------- Aluno ----------
def _tentativas_aluno(sessao: Session, aluno_id: int) -> list[Tentativa]:
    sessoes_ids = sessao.exec(select(Sessao.id).where(Sessao.aluno_id == aluno_id)).all()
    if not sessoes_ids:
        return []
    return sessao.exec(select(Tentativa).where(Tentativa.sessao_id.in_(sessoes_ids))).all()

@router.get("/alunos/{aluno_id}/relatorio-ia", response_model=Optional[RelatorioIAResposta])
def obter_relatorio_aluno(
    aluno_id: int,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    """Consultado ao abrir a página, pra restaurar um relatório já gerado
    antes sem precisar chamar a IA de novo."""
    aluno = _obter_aluno_do_professor(aluno_id, sessao, professor)
    if not aluno.relatorio_ia:
        return None
    return RelatorioIAResposta(
        relatorio=RelatorioIA.model_validate_json(aluno.relatorio_ia),
        gerado_em=aluno.relatorio_ia_gerado_em,
        do_cache=True,
    )

@router.post("/alunos/{aluno_id}/relatorio-ia", response_model=RelatorioIAResposta)
def gerar_relatorio_aluno(
    aluno_id: int,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    aluno = _obter_aluno_do_professor(aluno_id, sessao, professor)
    tentativas = _tentativas_aluno(sessao, aluno_id)

    if len(tentativas) < MINIMO_TENTATIVAS:
        raise HTTPException(
            status_code=400,
            detail=f"São necessárias pelo menos {MINIMO_TENTATIVAS} tentativas registradas para "
                   f"gerar uma análise confiável (atualmente: {len(tentativas)}).",
        )

    total_atual = len(tentativas)

    # Só chama a IA se houver tentativas novas desde a última geração salva.
    if aluno.relatorio_ia and aluno.relatorio_ia_total_tentativas == total_atual:
        return RelatorioIAResposta(
            relatorio=RelatorioIA.model_validate_json(aluno.relatorio_ia),
            gerado_em=aluno.relatorio_ia_gerado_em,
            do_cache=True,
        )

    relatorio = _chamar_ia_com_seguranca(_tentativas_para_dict(tentativas), aluno.nome, "aluno individual")

    aluno.relatorio_ia = relatorio.model_dump_json()
    aluno.relatorio_ia_gerado_em = _agora_utc()
    aluno.relatorio_ia_total_tentativas = total_atual
    sessao.add(aluno)
    sessao.commit()

    return RelatorioIAResposta(relatorio=relatorio, gerado_em=aluno.relatorio_ia_gerado_em, do_cache=False)

# ---------- Turma ----------
def _tentativas_turma(sessao: Session, turma_id: int) -> list[Tentativa]:
    alunos_ids = sessao.exec(select(Aluno.id).where(Aluno.turma_id == turma_id)).all()
    if not alunos_ids:
        return []
    sessoes_ids = sessao.exec(select(Sessao.id).where(Sessao.aluno_id.in_(alunos_ids))).all()
    if not sessoes_ids:
        return []
    return sessao.exec(select(Tentativa).where(Tentativa.sessao_id.in_(sessoes_ids))).all()

@router.get("/turmas/{turma_id}/relatorio-ia", response_model=Optional[RelatorioIAResposta])
def obter_relatorio_turma(
    turma_id: int,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    turma = _obter_turma_do_professor(turma_id, sessao, professor)
    if not turma.relatorio_ia:
        return None
    return RelatorioIAResposta(
        relatorio=RelatorioIA.model_validate_json(turma.relatorio_ia),
        gerado_em=turma.relatorio_ia_gerado_em,
        do_cache=True,
    )

@router.post("/turmas/{turma_id}/relatorio-ia", response_model=RelatorioIAResposta)
def gerar_relatorio_turma(
    turma_id: int,
    sessao: Session = Depends(obter_sessao),
    professor: Professor = Depends(obter_professor_atual),
):
    turma = _obter_turma_do_professor(turma_id, sessao, professor)
    tentativas = _tentativas_turma(sessao, turma_id)

    if len(tentativas) < MINIMO_TENTATIVAS:
        raise HTTPException(
            status_code=400,
            detail=f"São necessárias pelo menos {MINIMO_TENTATIVAS} tentativas registradas na turma "
                   f"para gerar uma análise confiável (atualmente: {len(tentativas)}).",
        )

    total_atual = len(tentativas)

    if turma.relatorio_ia and turma.relatorio_ia_total_tentativas == total_atual:
        return RelatorioIAResposta(
            relatorio=RelatorioIA.model_validate_json(turma.relatorio_ia),
            gerado_em=turma.relatorio_ia_gerado_em,
            do_cache=True,
        )

    relatorio = _chamar_ia_com_seguranca(_tentativas_para_dict(tentativas), turma.nome, "turma inteira")

    turma.relatorio_ia = relatorio.model_dump_json()
    turma.relatorio_ia_gerado_em = _agora_utc()
    turma.relatorio_ia_total_tentativas = total_atual
    sessao.add(turma)
    sessao.commit()

    return RelatorioIAResposta(relatorio=relatorio, gerado_em=turma.relatorio_ia_gerado_em, do_cache=False)