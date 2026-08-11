import Wordmark from "../../components/ui/Wordmark";

/**
 * Sempre no formato mobile: faixa de marca compacta no topo (rounded-b pra
 * parecer um cartão) + formulário abaixo. Sem variação de desktop.
 */
export default function AuthLayout({ children }) {
  return (
    <div className="min-h-full flex flex-col">
      <div className="bg-gradient-to-br from-ink via-ink to-marker-dark px-6 pt-10 pb-9 rounded-b-[2rem] shadow-lg shadow-ink/20">
        <Wordmark tamanho="lg" sobreFundoEscuro />
        <p className="mt-4 text-paper/70 text-sm leading-relaxed">
          Acompanhe a jornada de soletração de cada aluno, palavra por palavra.
        </p>
      </div>

      <div className="flex-1 flex items-center justify-center px-6 py-10">
        <div className="w-full">{children}</div>
      </div>
    </div>
  );
}