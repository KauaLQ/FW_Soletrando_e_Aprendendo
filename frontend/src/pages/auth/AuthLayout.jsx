import Wordmark from "../../components/ui/Wordmark";

/**
 * No mobile, o painel de marca vira uma faixa compacta no topo (rounded-b
 * pra parecer um cartão) e o formulário ocupa o resto da tela.
 * No desktop, os dois viram colunas lado a lado.
 */
export default function AuthLayout({ children }) {
  return (
    <div className="min-h-screen bg-paper flex flex-col md:flex-row">
      <div className="bg-ink px-6 pt-10 pb-8 rounded-b-[2rem] md:rounded-none md:flex-1 md:flex md:flex-col md:justify-center md:px-16 md:py-0">
        <Wordmark tamanho="lg" sobreFundoEscuro />
        <p className="mt-4 max-w-xs text-paper/70 text-sm md:text-base hidden md:block">
          Acompanhe a jornada de soletração de cada aluno, palavra por palavra.
        </p>
      </div>

      <div className="flex-1 flex items-center justify-center px-6 py-10">
        <div className="w-full max-w-sm">{children}</div>
      </div>
    </div>
  );
}
