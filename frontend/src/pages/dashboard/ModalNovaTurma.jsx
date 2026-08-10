import { useState } from "react";
import Modal from "../../components/ui/Modal";
import TextField from "../../components/ui/TextField";
import Button from "../../components/ui/Button";

const ANO_ATUAL = new Date().getFullYear();

export default function ModalNovaTurma({ aberto, aoFechar, aoCriar }) {
  const [nome, setNome] = useState("");
  const [anoLetivo, setAnoLetivo] = useState(String(ANO_ATUAL));
  const [erro, setErro] = useState("");
  const [salvando, setSalvando] = useState(false);

  function fechar() {
    setNome("");
    setAnoLetivo(String(ANO_ATUAL));
    setErro("");
    aoFechar();
  }

  async function aoSubmeter(e) {
    e.preventDefault();
    setErro("");
    setSalvando(true);
    try {
      await aoCriar({ nome, ano_letivo: Number(anoLetivo) });
      setNome("");
      setAnoLetivo(String(ANO_ATUAL));
    } catch (err) {
      setErro(err.message);
    } finally {
      setSalvando(false);
    }
  }

  return (
    <Modal aberto={aberto} aoFechar={fechar} titulo="Nova turma">
      <form onSubmit={aoSubmeter} className="space-y-4">
        <TextField
          label="Nome da turma"
          valor={nome}
          aoAlterar={(e) => setNome(e.target.value)}
          placeholder="Ex.: 3º Ano A"
          required
        />
        <TextField
          label="Ano letivo"
          tipo="number"
          valor={anoLetivo}
          aoAlterar={(e) => setAnoLetivo(e.target.value)}
          placeholder={String(ANO_ATUAL)}
          required
        />

        {erro && (
          <div role="alert" className="rounded-lg bg-danger/10 px-3.5 py-2.5 text-sm text-danger">
            {erro}
          </div>
        )}

        <Button type="submit" carregando={salvando}>
          Criar turma
        </Button>
      </form>
    </Modal>
  );
}