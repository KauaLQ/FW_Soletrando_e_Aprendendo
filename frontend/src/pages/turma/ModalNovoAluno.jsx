import { useState } from "react";
import Modal from "../../components/ui/Modal";
import TextField from "../../components/ui/TextField";
import Button from "../../components/ui/Button";

export default function ModalNovoAluno({ aberto, aoFechar, aoCriar }) {
  const [nome, setNome] = useState("");
  const [erro, setErro] = useState("");
  const [salvando, setSalvando] = useState(false);

  function fechar() {
    setNome("");
    setErro("");
    aoFechar();
  }

  async function aoSubmeter(e) {
    e.preventDefault();
    setErro("");
    setSalvando(true);
    try {
      await aoCriar({ nome });
      setNome("");
    } catch (err) {
      setErro(err.message);
    } finally {
      setSalvando(false);
    }
  }

  return (
    <Modal aberto={aberto} aoFechar={fechar} titulo="Novo aluno">
      <form onSubmit={aoSubmeter} className="space-y-4">
        <TextField
          label="Nome do aluno"
          valor={nome}
          aoAlterar={(e) => setNome(e.target.value)}
          placeholder="Nome completo"
          required
        />

        {erro && (
          <div role="alert" className="rounded-lg bg-danger/10 px-3.5 py-2.5 text-sm text-danger">
            {erro}
          </div>
        )}

        <Button type="submit" carregando={salvando}>
          Cadastrar aluno
        </Button>
      </form>
    </Modal>
  );
}