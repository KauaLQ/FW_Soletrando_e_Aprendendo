import { createContext, useState } from "react";

const CHAVE_TOKEN = "soletrando_token";
const CHAVE_PROFESSOR = "soletrando_professor";

// eslint-disable-next-line react-refresh/only-export-components
export const AuthContext = createContext(null);

export function AuthProvider({ children }) {
  const [token, setToken] = useState(() => localStorage.getItem(CHAVE_TOKEN));
  const [professor, setProfessor] = useState(() => {
    const salvo = localStorage.getItem(CHAVE_PROFESSOR);
    return salvo ? JSON.parse(salvo) : null;
  });

  function entrar(tokenNovo, professorNovo) {
    localStorage.setItem(CHAVE_TOKEN, tokenNovo);
    localStorage.setItem(CHAVE_PROFESSOR, JSON.stringify(professorNovo));
    setToken(tokenNovo);
    setProfessor(professorNovo);
  }

  function sair() {
    localStorage.removeItem(CHAVE_TOKEN);
    localStorage.removeItem(CHAVE_PROFESSOR);
    setToken(null);
    setProfessor(null);
  }

  const valor = { token, professor, autenticado: Boolean(token), entrar, sair };
  return <AuthContext.Provider value={valor}>{children}</AuthContext.Provider>;
}