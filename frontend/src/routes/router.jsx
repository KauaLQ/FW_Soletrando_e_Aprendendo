import { createBrowserRouter, Navigate } from "react-router-dom";
import AuthPage from "../pages/auth/AuthPage";
import ProtectedRoute from "../components/layout/ProtectedRoute";
import AppLayout from "../components/layout/AppLayout";
import DashboardPage from "../pages/dashboard/DashboardPage";
import TurmaPage from "../pages/turma/TurmaPage";
import AlunoPage from "../pages/aluno/AlunoPage";

const router = createBrowserRouter([
  { path: "/", element: <Navigate to="/dashboard" replace /> },
  { path: "/entrar", element: <AuthPage /> },
  {
    element: <ProtectedRoute />,
    children: [
      {
        element: <AppLayout />,
        children: [
          { path: "/dashboard", element: <DashboardPage /> },
          { path: "/turmas/:turmaId", element: <TurmaPage /> },
          { path: "/alunos/:alunoId", element: <AlunoPage /> },
        ],
      },
    ],
  },
  // Qualquer rota desconhecida cai na tela de login por enquanto
  { path: "*", element: <Navigate to="/entrar" replace /> },
]);

export default router;