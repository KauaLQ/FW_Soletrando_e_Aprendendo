import { LogOut } from "lucide-react";
import { useNavigate } from "react-router-dom";
import { useAuth } from "../../context/useAuth";
import LetterTile from "../ui/LetterTile";
import Wordmark from "../ui/Wordmark";

export default function Header() {
  const { professor, sair } = useAuth();
  const navegar = useNavigate();

  function aoSair() {
    sair();
    navegar("/entrar", { replace: true });
  }

  const inicial = professor?.nome?.[0]?.toUpperCase() || "?";

  return (
    <header className="sticky top-0 z-30 bg-paper/95 backdrop-blur border-b border-ink/10">
      <div className="px-4 h-16 flex items-center justify-between">
        <Wordmark tamanho="sm" />

        <div className="flex items-center gap-2 shrink-0">
          <div className="flex items-center gap-2 min-w-0">
            <LetterTile letra={inicial} indice={2} tamanho="sm" />
            <span className="text-sm text-ink/80 max-w-[6rem] truncate">{professor?.nome}</span>
          </div>
          <button
            onClick={aoSair}
            aria-label="Sair da conta"
            className="p-2 rounded-lg text-ink/50 hover:text-danger hover:bg-danger/10 transition-colors shrink-0"
          >
            <LogOut size={18} />
          </button>
        </div>
      </div>
    </header>
  );
}