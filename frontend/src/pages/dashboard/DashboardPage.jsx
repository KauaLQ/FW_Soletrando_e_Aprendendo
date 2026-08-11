import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { BookOpen } from "lucide-react";
import { listarTurmas, criarTurma } from "../../api/turmas";
import { useAuth } from "../../context/useAuth";
import TurmaCard from "../../components/ui/TurmaCard";
import BotaoFlutuante from "../../components/ui/BotaoFlutuante";
import ModalNovaTurma from "./ModalNovaTurma";

export default function DashboardPage() {
  const { token } = useAuth();
  const navegar = useNavigate();

  const [turmas, setTurmas] = useState([]);
  const [carregando, setCarregando] = useState(true);
  const [erro, setErro] = useState("");
  const [modalAberto, setModalAberto] = useState(false);

  const carregarTurmas = async () => {
    setCarregando(true);
    setErro("");
    try {
      const dados = await listarTurmas(token);
      setTurmas(dados);
    } catch (err) {
      setErro(err.message);
    } finally {
      setCarregando(false);
    }
  };

  useEffect(() => {
    // eslint-disable-next-line react-hooks/set-state-in-effect - fetch de dados ao montar é o padrão recomendado pelo próprio React, não uma derivação de estado
    carregarTurmas();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  async function aoCriarTurma({ nome, ano_letivo }) {
    const nova = await criarTurma(token, { nome, ano_letivo });
    setTurmas((atual) => [...atual, nova]);
    setModalAberto(false);
  }

  function abrirTurma(turma) {
    navegar(`/turmas/${turma.id}`, { state: { turma } });
  }

  return (
    <div className="pb-24">
      <h1 className="font-display font-semibold text-2xl text-ink mb-1 flex items-center gap-2">
        <BookOpen size={22} className="text-marker" />
        Minhas turmas
      </h1>
      <p className="text-sm text-ink/60 mb-5">
        {turmas.length > 0
          ? `${turmas.length} ${turmas.length === 1 ? "turma cadastrada" : "turmas cadastradas"}`
          : "Nenhuma turma cadastrada ainda"}
      </p>

      {erro && (
        <div className="mb-4 rounded-lg bg-danger/10 px-3.5 py-2.5 text-sm text-danger">{erro}</div>
      )}

      {carregando ? (
        <div className="flex justify-center py-10">
          <span className="w-6 h-6 border-2 border-ink/20 border-t-ink rounded-full animate-spin" />
        </div>
      ) : turmas.length === 0 ? (
        <div className="text-center py-14 px-4">
          <p className="text-ink/50 text-sm">Toque no botão abaixo pra cadastrar sua primeira turma.</p>
        </div>
      ) : (
        <div className="space-y-3">
          {turmas.map((turma, i) => (
            <TurmaCard key={turma.id} turma={turma} indice={i} onClick={() => abrirTurma(turma)} />
          ))}
        </div>
      )}

      <BotaoFlutuante label="Nova turma" onClick={() => setModalAberto(true)} />

      <ModalNovaTurma
        aberto={modalAberto}
        aoFechar={() => setModalAberto(false)}
        aoCriar={aoCriarTurma}
      />
    </div>
  );
}