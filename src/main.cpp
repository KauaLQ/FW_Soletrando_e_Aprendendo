#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <driver/i2s.h>

// ---------- Configurações de rede (AJUSTE AQUI) ----------
const char* WIFI_SSID     = "CLEUDO";
const char* WIFI_PASSWORD = "91898487";
const char* WS_HOST       = "192.168.1.106"; // IP do PC rodando backend.py
const uint16_t WS_PORT    = 8080;            // mesma porta passada em listen_websocket.py
const char* WS_PATH       = "/";

// ---------- Pinos do INMP441 (I2S) ----------
#define I2S_WS_PIN   25   // LRCLK / WS
#define I2S_SCK_PIN  32   // BCLK
#define I2S_SD_PIN   33   // SD (dados do microfone)
#define I2S_PORT     I2S_NUM_0

// ---------- Botão ----------
#define BUTTON_PIN   35    // pino input-only, precisa de pull externo no hardware
#define DEBOUNCE_MS  40

// ---------- Áudio ----------
#define SAMPLE_RATE   16000
#define I2S_READ_LEN  2048   // amostras de 32 bits lidas por vez (agora em modo estéreo,
                              // ou seja, 1024 "frames" mono reais por leitura)

// Quantos bits descartar ao converter a amostra de 32 bits para 16 bits.
// Se o áudio chegar muito baixo/muito alto no reconhecimento, ajuste este
// valor (ex.: 11 ou 8 = mais ganho / mais risco de clipping).
#define AUDIO_SHIFT_BITS 14

// Em modo estéreo (RIGHT_LEFT), o INMP441 entrega os frames intercalados:
// [amostra_canalA, amostra_canalB, amostra_canalA, amostra_canalB, ...]
// Um dos dois canais fica vazio (zerado), pois o mic só fala em um deles.
// CANAL_OFFSET = 0 -> usa os índices pares (0,2,4,...)
// CANAL_OFFSET = 1 -> usa os índices ímpares (1,3,5,...)
// Comece com 0. Se o áudio continuar mudo, troque para 1 (veja o
// diagnóstico de min/max no Serial para saber qual dos dois tem sinal).
#define CANAL_OFFSET 0

int32_t i2sReadBuffer[I2S_READ_LEN];
int16_t pcmSendBuffer[I2S_READ_LEN / 2];

WebSocketsClient webSocket;

bool gravando = false;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// diagnóstico: amplitude mínima/máxima observada durante a gravação atual
int32_t diagMin = 0;
int32_t diagMax = 0;

// ---------- I2S ----------
void i2sInstalar() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // <-- estéreo, ver comentário acima
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
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
  i2s_zero_dma_buffer(I2S_PORT);
}

inline int16_t converter32para16(int32_t amostra32) {
  return (int16_t)(amostra32 >> AUDIO_SHIFT_BITS);
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
      gravando = true;
      diagMin = INT32_MAX;
      diagMax = INT32_MIN;
      Serial.println("[BOTAO] Gravacao iniciada");
      i2s_zero_dma_buffer(I2S_PORT);
      webSocket.sendTXT("start_audio");
    } else if (!pressionado && gravando) {
      gravando = false;
      Serial.println("[BOTAO] Gravacao finalizada, enviando stop_audio");
      // Diagnóstico: se min/max ficarem em ~0 (tipo -3..3), o canal
      // escolhido (CANAL_OFFSET) está errado ou a fiação está com problema.
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

  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

// ---------- Loop ----------
void loop() {
  webSocket.loop();
  tratarBotao();

  if (gravando) {
    size_t bytesLidos = 0;
    i2s_read(I2S_PORT, i2sReadBuffer, sizeof(i2sReadBuffer), &bytesLidos, 0);

    if (bytesLidos > 0) {
      size_t amostrasLidas = bytesLidos / sizeof(int32_t);
      size_t out = 0;
      for (size_t i = CANAL_OFFSET; i < amostrasLidas; i += 2) {
        int32_t amostra = i2sReadBuffer[i];
        if (amostra < diagMin) diagMin = amostra;
        if (amostra > diagMax) diagMax = amostra;
        pcmSendBuffer[out++] = converter32para16(amostra);
      }
      if (out > 0) {
        webSocket.sendBIN((uint8_t*)pcmSendBuffer, out * sizeof(int16_t));
      }
    }
  }
}