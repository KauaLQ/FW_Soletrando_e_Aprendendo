import { Outlet } from "react-router-dom";
import Header from "./Header";

/**
 * Layout de toda página autenticada: header fixo + área de conteúdo
 * centralizada. Novas páginas (dashboard, turma, aluno) entram como rotas
 * filhas de <AppLayout> e só precisam se preocupar com o próprio conteúdo.
 */
export default function AppLayout() {
  return (
    <div className="min-h-screen bg-paper">
      <Header />
      <main className="px-4 py-6 md:px-8 md:py-8 max-w-5xl mx-auto">
        <Outlet />
      </main>
    </div>
  );
}
