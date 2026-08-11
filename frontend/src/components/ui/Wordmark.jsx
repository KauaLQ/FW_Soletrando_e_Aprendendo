const ALTURAS = {
  sm: "h-6",
  md: "h-9",
  lg: "h-12",
};

/**
 * Logo oficial (ilustração + wordmark) exportada de /public. O arquivo tem
 * fundo branco opaco, então sobre um painel escuro (AuthLayout) embrulhamos
 * num cartão claro pra não "flutuar" um retângulo branco cru sobre o ink.
 */
export default function Wordmark({ tamanho = "md", sobreFundoEscuro = false }) {
  const imagem = (
    <img
      src="/logo-wordmark.png"
      alt="Soletrando e Aprendendo"
      className={`${ALTURAS[tamanho]} w-auto object-contain`}
    />
  );

  if (!sobreFundoEscuro) return imagem;

  return <div className="inline-flex bg-paper rounded-xl px-3 py-2 shadow-sm">{imagem}</div>;
}