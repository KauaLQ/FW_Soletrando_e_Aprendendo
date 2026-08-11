import { useEffect, useMemo, useState } from "react";
import { useParams, useLocation } from "react-router-dom";
import { Radio, AlertTriangle, CheckCircle2, XCircle, Target, Timer } from "lucide-react";
import TopoPagina from "../../components/ui/TopoPagina";
import StatCard from "../../components/ui/StatCard";
import Button from "../../components/ui/Button";
import GraficoEvolucao from "../../components/ui/GraficoEvolucao";
import SessaoAccordion from "./SessaoAccordion";
import ModalParear from "./ModalParear";
import { obterAluno, obterPareamentoAtivo, parearAluno, despararAluno } from "../../api/alunos";
import { useAuth } from "../../context/useAuth";
import { useDashboardSocket } from "../../hooks/useDashboardSocket";

export default function AlunoPage() {
  const { alunoId } = useParams();
  const location = useLocation();
  const alunoResumo = location.state?.aluno;
  const { token } = useAuth();

  const [aluno, setAluno] = useState(null);
  const [pinAtivo, setPinAtivo] = useState(null);
  const [carregando, setCarregando] = useState(true);
  const [erro, setErro] = useState("");
  const [modalAberto, setModalAberto] = useState(false);
  const [avisoPinIncorreto, setAvisoPinIncorreto] = useState(null);

  // comSpinner=false é usado nas atualizações disparadas pelo WebSocket,
  // pra não piscar o loading da página inteira a cada nova tentativa.
  const carregarDados = async (comSpinner = true) => {
    if (comSpinner) setCarregando(true);
    setErro("");
    try {
      const [dadosAluno, pareamento] = await Promise.all([
        obterAluno(token, alunoId),
        obterPareamentoAtivo(token, alunoId),
      ]);
      setAluno(dadosAluno);
      setPinAtivo(pareamento.pin);
    } catch (err) {
      setErro(err.message);
    } finally {
      if (comSpinner) setCarregando(false);
    }
  };

  useEffect(() => {
    let cancelado = false;

    async function carregar() {
      setCarregando(true);
      setErro("");
      setAluno(null);

      try {
        const [dadosAluno, pareamento] = await Promise.all([
          obterAluno(token, alunoId),
          obterPareamentoAtivo(token, alunoId),
        ]);

        if (cancelado) return;

        setAluno(dadosAluno);
        setPinAtivo(pareamento.pin);
      } catch (err) {
        if (!cancelado) {
          setErro(err.message);
        }
      } finally {
        if (!cancelado) {
          setCarregando(false);
        }
      }
    }

    carregar();

    return () => {
      cancelado = true;
    };
  }, [token, alunoId]);

  // Reage em tempo real ao que acontece no dispositivo/backend.
  useDashboardSocket(token, (evento) => {
    if (evento.tipo === "nova_tentativa" && String(evento.aluno_id) === alunoId) {
      carregarDados(false);
    } else if (evento.tipo === "pareamento_alterado" && String(evento.aluno_id) === alunoId) {
      setPinAtivo(evento.pin);
    } else if (evento.tipo === "pin_incorreto") {
      setAvisoPinIncorreto(evento.pin);
      setTimeout(() => setAvisoPinIncorreto(null), 8000);
    }
  });

  async function aoParear(pin) {
    const resposta = await parearAluno(token, alunoId, pin);
    setPinAtivo(resposta.pin);
    setModalAberto(false);
  }

  async function aoDesparear() {
    await despararAluno(token, alunoId);
    setPinAtivo(null);
  }

  const taxaPrecisao =
    aluno && aluno.total_acertos + aluno.total_erros > 0
      ? Math.round((aluno.total_acertos / (aluno.total_acertos + aluno.total_erros)) * 100)
      : 0;

  // Backend manda as sessões da mais recente pra mais antiga; o gráfico quer ordem cronológica
  const sessoesCronologicas = useMemo(() => (aluno ? [...aluno.sessoes].reverse() : []), [aluno]);

  return (
    <div className="pb-24">
      <TopoPagina
        titulo={aluno?.nome || alunoResumo?.nome || `Aluno #${alunoId}`}
        subtitulo="Desempenho na soletração"
      />

      {erro && (
        <div className="mb-4 rounded-lg bg-danger/10 px-3.5 py-2.5 text-sm text-danger">{erro}</div>
      )}

      {avisoPinIncorreto && !modalAberto && (
        <div className="mb-4 rounded-lg bg-danger/10 px-3.5 py-2.5 text-sm text-danger flex items-start gap-2">
          <AlertTriangle size={16} className="shrink-0 mt-0.5" />
          <span>
            O dispositivo enviou dados com o PIN <strong className="font-mono">{avisoPinIncorreto}</strong>, que não
            corresponde a nenhum pareamento ativo.
          </span>
        </div>
      )}

      {carregando ? (
        <div className="flex justify-center py-10">
          <span className="w-6 h-6 border-2 border-ink/20 border-t-ink rounded-full animate-spin" />
        </div>
      ) : aluno ? (
        <>
          <div className="flex gap-3 mb-3">
            <StatCard label="Acertos" valor={aluno.total_acertos} corDestaque="text-marker" icone={CheckCircle2} />
            <StatCard label="Erros" valor={aluno.total_erros} corDestaque="text-danger" icone={XCircle} />
            <StatCard label="Precisão" valor={`${taxaPrecisao}%`} icone={Target} />
          </div>
          <div className="mb-5">
            <StatCard
              label="Tempo médio de soletração"
              valor={`${aluno.tempo_medio_soletracao.toFixed(1)}s`}
              icone={Timer}
            />
          </div>

          {sessoesCronologicas.length > 0 && (
            <div className="bg-white border border-ink/10 rounded-2xl p-4 mb-5">
              <h2 className="text-sm font-semibold text-ink/70 mb-3">Evolução por sessão</h2>
              <GraficoEvolucao key={alunoId} alunoId={alunoId} sessoes={sessoesCronologicas} />
            </div>
          )}

          <Button
            variante="fantasma"
            onClick={() => setModalAberto(true)}
            className={`border mb-6 ${pinAtivo ? "border-marker/30 text-marker" : "border-ink/15 text-ink/70"}`}
          >
            <Radio size={18} />
            {pinAtivo ? `Pareado (PIN ${pinAtivo})` : "Parear com dispositivo ESP32"}
          </Button>

          <h2 className="font-display font-semibold text-lg text-ink mb-3">Histórico de sessões</h2>
          {aluno.sessoes.length === 0 ? (
            <p className="text-sm text-ink/50 py-6 text-center">Nenhuma sessão registrada ainda.</p>
          ) : (
            <div className="space-y-2.5">
              {aluno.sessoes.map((sessao) => (
                <SessaoAccordion key={sessao.id} sessao={sessao} />
              ))}
            </div>
          )}
        </>
      ) : null}

      <ModalParear
        aberto={modalAberto}
        aoFechar={() => setModalAberto(false)}
        aoParear={aoParear}
        aoDesparear={aoDesparear}
        pinAtivo={pinAtivo}
        avisoPinIncorreto={avisoPinIncorreto}
      />
    </div>
  );
}