export default function StatCard({ label, valor, corDestaque = "text-ink", icone: Icone }) {
  return (
    <div className="bg-white border border-ink/10 rounded-2xl p-4 flex-1 min-w-[92px] shadow-sm shadow-ink/5">
      <div className="flex items-center gap-1.5 mb-1">
        {Icone && <Icone size={13} className="text-ink/40" />}
        <p className="text-xs text-ink/50">{label}</p>
      </div>
      <p className={`font-display font-semibold text-2xl ${corDestaque}`}>{valor}</p>
    </div>
  );
}