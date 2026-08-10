import { useParams, useLocation } from "react-router-dom";
import TopoPagina from "../../components/ui/TopoPagina";

export default function AlunoPage() {
  const { alunoId } = useParams();
  const location = useLocation();
  const aluno = location.state?.aluno;

  return (
    <div>
      <TopoPagina titulo={aluno?.nome || `Aluno #${alunoId}`} />
      <p className="text-sm text-ink/60">
        O dashboard de estatísticas e a lista de sessões entram aqui na próxima etapa.
      </p>
    </div>
  );
}