import requisitar from "./client";

export function obterRelatorioAluno(token, alunoId) {
  return requisitar(`/alunos/${alunoId}/relatorio-ia`, { token });
}

export function gerarRelatorioAluno(token, alunoId) {
  return requisitar(`/alunos/${alunoId}/relatorio-ia`, { method: "POST", token });
}

export function obterRelatorioTurma(token, turmaId) {
  return requisitar(`/turmas/${turmaId}/relatorio-ia`, { token });
}

export function gerarRelatorioTurma(token, turmaId) {
  return requisitar(`/turmas/${turmaId}/relatorio-ia`, { method: "POST", token });
}