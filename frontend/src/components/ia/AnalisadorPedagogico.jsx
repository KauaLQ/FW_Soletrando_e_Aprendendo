import { useEffect, useState } from "react";
import {
  Sparkles, AlertTriangle, Ear, SpellCheck,
  Lightbulb, Clock3, CircleCheck, CircleX, Target,
} from "lucide-react";
import Button from "../ui/Button";
import { useAuth } from "../../context/useAuth";
import {
  obterRelatorioAluno, gerarRelatorioAluno,
  obterRelatorioTurma, gerarRelatorioTurma,
} from "../../api/relatorioIA";

function formatarData(iso) {
  return new Date(iso).toLocaleString("pt-BR", {
    day: "2-digit", month: "2-digit", hour: "2-digit", minute: "2-digit",
  });
}

const ICONE_TIPO = { fonetico: Ear, ortografico: SpellCheck };
const LABEL_TIPO = { fonetico: "Fonético", ortografico: "Ortográfico" };

export default function AnalisadorPedagogico({ tipo, id }) {
  const { token } = useAuth();
  const [relatorio, setRelatorio] = useState(null);
  const [geradoEm, setGeradoEm] = useState(null);
  const [carregandoCache, setCarregandoCache] = useState(true);
  const [gerando, setGerando] = useState(false);
  const [erro, setErro] = useState("");
  const [palavraAberta, setPalavraAberta] = useState(null);

  const buscarCache = tipo === "turma" ? obterRelatorioTurma : obterRelatorioAluno;
  const gerar = tipo === "turma" ? gerarRelatorioTurma : gerarRelatorioAluno;

  useEffect(() => {
    let cancelado = false;
    setCarregandoCache(true);
    buscarCache(token, id)
      .then((resposta) => {
        if (cancelado || !resposta) return;
        setRelatorio(resposta.relatorio);
        setGeradoEm(resposta.gerado_em);
      })
      .catch(() => {})
      .finally(() => { if (!cancelado) setCarregandoCache(false); });
    return () => { cancelado = true; };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tipo, id, token]);

  async function aoGerar() {
    setErro("");
    setGerando(true);
    try {
      const resposta = await gerar(token, id);
      setRelatorio(resposta.relatorio);
      setGeradoEm(resposta.gerado_em);
      setPalavraAberta(null);
    } catch (err) {
      setErro(err.message);
    } finally {
      setGerando(false);
    }
  }

  return (
    <div className="bg-white border border-ink/10 rounded-2xl p-4 mb-5">
      <div className="flex items-center gap-2 mb-1">
        <Sparkles size={18} className="text-marker" />
        <h2 className="font-display font-semibold text-lg text-ink">Análise Pedagógica com IA</h2>
      </div>
      <p className="text-xs text-ink/50 mb-4">
        {tipo === "turma"
          ? "Padrões de erro, palavras recomendadas e metodologias para a turma toda."
          : "Padrões de erro, palavras recomendadas e metodologias para este aluno."}
      </p>

      <Button onClick={aoGerar} carregando={gerando} disabled={carregandoCache} className="mb-2">
        <Sparkles size={18} />
        {relatorio ? "Atualizar Análise Pedagógica" : "Gerar Análise Pedagógica com IA"}
      </Button>

      {geradoEm && !gerando && (
        <p className="flex items-center gap-1 text-xs text-ink/40 mb-3">
          <Clock3 size={12} /> Gerado em {formatarData(geradoEm)}
        </p>
      )}

      {erro && (
        <div role="alert" className="flex items-start gap-2 rounded-lg bg-danger/10 px-3.5 py-2.5 text-sm text-danger mb-3">
          <AlertTriangle size={16} className="shrink-0 mt-0.5" />
          <span>{erro}</span>
        </div>
      )}

      {gerando && (
        <div className="flex items-center gap-3 rounded-xl bg-paper-dim px-4 py-4 mb-3">
          <span className="w-5 h-5 border-2 border-marker/30 border-t-marker rounded-full animate-spin shrink-0" />
          <p className="text-sm text-ink/60">
            A IA está analisando as tentativas registradas... isso pode levar alguns segundos.
          </p>
        </div>
      )}

      {relatorio && !gerando && (
        <div className="space-y-6 mt-2">
          <div className="flex gap-3">
            <MiniStat icone={CircleCheck} label="Acertos" valor={relatorio.resumo_desempenho.total_acertos} cor="text-marker" />
            <MiniStat icone={CircleX} label="Erros" valor={relatorio.resumo_desempenho.total_erros} cor="text-danger" />
            <MiniStat icone={Target} label="Precisão" valor={`${Math.round(relatorio.resumo_desempenho.taxa_acerto)}%`} cor="text-ink" />
          </div>

          <section>
            <h3 className="text-sm font-semibold text-ink/70 mb-2.5">Diagnóstico Fonético/Ortográfico</h3>
            {relatorio.padroes_identificados.length === 0 ? (
              <p className="text-sm text-ink/50">Nenhum padrão recorrente identificado — ótimo sinal!</p>
            ) : (
              <div className="space-y-2.5">
                {relatorio.padroes_identificados.map((padrao, i) => {
                const Icone = ICONE_TIPO[padrao.tipo] || SpellCheck;
                return (
                    <div key={i} className="bg-paper-dim rounded-xl p-3.5">
                    <div className="flex items-start gap-2 mb-1.5">
                        <Icone size={16} className="text-marker shrink-0 mt-0.5" />
                        <div className="min-w-0 flex-1">
                        <p className="text-sm font-semibold text-ink leading-snug">
                            {padrao.descricao}
                        </p>
                        <span className="inline-block max-w-full truncate align-top mt-1.5 px-2 py-0.5 rounded-full text-[11px] font-semibold bg-chalk/25 text-ink/70">
                            {LABEL_TIPO[padrao.tipo] || padrao.tipo} · {padrao.frequencia}x
                        </span>
                        </div>
                    </div>
                    {padrao.exemplos.length > 0 && (
                        <div className="flex flex-wrap gap-1.5 mt-2">
                        {padrao.exemplos.map((ex, j) => (
                            <span key={j} className="text-xs font-mono bg-white border border-ink/10 rounded-md px-2 py-1 text-ink/70">
                            {ex.palavra_esperada} → {ex.transcrito}
                            </span>
                        ))}
                        </div>
                    )}
                    </div>
                );
                })}
              </div>
            )}
          </section>

          <section>
            <h3 className="text-sm font-semibold text-ink/70 mb-2.5">Palavras Recomendadas para Treinar</h3>
            <div className="flex flex-wrap gap-2">
              {relatorio.palavras_recomendadas.map((p, i) => (
                <button
                  key={i}
                  onClick={() => setPalavraAberta(palavraAberta === i ? null : i)}
                  className={`px-3 py-1.5 rounded-full text-sm font-semibold transition-colors ${
                    palavraAberta === i ? "bg-marker text-paper" : "bg-marker/10 text-marker-dark hover:bg-marker/20"
                  }`}
                >
                  {p.palavra}
                </button>
              ))}
            </div>
            {palavraAberta !== null && relatorio.palavras_recomendadas[palavraAberta] && (
              <p className="mt-2.5 text-xs text-ink/60 bg-paper-dim rounded-lg px-3 py-2">
                {relatorio.palavras_recomendadas[palavraAberta].motivo}
              </p>
            )}
          </section>

          <section>
            <h3 className="text-sm font-semibold text-ink/70 mb-2.5">Estratégias Pedagógicas Sugeridas</h3>
            <div className="space-y-2.5">
              {relatorio.metodologias_sugeridas.map((m, i) => (
                <div key={i} className="border border-ink/10 rounded-xl p-3.5">
                  <div className="flex items-center gap-2 mb-1">
                    <Lightbulb size={16} className="text-chalk shrink-0" />
                    <span className="text-sm font-semibold text-ink">{m.nome}</span>
                  </div>
                  <p className="text-xs text-ink/60 mb-1.5">{m.descricao}</p>
                  <p className="text-xs text-ink/70 bg-paper-dim rounded-lg px-2.5 py-2">
                    <strong className="font-semibold">Como aplicar: </strong>{m.como_aplicar}
                  </p>
                </div>
              ))}
            </div>
          </section>

          {relatorio.observacoes_gerais && (
            <p className="text-xs text-ink/50 italic border-t border-ink/10 pt-3">
              {relatorio.observacoes_gerais}
            </p>
          )}
        </div>
      )}
    </div>
  );
}

function MiniStat({ icone: Icone, label, valor, cor }) {
  return (
    <div className="flex-1 bg-paper-dim rounded-xl p-2.5 text-center">
      <Icone size={14} className={`mx-auto mb-1 ${cor}`} />
      <p className={`font-display font-semibold text-lg ${cor}`}>{valor}</p>
      <p className="text-[10px] text-ink/50">{label}</p>
    </div>
  );
}