import { ChevronRight } from "lucide-react";
import LetterTile from "./LetterTile";

export default function AlunoRow({ aluno, indice = 0, onClick }) {
  return (
    <button
      onClick={onClick}
      className="w-full flex items-center gap-3 bg-white border border-ink/10 rounded-2xl p-3.5 text-left hover:border-marker/40 active:scale-[0.99] transition"
    >
      <LetterTile letra={aluno.nome?.[0]?.toUpperCase() || "?"} indice={indice} tamanho="md" className="shrink-0" />
      <span className="flex-1 font-medium text-ink truncate">{aluno.nome}</span>
      <ChevronRight size={18} className="text-ink/30 shrink-0" />
    </button>
  );
}