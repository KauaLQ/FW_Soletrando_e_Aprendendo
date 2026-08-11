import requisitar from "./client";

export function obterAluno(token, alunoId) {
  return requisitar(`/alunos/${alunoId}`, { token });
}

export function obterPareamentoAtivo(token, alunoId) {
  return requisitar(`/alunos/${alunoId}/parear`, { token });
}

export function parearAluno(token, alunoId, pin) {
  return requisitar(`/alunos/${alunoId}/parear`, { method: "POST", token, body: { pin } });
}

export function despararAluno(token, alunoId) {
  return requisitar(`/alunos/${alunoId}/parear`, { method: "DELETE", token });
}