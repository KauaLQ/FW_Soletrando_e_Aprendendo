import { useEffect, useRef } from "react";
import { API_URL } from "../api/client";

const INTERVALO_RECONEXAO_MS = 3000;

/**
 * Mantém uma conexão WebSocket com o backend pra receber eventos em tempo
 * real (nova tentativa, pareamento alterado, PIN incorreto). Reconecta
 * sozinho se a conexão cair, mesmo espírito do reconnect que o firmware
 * já faz com o WebSocket da ESP32.
 */
export function useDashboardSocket(token, aoReceberEvento) {
  // Guardamos o callback numa ref pra não precisar recriar a conexão toda
  // vez que o componente re-renderiza com uma nova função inline.
  const callbackRef = useRef(aoReceberEvento);
  callbackRef.current = aoReceberEvento;

  useEffect(() => {
    if (!token) return;

    let socket;
    let timeoutReconexao;
    let cancelado = false;

    function conectar() {
      const wsUrl = `${API_URL.replace(/^http/, "ws")}/ws/dashboard?token=${token}`;
      socket = new WebSocket(wsUrl);

      socket.onmessage = (msg) => {
        try {
          callbackRef.current(JSON.parse(msg.data));
        } catch {
          // ignora mensagem mal formada
        }
      };

      socket.onclose = () => {
        if (!cancelado) {
          timeoutReconexao = setTimeout(conectar, INTERVALO_RECONEXAO_MS);
        }
      };
    }

    conectar();

    return () => {
      cancelado = true;
      clearTimeout(timeoutReconexao);
      socket?.close();
    };
  }, [token]);
}