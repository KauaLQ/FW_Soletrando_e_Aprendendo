const VARIANTES = {
  primario: "bg-ink text-paper hover:bg-ink/90",
  fantasma: "bg-transparent text-ink hover:bg-ink/5",
};

export default function Button({ children, variante = "primario", carregando = false, className = "", ...props }) {
  return (
    <button
      className={`w-full inline-flex items-center justify-center gap-2 rounded-xl px-4 py-3 text-base font-semibold transition-colors disabled:opacity-60 disabled:cursor-not-allowed ${VARIANTES[variante]} ${className}`}
      disabled={carregando || props.disabled}
      {...props}
    >
      {carregando ? (
        <span className="w-4 h-4 border-2 border-current border-t-transparent rounded-full animate-spin" />
      ) : (
        children
      )}
    </button>
  );
}
