import { createContext, useContext, useEffect, useState } from "react";

const CHAVE_TOKEN = "soletrando_token";
const CHAVE_PROFESSOR = "soletrando_professor";

const AuthContext = createContext(null);

export function AuthProvider({ children }) {
  const [token, setToken] = useState(null);
  const [professor, setProfessor] = useState(null);
  // Evita "piscar" a tela de login antes de checarmos o localStorage
  const [carregando, setCarregando] = useState(true);

  useEffect(() => {
    const tokenSalvo = localStorage.getItem(CHAVE_TOKEN);
    const professorSalvo = localStorage.getItem(CHAVE_PROFESSOR);

    if (tokenSalvo && professorSalvo) {
      setToken(tokenSalvo);
      setProfessor(JSON.parse(professorSalvo));
    }
    setCarregando(false);
  }, []);

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

  const valor = {
    token,
    professor,
    carregando,
    autenticado: Boolean(token),
    entrar,
    sair,
  };

  return <AuthContext.Provider value={valor}>{children}</AuthContext.Provider>;
}

export function useAuth() {
  const contexto = useContext(AuthContext);
  if (!contexto) throw new Error("useAuth precisa estar dentro de um <AuthProvider>");
  return contexto;
}
