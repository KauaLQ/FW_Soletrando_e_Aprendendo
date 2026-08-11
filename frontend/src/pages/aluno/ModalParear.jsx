import { useState } from "react";
import Modal from "../../components/ui/Modal";
import TextField from "../../components/ui/TextField";
import Button from "../../components/ui/Button";

export default function ModalParear({ aberto, aoFechar, aoParear, aoDesparear, pinAtivo, avisoPinIncorreto }) {
  const [pin, setPin] = useState("");
  const [erro, setErro] = useState("");
  const [salvando, setSalvando] = useState(false);

  function fechar() {
    setPin("");
    setErro("");
    aoFechar();
  }

  async function aoSubmeter(e) {
    e.preventDefault();
    setErro("");
    if (!/^\d{4}$/.test(pin)) {
      setErro("O PIN deve ter exatamente 4 dígitos.");
      return;
    }
    setSalvando(true);
    try {
      await aoParear(pin);
      setPin("");
    } catch (err) {
      setErro(err.message);
    } finally {
      setSalvando(false);
    }
  }

  return (
    <Modal aberto={aberto} aoFechar={fechar} titulo="Parear com dispositivo ESP32">
      <p className="text-sm text-ink/60 mb-4">
        Digite o PIN exibido no menu "Parear servidor" do dispositivo para vincular esta sessão ao aluno.
      </p>

      {pinAtivo && (
        <div className="mb-3 rounded-lg bg-marker/10 px-3.5 py-2.5 text-sm text-marker-dark flex items-center justify-between">
          <span>
            PIN ativo: <strong className="font-mono">{pinAtivo}</strong>
          </span>
          <button type="button" onClick={aoDesparear} className="text-xs font-semibold text-danger hover:underline">
            Desparear
          </button>
        </div>
      )}

      {avisoPinIncorreto && (
        <div className="mb-4 rounded-lg bg-danger/10 px-3.5 py-2.5 text-sm text-danger">
          O dispositivo enviou dados com o PIN <strong className="font-mono">{avisoPinIncorreto}</strong>, que não
          corresponde a nenhum pareamento ativo.
        </div>
      )}

      <form onSubmit={aoSubmeter} className="space-y-4">
        <TextField
          label="PIN do dispositivo"
          valor={pin}
          aoAlterar={(e) => setPin(e.target.value.replace(/\D/g, "").slice(0, 4))}
          placeholder="0000"
          inputMode="numeric"
          maxLength={4}
          required
        />

        {erro && (
          <div role="alert" className="rounded-lg bg-danger/10 px-3.5 py-2.5 text-sm text-danger">
            {erro}
          </div>
        )}

        <Button type="submit" carregando={salvando}>
          {pinAtivo ? "Reparear com novo PIN" : "Parear"}
        </Button>
      </form>
    </Modal>
  );
}