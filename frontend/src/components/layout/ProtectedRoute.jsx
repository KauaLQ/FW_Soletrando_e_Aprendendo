import { Navigate, Outlet } from "react-router-dom";
import { useAuth } from "../../context/AuthContext";

/**
 * Bloqueia o acesso às rotas filhas se não houver sessão ativa.
 * Enquanto ainda estamos checando o localStorage (carregando), não decide
 * nada -- evita mandar pra /entrar por um instante e "piscar" a tela.
 */
export default function ProtectedRoute() {
  const { autenticado, carregando } = useAuth();

  if (carregando) {
    return (
      <div className="min-h-screen flex items-center justify-center bg-paper">
        <span className="w-6 h-6 border-2 border-ink/20 border-t-ink rounded-full animate-spin" />
      </div>
    );
  }

  if (!autenticado) return <Navigate to="/entrar" replace />;

  return <Outlet />;
}
