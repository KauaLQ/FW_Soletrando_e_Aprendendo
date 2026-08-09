import requisitar from "./client";

export function login(email, senha) {
  return requisitar("/auth/login", { method: "POST", body: { email, senha } });
}

export function registrar(nome, email, senha) {
  return requisitar("/auth/registrar", { method: "POST", body: { nome, email, senha } });
}
