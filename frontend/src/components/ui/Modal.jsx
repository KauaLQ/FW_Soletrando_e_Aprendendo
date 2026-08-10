import { X } from "lucide-react";

/**
 * Modal em formato "bottom sheet" (sobe da parte de baixo da tela) --
 * é o padrão mobile mais natural, em vez de um dialog centralizado como
 * seria no desktop. `fixed inset-0` fica contido dentro do MobileFrame
 * (ver comentário lá) então nunca escapa pro tamanho da janela inteira.
 */
export default function Modal({ aberto, aoFechar, titulo, children }) {
  if (!aberto) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-end">
      <div className="absolute inset-0 bg-ink/50" onClick={aoFechar} />
      <div className="relative w-full bg-paper rounded-t-3xl px-6 pt-5 pb-8 max-h-[85%] overflow-y-auto animate-slide-up">
        <div className="flex items-center justify-between mb-4">
          <h2 className="font-display text-xl text-ink">{titulo}</h2>
          <button
            onClick={aoFechar}
            aria-label="Fechar"
            className="p-1.5 -mr-1.5 rounded-full text-ink/40 hover:bg-ink/5"
          >
            <X size={20} />
          </button>
        </div>
        {children}
      </div>
    </div>
  );
}