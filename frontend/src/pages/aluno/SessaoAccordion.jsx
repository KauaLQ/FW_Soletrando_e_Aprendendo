import { useState } from "react";
import { ChevronDown } from "lucide-react";
import Badge from "../../components/ui/Badge";
import { API_URL } from "../../api/client";

function formatarData(iso) {
  return new Date(iso).toLocaleString("pt-BR", {
    day: "2-digit",
    month: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
  });
}

export default function SessaoAccordion({ sessao }) {
  const [aberta, setAberta] = useState(false);
  const acertos = sessao.tentativas.filter((t) => t.resultado).length;

  return (
    <div className="bg-white border border-ink/10 rounded-2xl overflow-hidden">
      <button onClick={() => setAberta((v) => !v)} className="w-full flex items-center justify-between p-4 text-left">
        <div className="min-w-0">
          <p className="font-semibold text-ink text-sm truncate">
            Nível {sessao.nivel_atingido} · {formatarData(sessao.data_inicio)}
          </p>
          <p className="text-xs text-ink/50">{acertos}/{sessao.tentativas.length} acertos</p>
        </div>
        <ChevronDown
          size={18}
          className={`text-ink/40 shrink-0 transition-transform ${aberta ? "rotate-180" : ""}`}
        />
      </button>

      {aberta && (
        <div className="border-t border-ink/10 divide-y divide-ink/10">
          {sessao.tentativas.length === 0 ? (
            <p className="p-4 text-xs text-ink/40">Nenhuma tentativa registrada nesta sessão.</p>
          ) : (
            sessao.tentativas.map((t) => (
              <div key={t.id} className="p-4 space-y-2">
                <div className="flex items-center justify-between gap-2">
                  <div className="min-w-0">
                    <p className="text-sm font-medium text-ink truncate">{t.palavra_esperada}</p>
                    <p className="text-xs text-ink/50 truncate">Transcrito: {t.transcricao_audio || "—"}</p>
                  </div>
                  <Badge sucesso={t.resultado}>{t.resultado ? "Acertou" : "Errou"}</Badge>
                </div>

                <div className="flex flex-wrap gap-x-4 gap-y-1 text-xs text-ink/50">
                  <span>Distância: {t.distancia_levenshtein}</span>
                  <span>Tempo: {t.tempo_audio.toFixed(1)}s</span>
                  <span>{formatarData(t.criado_em)}</span>
                </div>

                {t.audio && <audio controls src={`${API_URL}/${t.audio}`} className="w-full h-9" />}
              </div>
            ))
          )}
        </div>
      )}
    </div>
  );
}