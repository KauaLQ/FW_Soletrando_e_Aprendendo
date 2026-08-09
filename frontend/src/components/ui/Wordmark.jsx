import LetterTile from "./LetterTile";

/**
 * "SOL" em blocos de alfabeto + "etrando" em display type -- usado no painel
 * de marca da tela de login. `sobreFundoEscuro` ajusta a cor do texto solto
 * quando o wordmark aparece sobre o painel escuro (bg-ink).
 */
export default function Wordmark({ tamanho = "md", sobreFundoEscuro = false }) {
  return (
    <div className="flex items-center gap-2">
      <div className="flex gap-1">
        <LetterTile letra="S" indice={0} tamanho={tamanho} className="-rotate-6" />
        <LetterTile letra="O" indice={1} tamanho={tamanho} className="rotate-3" />
        <LetterTile letra="L" indice={2} tamanho={tamanho} className="-rotate-3" />
      </div>
      <span className={`font-display text-2xl ${sobreFundoEscuro ? "text-paper" : "text-ink"}`}>
        etrando
      </span>
    </div>
  );
}
