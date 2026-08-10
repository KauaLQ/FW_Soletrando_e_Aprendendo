import { Outlet } from "react-router-dom";
import Header from "./Header";

/**
 * Casca de toda página autenticada: header fixo + área de conteúdo.
 * Sem wrapper de altura/fundo aqui. o MobileFrame já cuida disso.
 */
export default function AppLayout() {
  return (
    <>
      <Header />
      <main className="px-4 py-6">
        <Outlet />
      </main>
    </>
  );
}