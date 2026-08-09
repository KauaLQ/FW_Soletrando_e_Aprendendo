const CORES = ["bg-marker", "bg-chalk", "bg-chalk-blue", "bg-danger"];

const TAMANHOS = {
  sm: "w-7 h-7 text-sm",
  md: "w-10 h-10 text-lg",
  lg: "w-14 h-14 text-2xl",
};

/**
 * Bloco de letra ao estilo peça de alfabeto -- é o elemento visual que se
 * repete pelo app (wordmark, avatar do professor, e futuramente PIN/nível).
 * `indice` decide a cor (cicla pela paleta), não precisa ser único.
 */
export default function LetterTile({ letra, indice = 0, tamanho = "md", className = "" }) {
  const cor = CORES[indice % CORES.length];

  return (
    <span
      className={`inline-flex items-center justify-center ${TAMANHOS[tamanho]} ${cor} text-paper font-display font-semibold rounded-tile shadow-sm select-none ${className}`}
    >
      {letra}
    </span>
  );
}
