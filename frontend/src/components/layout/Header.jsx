import { LogOut } from "lucide-react";
import { useNavigate } from "react-router-dom";
import { useAuth } from "../../context/AuthContext";
import LetterTile from "../ui/LetterTile";

export default function Header() {
  const { professor, sair } = useAuth();
  const navegar = useNavigate();

  function aoSair() {
    sair();
    navegar("/entrar", { replace: true });
  }

  const inicial = professor?.nome?.[0]?.toUpperCase() || "?";

  return (
    <header className="sticky top-0 z-10 bg-paper/95 backdrop-blur border-b border-ink/10">
      <div className="max-w-5xl mx-auto px-4 md:px-8 h-16 flex items-center justify-between">
        <div className="flex items-center gap-2">
          <LetterTile letra="S" indice={0} tamanho="sm" />
          <span className="font-display text-lg text-ink hidden sm:inline">Soletrando</span>
        </div>

        <div className="flex items-center gap-3">
          <div className="flex items-center gap-2">
            <LetterTile letra={inicial} indice={2} tamanho="sm" />
            <span className="text-sm text-ink/80 hidden sm:inline max-w-[10rem] truncate">
              {professor?.nome}
            </span>
          </div>
          <button
            onClick={aoSair}
            aria-label="Sair da conta"
            className="p-2 rounded-lg text-ink/50 hover:text-danger hover:bg-danger/10 transition-colors"
          >
            <LogOut size={18} />
          </button>
        </div>
      </div>
    </header>
  );
}
