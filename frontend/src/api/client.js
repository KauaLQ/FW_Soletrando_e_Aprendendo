const API_URL = import.meta.env.VITE_API_URL || "http://localhost:8080";

/**
 * Wrapper único pra todas as chamadas à API. Centraliza:
 * - montagem da URL a partir de VITE_API_URL
 * - envio do token (quando informado)
 * - leitura do corpo de erro do FastAPI ({ detail: "..." }) numa mensagem amigável
 */
export default async function requisitar(caminho, { method = "GET", body, token } = {}) {
  const headers = { "Content-Type": "application/json" };
  if (token) headers.Authorization = `Bearer ${token}`;

  const resposta = await fetch(`${API_URL}${caminho}`, {
    method,
    headers,
    body: body ? JSON.stringify(body) : undefined,
  });

  const dados = await resposta.json().catch(() => null);

  if (!resposta.ok) {
    const mensagem = dados?.detail || "Não foi possível completar a requisição.";
    throw new Error(mensagem);
  }

  return dados;
}

export { API_URL };
