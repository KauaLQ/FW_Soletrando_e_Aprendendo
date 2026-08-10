import requisitar from "./client";

export function listarTurmas(token) {
  return requisitar("/turmas", { token });
}

export function criarTurma(token, { nome, ano_letivo }) {
  return requisitar("/turmas", { method: "POST", token, body: { nome, ano_letivo } });
}

export function listarAlunos(token, turmaId) {
  return requisitar(`/turmas/${turmaId}/alunos`, { token });
}

export function criarAluno(token, turmaId, { nome }) {
  return requisitar(`/turmas/${turmaId}/alunos`, { method: "POST", token, body: { nome } });
}