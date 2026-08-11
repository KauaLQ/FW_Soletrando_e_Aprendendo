const VARIANTES = {
  primario: "bg-marker text-paper hover:bg-marker-dark shadow-sm shadow-marker/25 hover:shadow-md",
  fantasma: "bg-transparent text-ink hover:bg-ink/5",
};

export default function Button({ children, variante = "primario", carregando = false, className = "", ...props }) {
  return (
    <button
      className={`w-full inline-flex items-center justify-center gap-2 rounded-xl px-4 py-3 text-base font-semibold transition-all active:scale-[0.98] disabled:opacity-60 disabled:cursor-not-allowed disabled:active:scale-100 ${VARIANTES[variante]} ${className}`}
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