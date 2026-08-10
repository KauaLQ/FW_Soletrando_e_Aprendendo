import { Navigate, Outlet } from "react-router-dom";
import { useAuth } from "../../context/useAuth";

export default function ProtectedRoute() {
  const { autenticado } = useAuth();
  if (!autenticado) return <Navigate to="/entrar" replace />;
  return <Outlet />;
}