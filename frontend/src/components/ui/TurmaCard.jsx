import { Users, Mic, Layers, ChevronRight } from "lucide-react";
import LetterTile from "./LetterTile";

export default function TurmaCard({ turma, indice = 0, onClick }) {
  return (
    <button
      onClick={onClick}
      className="w-full flex items-center gap-3 bg-white border border-ink/10 rounded-2xl p-4 text-left hover:border-marker/40 active:scale-[0.99] transition"
    >
      <LetterTile
        letra={turma.nome?.[0]?.toUpperCase() || "T"}
        indice={indice}
        tamanho="lg"
        className="shrink-0"
      />

      <div className="flex-1 min-w-0">
        <h3 className="font-display text-lg text-ink truncate">{turma.nome}</h3>
        <p className="text-xs text-ink/50 mb-2">Ano letivo {turma.ano_letivo}</p>
        <div className="flex items-center gap-3 text-xs text-ink/60">
          <span className="flex items-center gap-1" title="Alunos">
            <Users size={14} /> {turma.total_alunos}
          </span>
          <span className="flex items-center gap-1" title="Palavras soletradas">
            <Mic size={14} /> {turma.total_palavras_gravadas}
          </span>
          <span className="flex items-center gap-1" title="Sessões">
            <Layers size={14} /> {turma.total_sessoes}
          </span>
        </div>
      </div>

      <ChevronRight size={18} className="text-ink/30 shrink-0" />
    </button>
  );
}