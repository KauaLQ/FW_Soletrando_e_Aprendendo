import { Plus } from "lucide-react";

/**
 * Botão de ação flutuante (FAB), fixo no canto inferior direito do
 * MobileFrame. Reaproveitado no dashboard ("+ Nova turma") e na turma
 * ("+ Novo aluno").
 */
export default function BotaoFlutuante({ onClick, label = "Novo" }) {
  return (
    <button
      onClick={onClick}
      className="fixed bottom-6 right-6 z-40 flex items-center gap-2 bg-marker text-paper pl-4 pr-5 py-3.5 rounded-full shadow-lg shadow-marker/30 font-semibold hover:bg-marker-dark transition-colors"
    >
      <Plus size={20} />
      {label}
    </button>
  );
}