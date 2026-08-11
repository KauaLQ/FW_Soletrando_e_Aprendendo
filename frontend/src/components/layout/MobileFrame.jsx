/**
 * Força a experiência 100% mobile, mesmo em telas largas: o conteúdo fica
 * preso numa coluna com largura de celular (430px), centralizada. Em telas
 * realmente estreitas (celular de verdade), essa coluna ocupa 100% da tela
 * naturalmente, sem precisar de nenhum breakpoint.
 *
 * O `transform` aqui não muda nada visualmente, mas tem um efeito colateral
 * importante: vira o "containing block" de qualquer elemento `fixed` lá
 * dentro (botão flutuante, modais). Sem isso, um `fixed` prenderia no canto
 * da janela do navegador inteira, e não no canto desse quadro.
 *
 * `bg-notebook` aplica uma grade de pontinhos bem sutil no fundo, um
 * aceno discreto ao tema "caderno escolar" sem comprometer a leitura.
 */
export default function MobileFrame({ children }) {
  return (
    <div className="h-dvh w-full bg-paper-dim bg-notebook flex justify-center">
      <div className="relative transform w-full max-w-[430px] h-full bg-paper overflow-y-auto shadow-2xl shadow-ink/10">
        {children}
      </div>
    </div>
  );
}