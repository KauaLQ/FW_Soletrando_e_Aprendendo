import { useEffect, useState } from "react";
import { useParams, useLocation, useNavigate } from "react-router-dom";
import { listarTurmas, listarAlunos, criarAluno } from "../../api/turmas";
import { useAuth } from "../../context/useAuth";
import TopoPagina from "../../components/ui/TopoPagina";
import AlunoRow from "../../components/ui/AlunoRow";
import BotaoFlutuante from "../../components/ui/BotaoFlutuante";
import ModalNovoAluno from "./ModalNovoAluno";

export default function TurmaPage() {
  const { turmaId } = useParams();
  const location = useLocation();
  const navegar = useNavigate();
  const { token } = useAuth();

  // Se veio de um clique no card do dashboard, já temos os dados da turma
  // (nome/ano) sem precisar buscar de novo. Se a página foi acessada direto
  // (ex.: F5), cai no fallback abaixo e busca na lista de turmas.
  const [turma, setTurma] = useState(location.state?.turma || null);
  const [alunos, setAlunos] = useState([]);
  const [carregando, setCarregando] = useState(true);
  const [erro, setErro] = useState("");
  const [modalAberto, setModalAberto] = useState(false);

  const carregarDados = async () => {
    setCarregando(true);
    setErro("");
    try {
      const [listaAlunos, turmas] = await Promise.all([
        listarAlunos(token, turmaId),
        turma ? Promise.resolve(null) : listarTurmas(token),
      ]);
      setAlunos(listaAlunos);
      if (turmas) {
        const encontrada = turmas.find((t) => String(t.id) === turmaId);
        if (encontrada) setTurma(encontrada);
      }
    } catch (err) {
      setErro(err.message);
    } finally {
      setCarregando(false);
    }
  };

  useEffect(() => {
    // eslint-disable-next-line react-hooks/set-state-in-effect -- fetch de dados ao montar é o padrão recomendado pelo próprio React, não uma derivação de estado
    carregarDados();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [turmaId]);

  async function aoCriarAluno({ nome }) {
    const novo = await criarAluno(token, turmaId, { nome });
    setAlunos((atual) => [...atual, novo]);
    setModalAberto(false);
  }

  function abrirAluno(aluno) {
    navegar(`/alunos/${aluno.id}`, { state: { aluno } });
  }

  return (
    <div className="pb-24">
      <TopoPagina
        titulo={turma?.nome || "Turma"}
        subtitulo={turma ? `Ano letivo ${turma.ano_letivo}` : undefined}
      />

      {erro && (
        <div className="mb-4 rounded-lg bg-danger/10 px-3.5 py-2.5 text-sm text-danger">{erro}</div>
      )}

      {carregando ? (
        <div className="flex justify-center py-10">
          <span className="w-6 h-6 border-2 border-ink/20 border-t-ink rounded-full animate-spin" />
        </div>
      ) : alunos.length === 0 ? (
        <div className="text-center py-14 px-4">
          <p className="text-ink/50 text-sm">
            Toque no botão abaixo pra cadastrar o primeiro aluno dessa turma.
          </p>
        </div>
      ) : (
        <div className="space-y-2.5">
          {alunos.map((aluno, i) => (
            <AlunoRow key={aluno.id} aluno={aluno} indice={i} onClick={() => abrirAluno(aluno)} />
          ))}
        </div>
      )}

      <BotaoFlutuante label="Novo aluno" onClick={() => setModalAberto(true)} />

      <ModalNovoAluno
        aberto={modalAberto}
        aoFechar={() => setModalAberto(false)}
        aoCriar={aoCriarAluno}
      />
    </div>
  );
}