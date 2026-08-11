export default function Badge({ sucesso, children }) {
  return (
    <span
      className={`inline-flex items-center px-2.5 py-1 rounded-full text-xs font-semibold shrink-0 ${
        sucesso ? "bg-marker/10 text-marker-dark" : "bg-danger/10 text-danger"
      }`}
    >
      {children}
    </span>
  );
}