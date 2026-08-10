import { ChevronLeft } from "lucide-react";
import { useNavigate } from "react-router-dom";

/**
 * Cabeçalho de sub-página (turma, aluno...): seta de voltar + título.
 * Padrão de navegação mobile. evita precisar de breadcrumbs de desktop.
 */
export default function TopoPagina({ titulo, subtitulo }) {
  const navegar = useNavigate();

  return (
    <div className="flex items-center gap-2 mb-5">
      <button
        onClick={() => navegar(-1)}
        aria-label="Voltar"
        className="p-2 -ml-2 rounded-full text-ink/60 hover:bg-ink/5 shrink-0"
      >
        <ChevronLeft size={22} />
      </button>
      <div className="min-w-0">
        <h1 className="font-display text-xl text-ink truncate">{titulo}</h1>
        {subtitulo && <p className="text-xs text-ink/50 truncate">{subtitulo}</p>}
      </div>
    </div>
  );
}