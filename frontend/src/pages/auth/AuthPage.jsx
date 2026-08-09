import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { Mail, Lock, User } from "lucide-react";
import AuthLayout from "./AuthLayout";
import TextField from "../../components/ui/TextField";
import Button from "../../components/ui/Button";
import { login, registrar } from "../../api/auth";
import { useAuth } from "../../context/AuthContext";

export default function AuthPage() {
  // Alternância rápida entre login/registro sem trocar de rota -- mesma
  // tela, mesmo estado de formulário, só muda o que é submetido.
  const [modo, setModo] = useState("login"); // 'login' | 'registro'
  const [nome, setNome] = useState("");
  const [email, setEmail] = useState("");
  const [senha, setSenha] = useState("");
  const [erro, setErro] = useState("");
  const [carregando, setCarregando] = useState(false);

  const { entrar } = useAuth();
  const navegar = useNavigate();
  const ehRegistro = modo === "registro";

  async function aoSubmeter(e) {
    e.preventDefault();
    setErro("");
    setCarregando(true);
    try {
      const dados = ehRegistro ? await registrar(nome, email, senha) : await login(email, senha);
      entrar(dados.access_token, dados.professor);
      navegar("/dashboard", { replace: true });
    } catch (err) {
      setErro(err.message);
    } finally {
      setCarregando(false);
    }
  }

  function alternarModo() {
    setErro("");
    setModo(ehRegistro ? "login" : "registro");
  }

  return (
    <AuthLayout>
      <div className="mb-8">
        <h1 className="font-display text-3xl text-ink">
          {ehRegistro ? "Criar conta" : "Bem-vindo de volta"}
        </h1>
        <p className="mt-1 text-ink/60 text-sm">
          {ehRegistro
            ? "Cadastre-se para acompanhar suas turmas."
            : "Entre para ver o progresso das suas turmas."}
        </p>
      </div>

      <form onSubmit={aoSubmeter} className="space-y-4">
        {ehRegistro && (
          <TextField
            label="Nome"
            icone={User}
            valor={nome}
            aoAlterar={(e) => setNome(e.target.value)}
            placeholder="Seu nome completo"
            autoComplete="name"
            required
          />
        )}

        <TextField
          label="Email"
          tipo="email"
          icone={Mail}
          valor={email}
          aoAlterar={(e) => setEmail(e.target.value)}
          placeholder="voce@escola.com"
          autoComplete="email"
          required
        />

        <TextField
          label="Senha"
          tipo="password"
          icone={Lock}
          valor={senha}
          aoAlterar={(e) => setSenha(e.target.value)}
          placeholder="••••••••"
          minLength={6}
          autoComplete={ehRegistro ? "new-password" : "current-password"}
          required
        />

        {erro && (
          <div role="alert" className="rounded-lg bg-danger/10 px-3.5 py-2.5 text-sm text-danger">
            {erro}
          </div>
        )}

        <Button type="submit" carregando={carregando}>
          {ehRegistro ? "Criar conta" : "Entrar"}
        </Button>
      </form>

      <p className="mt-6 text-center text-sm text-ink/60">
        {ehRegistro ? "Já tem uma conta?" : "Ainda não tem conta?"}{" "}
        <button
          type="button"
          onClick={alternarModo}
          className="font-semibold text-marker hover:text-marker-dark"
        >
          {ehRegistro ? "Entrar" : "Criar conta"}
        </button>
      </p>
    </AuthLayout>
  );
}
