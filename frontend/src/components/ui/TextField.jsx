import { useState } from "react";
import { Eye, EyeOff } from "lucide-react";

/**
 * Campo de texto padrão do app: label acima, ícone opcional à esquerda,
 * e olho de mostrar/ocultar automático quando tipo="password".
 * Alvos de toque generosos (py-3) pensando em mobile primeiro.
 */
export default function TextField({
  label,
  tipo = "text",
  valor,
  aoAlterar,
  icone: Icone,
  erro,
  ...props
}) {
  const [mostrarSenha, setMostrarSenha] = useState(false);
  const ehSenha = tipo === "password";
  const tipoReal = ehSenha && mostrarSenha ? "text" : tipo;

  return (
    <label className="block">
      <span className="block text-sm font-medium text-ink/70 mb-1.5">{label}</span>
      <div
        className={`flex items-center gap-2 rounded-xl border bg-white px-3.5 py-3 transition-colors ${
          erro ? "border-danger" : "border-ink/10 focus-within:border-marker"
        }`}
      >
        {Icone && <Icone size={18} className="text-ink/40 shrink-0" />}
        <input
          type={tipoReal}
          value={valor}
          onChange={aoAlterar}
          className="w-full bg-transparent outline-none text-ink placeholder:text-ink/30 text-base"
          {...props}
        />
        {ehSenha && (
          <button
            type="button"
            onClick={() => setMostrarSenha((v) => !v)}
            className="text-ink/40 hover:text-ink/70 shrink-0"
            tabIndex={-1}
            aria-label={mostrarSenha ? "Ocultar senha" : "Mostrar senha"}
          >
            {mostrarSenha ? <EyeOff size={18} /> : <Eye size={18} />}
          </button>
        )}
      </div>
      {erro && <span className="mt-1 block text-xs text-danger">{erro}</span>}
    </label>
  );
}
