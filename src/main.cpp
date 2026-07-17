#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// ---------- Configurações de rede (AJUSTE AQUI) ----------
const char* WIFI_SSID     = "CLEUDO";
const char* WIFI_PASSWORD = "91898487";
const char* WS_HOST       = "192.168.1.106"; // IP do PC rodando backend.py
const uint16_t WS_PORT    = 8080;
const char* WS_PATH       = "/";

// ---------- Pinos do INMP441 (I2S) ----------
#define I2S_WS_PIN   25
#define I2S_SCK_PIN  32
#define I2S_SD_PIN   33
#define I2S_PORT     I2S_NUM_0

// ---------- Botão ----------
#define BUTTON_PIN   35    // pino input-only, precisa de pull externo no hardware
#define DEBOUNCE_MS  30

// ---------- Áudio ----------
#define SAMPLE_RATE       16000
#define AUDIO_SHIFT_BITS  14   // ganho da conversão 32->16 bits (mais alto = mais ganho)
#define CANAL_OFFSET      0    // 0 ou 1, veja o diagnóstico de min/max no Serial

// Quantas amostras MONO por chunk que a tarefa de captura empilha na fila.
#define CHUNK_AMOSTRAS 512
// Quantos chunks a fila comporta (margem de segurança contra soluços de rede)
#define FILA_TAMANHO 40
// Quantos chunks o loop() envia por rede em CADA iteração — limitado de
// propósito para nunca travar o loop() tempo demais e sempre voltar a
// checar o botão rapidinho.
#define MAX_ENVIOS_POR_ITERACAO 2

typedef struct {
  int16_t dados[CHUNK_AMOSTRAS];
  size_t quantidade;
} ChunkAudio;

QueueHandle_t filaAudio;
TaskHandle_t tarefaCapturaHandle;

WebSocketsClient webSocket;

volatile bool gravando = false;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// diagnóstico: amplitude mínima/máxima observada durante a gravação atual
volatile int32_t diagMin = 0;
volatile int32_t diagMax = 0;

// ---------- I2S ----------
void i2sInstalar() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // estéreo (workaround do bug do driver mono)
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 10,
    .dma_buf_len = 512,     // 10*512 frames estéreo de folga (~320ms de margem)
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
  // buffer estéreo: precisa de 2x CHUNK_AMOSTRAS pra sobrar CHUNK_AMOSTRAS
  // amostras mono depois de descartar o canal vazio
  static int32_t bufferI2S[CHUNK_AMOSTRAS * 2];

  for (;;) {
    if (gravando) {
      size_t bytesLidos = 0;
      // leitura bloqueante: essa tarefa não faz mais nada, então pode
      // esperar o I2S render os dados sem prejudicar o resto do sistema
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
        // se a fila estiver cheia (rede muito lenta), descarta o chunk
        // mais antigo pra não travar a captura -- melhor perder um
        // pedacinho já enviado do passado do que travar tudo
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
      Serial.printf("[WS] Resposta do backend: %s\n", resposta.c_str());
      break;
    }
    default:
      break;
  }
}

// ---------- Botão (push-to-talk com debounce) ----------
void tratarBotao() {
  bool leitura = digitalRead(BUTTON_PIN);

  if (leitura != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    bool pressionado = (leitura == LOW);

    if (pressionado && !gravando) {
      diagMin = INT32_MAX;
      diagMax = INT32_MIN;
      xQueueReset(filaAudio); // descarta qualquer sobra de uma gravação anterior
      gravando = true;        // só agora a tarefa de captura começa a ler o I2S
      Serial.println("[BOTAO] Gravacao iniciada");
      webSocket.sendTXT("start_audio");
    } else if (!pressionado && gravando) {
      gravando = false;
      Serial.println("[BOTAO] Gravacao finalizada, enviando stop_audio");

      // esvazia o que sobrou na fila antes do stop_audio, pra não cortar
      // o finalzinho da fala
      ChunkAudio chunk;
      while (xQueueReceive(filaAudio, &chunk, 0) == pdTRUE) {
        if (chunk.quantidade > 0) {
          webSocket.sendBIN((uint8_t*)chunk.dados, chunk.quantidade * sizeof(int16_t));
        }
      }

      Serial.printf("[DIAG] Amplitude bruta (32 bits) observada: min=%ld max=%ld\n",
                    (long)diagMin, (long)diagMax);
      webSocket.sendTXT("stop_audio");
    }
  }

  lastButtonState = leitura;
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);

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

  // tarefa de captura no núcleo 0, prioridade acima do loop() padrão (1),
  // pra ser atendida rapidamente sempre que o I2S tiver dado pronto
  xTaskCreatePinnedToCore(tarefaCapturaAudio, "CapturaAudio", 4096, NULL, 2, &tarefaCapturaHandle, 0);

  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

// ---------- Loop (núcleo 1): só botão + rede, nunca fica preso muito tempo ----------
void loop() {
  webSocket.loop();
  tratarBotao();

  ChunkAudio chunk;
  int enviosNestaIteracao = 0;
  while (enviosNestaIteracao < MAX_ENVIOS_POR_ITERACAO &&
         xQueueReceive(filaAudio, &chunk, 0) == pdTRUE) {
    if (chunk.quantidade > 0) {
      webSocket.sendBIN((uint8_t*)chunk.dados, chunk.quantidade * sizeof(int16_t));
    }
    enviosNestaIteracao++;
  }
}