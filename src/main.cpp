#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// ---------- Configurações de rede ----------
const char* WIFI_SSID     = "KAUA_LQ";
const char* WIFI_PASSWORD = "12345678";
const char* WS_HOST       = "192.168.1.101"; // IP do PC rodando backend.py
const uint16_t WS_PORT    = 8080;
const char* WS_PATH       = "/";

// ---------- Pinos do INMP441 (I2S) ----------
#define I2S_WS_PIN   25
#define I2S_SCK_PIN  32
#define I2S_SD_PIN   33
#define I2S_PORT     I2S_NUM_0

// ---------- Botões ----------
#define BUTTON_GRAVAR_PIN    35   // grava o áudio (só liberado com palavra pronta)
#define BUTTON_PALAVRA_PIN   34   // pede uma nova palavra ao backend
#define DEBOUNCE_MS          30

// ---------- Áudio ----------
#define SAMPLE_RATE       16000
#define AUDIO_SHIFT_BITS  14
#define CANAL_OFFSET      0

#define CHUNK_AMOSTRAS 512
#define FILA_TAMANHO 40
#define MAX_ENVIOS_POR_ITERACAO 2

// Nível sempre usado ao pedir palavra (conforme pedido: fixo no menor nível)
#define NIVEL_PALAVRA 1

typedef struct {
  int16_t dados[CHUNK_AMOSTRAS];
  size_t quantidade;
} ChunkAudio;

QueueHandle_t filaAudio;
TaskHandle_t tarefaCapturaHandle;

WebSocketsClient webSocket;

// ---------- Máquina de estados do jogo ----------
enum EstadoJogo {
  AGUARDANDO_PEDIDO_PALAVRA,    // esperando o jogador apertar IO34
  AGUARDANDO_RESPOSTA_PALAVRA,  // já pedimos, esperando o backend responder
  PALAVRA_PRONTA,               // temos palavra, IO35 liberado pra gravar
  GRAVANDO,                     // gravando áudio agora
  AGUARDANDO_RESULTADO          // já mandamos stop_audio, esperando feedback
};

volatile EstadoJogo estado = AGUARDANDO_PEDIDO_PALAVRA;
String palavraAtual = "";

volatile bool gravando = false;
volatile bool estadoConfirmadoGravar = HIGH;
bool lastGravarState = HIGH;
unsigned long lastDebounceGravarTime = 0;

volatile bool estadoConfirmadoPalavra = HIGH;
bool lastPalavraState = HIGH;
unsigned long lastDebouncePalavraTime = 0;

// diagnóstico: amplitude mínima/máxima observada durante a gravação atual
volatile int32_t diagMin = 0;
volatile int32_t diagMax = 0;

// ---------- I2S ----------
void i2sInstalar() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 10,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN
  };

  i2s_driver_install(I2S_PORT, &config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
}

inline int16_t converter32para16(int32_t amostra32) {
  return (int16_t)(amostra32 >> AUDIO_SHIFT_BITS);
}

// ---------- Tarefa dedicada de captura (roda sozinha no núcleo 0) ----------
void tarefaCapturaAudio(void *parametro) {
  static int32_t bufferI2S[CHUNK_AMOSTRAS * 2];

  for (;;) {
    if (gravando) {
      size_t bytesLidos = 0;
      i2s_read(I2S_PORT, bufferI2S, sizeof(bufferI2S), &bytesLidos, portMAX_DELAY);
      size_t amostrasLidas = bytesLidos / sizeof(int32_t);

      ChunkAudio chunk;
      size_t out = 0;
      for (size_t i = CANAL_OFFSET; i < amostrasLidas && out < CHUNK_AMOSTRAS; i += 2) {
        int32_t amostra = bufferI2S[i];
        if (amostra < diagMin) diagMin = amostra;
        if (amostra > diagMax) diagMax = amostra;
        chunk.dados[out++] = converter32para16(amostra);
      }
      chunk.quantidade = out;

      if (out > 0) {
        if (xQueueSend(filaAudio, &chunk, 0) != pdTRUE) {
          ChunkAudio descartado;
          xQueueReceive(filaAudio, &descartado, 0);
          xQueueSend(filaAudio, &chunk, 0);
        }
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

// ---------- WebSocket ----------
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("[WS] Conectado ao backend");
      break;
    case WStype_DISCONNECTED:
      Serial.println("[WS] Desconectado do backend");
      break;
    case WStype_TEXT: {
      String resposta = String((char*)payload).substring(0, length);
      // O texto recebido significa coisas diferentes dependendo do que estávamos esperando
      switch (estado) {
        case AGUARDANDO_RESPOSTA_PALAVRA:
          palavraAtual = resposta;
          Serial.println("=================================");
          Serial.printf("[JOGO] Palavra para soletrar: %s\n", palavraAtual.c_str());
          Serial.println("[JOGO] Pode apertar o botao de GRAVAR (IO35)");
          Serial.println("=================================");
          estado = PALAVRA_PRONTA;
          break;

        case AGUARDANDO_RESULTADO:
          Serial.printf("[JOGO] Resultado do backend: %s\n", resposta.c_str());
          if (resposta == palavraAtual) {
            Serial.println("[JOGO] Acertou!");
          } else {
            Serial.println("[JOGO] Nao bateu com a palavra esperada.");
          }
          Serial.println("[JOGO] Aperte IO34 para pedir a proxima palavra");
          estado = AGUARDANDO_PEDIDO_PALAVRA;
          break;

        default:
          Serial.printf("[WS] Resposta inesperada (estado atual nao esperava texto): %s\n",
                        resposta.c_str());
          break;
      }
      break;
    }
    default:
      break;
  }
}

// ---------- Botão de pedir palavra (IO34) ----------
void tratarBotaoPalavra() {
  bool leitura = digitalRead(BUTTON_PALAVRA_PIN);
  // Se o botão mudou (mesmo que por ruído), reinicia o timer
  if (leitura != lastPalavraState) {
    lastDebouncePalavraTime = millis();
  }
  // Se o estado se manteve estável por tempo suficiente (passou o debounce)
  if ((millis() - lastDebouncePalavraTime) > DEBOUNCE_MS) {
    // Verifica se o estado estável atual é diferente do último estado que confirmamos
    if (leitura != estadoConfirmadoPalavra) {
      estadoConfirmadoPalavra = leitura;
      if (estadoConfirmadoPalavra == LOW) {
        // só permite pedir palavra nova se não estivermos no meio de uma gravação ou já esperando resposta
        if (estado == AGUARDANDO_PEDIDO_PALAVRA) {
          Serial.printf("[BOTAO] Pedindo palavra (nivel %d)...\n", NIVEL_PALAVRA);
          webSocket.sendTXT(("pedir_palavra " + String(NIVEL_PALAVRA)).c_str());
          estado = AGUARDANDO_RESPOSTA_PALAVRA;
        } else {
          Serial.println("[AVISO] Ainda nao e possivel pedir nova palavra agora.");
        }
      }
    }
  }
  lastPalavraState = leitura;
}

// ---------- Botão de gravar (IO35) ----------
void tratarBotaoGravar() {
  bool leitura = digitalRead(BUTTON_GRAVAR_PIN);

  if (leitura != lastGravarState) {
    lastDebounceGravarTime = millis();
  }

  if ((millis() - lastDebounceGravarTime) > DEBOUNCE_MS) {
    // Atualiza o estado estável filtrado pelo debounce
    if (leitura != estadoConfirmadoGravar) {
      estadoConfirmadoGravar = leitura;
      if (estadoConfirmadoGravar == LOW) { // Acabou de PRESSIONAR o botão (Transição para LOW)
        if (!gravando) {
          if (estado != PALAVRA_PRONTA) {
            Serial.println("[AVISO] Peca uma palavra primeiro (botao IO34) antes de gravar.");
          } else {
            diagMin = INT32_MAX;
            diagMax = INT32_MIN;
            xQueueReset(filaAudio);
            gravando = true;
            estado = GRAVANDO;
            Serial.println("[BOTAO] Gravacao iniciada");
            webSocket.sendTXT("start_audio");
          }
        }
      } else if (estadoConfirmadoGravar == HIGH) { // Acabou de SOLTAR o botão (Transição para HIGH)
        if (gravando) {
          gravando = false;
          Serial.println("[BOTAO] Gravacao finalizada, enviando stop_audio");

          ChunkAudio chunk;
          while (xQueueReceive(filaAudio, &chunk, 0) == pdTRUE) {
            if (chunk.quantidade > 0) {
              webSocket.sendBIN((uint8_t*)chunk.dados, chunk.quantidade * sizeof(int16_t));
            }
          }

          Serial.printf("[DIAG] Amplitude bruta (32 bits) observada: min=%ld max=%ld\n", (long)diagMin, (long)diagMax);
          webSocket.sendTXT("stop_audio");
          estado = AGUARDANDO_RESULTADO;
        }
      }
    }
  }
  lastGravarState = leitura;
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_GRAVAR_PIN, INPUT);
  pinMode(BUTTON_PALAVRA_PIN, INPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Conectando");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[WIFI] Conectado, IP: ");
  Serial.println(WiFi.localIP());

  i2sInstalar();

  filaAudio = xQueueCreate(FILA_TAMANHO, sizeof(ChunkAudio));

  xTaskCreatePinnedToCore(tarefaCapturaAudio, "CapturaAudio", 4096, NULL, 2, &tarefaCapturaHandle, 0);

  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

// ---------- Loop (núcleo 1) ----------
void loop() {
  webSocket.loop();
  tratarBotaoPalavra();
  tratarBotaoGravar();

  ChunkAudio chunk;
  int enviosNestaIteracao = 0;
  while (enviosNestaIteracao < MAX_ENVIOS_POR_ITERACAO && xQueueReceive(filaAudio, &chunk, 0) == pdTRUE) {
    if (chunk.quantidade > 0) {
      webSocket.sendBIN((uint8_t*)chunk.dados, chunk.quantidade * sizeof(int16_t));
    }
    enviosNestaIteracao++;
  }
}