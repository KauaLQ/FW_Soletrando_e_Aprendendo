export default function StatCard({ label, valor, corDestaque = "text-ink" }) {
  return (
    <div className="bg-white border border-ink/10 rounded-2xl p-4 flex-1 min-w-[92px]">
      <p className="text-xs text-ink/50 mb-1">{label}</p>
      <p className={`font-display text-2xl ${corDestaque}`}>{valor}</p>
    </div>
  );
}