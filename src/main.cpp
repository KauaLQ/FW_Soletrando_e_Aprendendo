#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <Adafruit_SSD1306.h>
#include <MD_Parola.h>
#include "img.h"
#include "modules/buzzer/buzzer.h"
#include "modules/matrix/matrix.h"
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// ---------- Configurações de rede ----------
const char* WIFI_SSID     = "KAUA_LQ";
const char* WIFI_PASSWORD = "12345678";
const char* WS_HOST       = "192.168.1.104"; // IP do PC rodando backend.py
const uint16_t WS_PORT    = 8080;
const char* WS_PATH       = "/";

// ---------- Display OLED ----------
#define LARGURA_TELA 128 
#define ALTURA_TELA 64
// Declara o objeto do display (-1 significa que o display não tem pino de RESET)
Adafruit_SSD1306 display(LARGURA_TELA, ALTURA_TELA, &Wire, -1);

// ---------- Configurações da Matriz LED ----------
#define DATA_PIN  13
#define CLK_PIN   14
#define CS_PIN    15
#define HARDWARE_TYPE MD_MAX72XX::GENERIC_HW
#define MAX_DEVICES 1
SPIClass hspi(HSPI);
MD_Parola matrix = MD_Parola(HARDWARE_TYPE, hspi, CS_PIN, MAX_DEVICES);
// Controlam o pisca-pisca do número zero na matriz
unsigned long ultimoPiscaMatriz = 0;
bool estadoLedPisca = true;

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

// ---------- Progressão de níveis ----------
// Começa em 1, sobe a cada acerto, reseta para 1 se errar OU se acertar
// no nível máximo (fecha o ciclo e recomeça do 1).
#define NIVEL_INICIAL 1
#define NIVEL_MAXIMO  3
int nivelAtual = NIVEL_INICIAL;

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
  MEMORIZANDO_PALAVRA,          // palavra na tela, contando tempo pra decorar
  PALAVRA_PRONTA,               // tempo acabou, IO35 liberado pra gravar
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

// ---------- Tempo de memorização por nível (ms) ----------
#define TEMPO_MEMORIZACAO_NIVEL1 10000
#define TEMPO_MEMORIZACAO_NIVEL2 5000
#define TEMPO_MEMORIZACAO_NIVEL3 3000

unsigned long inicioMemorizacao = 0;
unsigned long duracaoMemorizacao = 0;
int ultimoSegundoMostrado = -1; // pra só redesenhar quando o segundo mudar

// ---------- Controle não-bloqueante da animação de resultado (acerto/erro) ----------
#define INTERVALO_FRAME_RESULTADO  150   // velocidade de troca de frames

unsigned long obterTempoMemorizacao(int nivel) {
  switch (nivel) {
    case 1: return TEMPO_MEMORIZACAO_NIVEL1;
    case 2: return TEMPO_MEMORIZACAO_NIVEL2;
    case 3: return TEMPO_MEMORIZACAO_NIVEL3;
    default: return TEMPO_MEMORIZACAO_NIVEL1;
  }
}

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
      matrixStartAnimation(MATRIX_ANIM_ARROW, matrix);
      startBuzzerSong(1);  // Inicia a música uma vez
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Conectado ao backend");
      display.setCursor(0, 10);
      display.drawFastHLine(0, 10, LARGURA_TELA, SSD1306_WHITE);
      display.setCursor(0, 15);
      display.print("Aperte A para");
      display.setCursor(0, 25);
      display.print("Iniciar/Continuar");
      display.drawFastHLine(0, 35, LARGURA_TELA, SSD1306_WHITE);
      display.display();
      break;
    case WStype_DISCONNECTED:
      matrixClear(matrix); // Apaga a matriz ao perder a conexão com o backend
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Aguardando backend");
      display.setCursor(0, 10);
      display.drawFastHLine(0, 10, LARGURA_TELA, SSD1306_WHITE);
      display.display();
      break;
    case WStype_TEXT: {
      String resposta = String((char*)payload).substring(0, length);
      // O texto recebido significa coisas diferentes dependendo do que estávamos esperando
      switch (estado) {
        case AGUARDANDO_RESPOSTA_PALAVRA:
          palavraAtual = resposta;
          display.fillRect(0, 0, LARGURA_TELA, 10, SSD1306_BLACK); // Limpa a linha de status
          display.fillRect(0, 15, LARGURA_TELA, 20, SSD1306_BLACK); // Limpa a linha da palavra
          display.setCursor(0, 0);
          display.printf("Nivel %d", nivelAtual);
          display.setTextSize(2);
          display.setCursor(0, 15);
          display.printf("%s", palavraAtual.c_str());
          display.setTextSize(1);
          display.drawFastHLine(0, 35, LARGURA_TELA, SSD1306_WHITE);
          display.fillRect(0, 40, LARGURA_TELA, 24, SSD1306_BLACK); // Limpa a linha de instrução
          display.setCursor(0, 40);
          display.print("Memorize a palavra!");
          display.display();
          // Inicia o timer de memorização (não bloqueante)
          duracaoMemorizacao = obterTempoMemorizacao(nivelAtual);
          inicioMemorizacao = millis();
          ultimoSegundoMostrado = -1;
          estado = MEMORIZANDO_PALAVRA;
          break;

        case AGUARDANDO_RESULTADO:
          // Debug: mostra a resposta do backend no console. Descomente caso necessário.
          // Serial.printf("[JOGO] Resultado do backend: %s\n", resposta.c_str());
          if (resposta == palavraAtual) {
            display.fillRect(0, 15, LARGURA_TELA, 20, SSD1306_BLACK); // Limpa a linha da palavra
            display.setTextSize(2);
            display.setCursor(0, 15);
            display.printf("Acertou!");
            display.setTextSize(1);
            display.display();
            matrixStartAnimation(MATRIX_ANIM_CHECK, matrix); // Mostra a animação de acerto
            if (nivelAtual >= NIVEL_MAXIMO) {
              // fechou o ciclo: acertou no nível máximo -> recomeça do 1
              startBuzzerSong(2);
              display.fillRect(0, 0, LARGURA_TELA, 10, SSD1306_BLACK); // Limpa a linha de status
              display.setCursor(0, 0);
              display.print("Jogo concluido!");
              display.display();
              nivelAtual = NIVEL_INICIAL;
            } else {
              singleNoteBuzzer(NOTE_A5, 600);
              nivelAtual++;
              display.fillRect(0, 0, LARGURA_TELA, 10, SSD1306_BLACK); // Limpa a linha de status
              display.setCursor(0, 0);
              display.printf("Proximo nivel: %d", nivelAtual);
              display.display();
            }
          } else {
            singleNoteBuzzer(NOTE_C4, 600);
            display.fillRect(0, 0, LARGURA_TELA, 10, SSD1306_BLACK); // Limpa a linha de status
            display.fillRect(0, 15, LARGURA_TELA, 20, SSD1306_BLACK); // Limpa a linha da palavra
            display.setCursor(0, 0);
            display.print("Nivel resetado");
            display.setCursor(0, 15);
            display.printf("Palavra: %s", palavraAtual.c_str());
            display.setCursor(0, 25);
            display.printf("Resposta: %s", resposta.c_str());
            display.display();
            nivelAtual = NIVEL_INICIAL;
            matrixStartAnimation(MATRIX_ANIM_CROSS, matrix); // Mostra a animação de erro
          }
          display.fillRect(0, 40, LARGURA_TELA, 24, SSD1306_BLACK); // Limpa a linha de instrução
          display.setCursor(0, 40);
          display.print("Aperte A para pedir");
          display.display();
          estado = AGUARDANDO_PEDIDO_PALAVRA;
          break;

        default:
          display.fillRect(0, 15, LARGURA_TELA, 20, SSD1306_BLACK); // Limpa a linha da palavra
          display.setTextSize(2);
          display.setCursor(0, 15);
          display.printf("ERRO");
          display.setTextSize(1);
          display.display();
          // Debug: mostra a resposta do backend no console. Descomente caso necessário.
          // Serial.printf("[WS] Resposta inesperada (estado atual nao esperava texto): %s\n", resposta.c_str());
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
          // Serial.printf("[BOTAO] Pedindo palavra (nivel %d)...\n", nivelAtual);
          webSocket.sendTXT(("pedir_palavra " + String(nivelAtual)).c_str());
          estado = AGUARDANDO_RESPOSTA_PALAVRA;
        } else {
          singleNoteBuzzer(NOTE_C4, 300);
          display.fillRect(0, 40, LARGURA_TELA, 24, SSD1306_BLACK); // Limpa a linha de instrução
          display.setCursor(0, 40);
          display.print("Botao indisponivel");
          display.setCursor(0, 50);
          display.print("Segure B para gravar");
          display.display();
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
            singleNoteBuzzer(NOTE_C4, 300);
            display.fillRect(0, 40, LARGURA_TELA, 24, SSD1306_BLACK); // Limpa a linha de instrução
            display.setCursor(0, 40);
            display.print("Botao indisponivel");
            display.setCursor(0, 50);
            display.print("Aperte A para pedir");
            display.display();
          } else {
            diagMin = INT32_MAX;
            diagMax = INT32_MIN;
            xQueueReset(filaAudio);
            gravando = true;
            estado = GRAVANDO;
            matrixClear(matrix); // Apaga a matriz ao pressionar o botão B para gravar
            display.fillRect(0, 40, LARGURA_TELA, 24, SSD1306_BLACK); // Limpa a linha de instrução
            display.setCursor(0, 40);
            display.print("Gravando: Solte B");
            display.setCursor(0, 50);
            display.print("para finalizar");
            display.display();
            webSocket.sendTXT("start_audio");
          }
        }
      } else if (estadoConfirmadoGravar == HIGH) { // Acabou de SOLTAR o botão (Transição para HIGH)
        if (gravando) {
          gravando = false;
          display.fillRect(0, 40, LARGURA_TELA, 24, SSD1306_BLACK); // Limpa a linha de instrução
          display.setCursor(0, 40);
          display.print("Gravacao finalizada");
          display.setCursor(0, 50);
          display.print("Aguardando resultado");
          display.display();

          ChunkAudio chunk;
          while (xQueueReceive(filaAudio, &chunk, 0) == pdTRUE) {
            if (chunk.quantidade > 0) {
              webSocket.sendBIN((uint8_t*)chunk.dados, chunk.quantidade * sizeof(int16_t));
            }
          }

          // Debug: mostra a amplitude bruta observada durante a gravação. Descomente caso necessário.
          // Serial.printf("[DIAG] Amplitude bruta (32 bits) observada: min=%ld max=%ld\n", (long)diagMin, (long)diagMax);
          webSocket.sendTXT("stop_audio");
          estado = AGUARDANDO_RESULTADO;
        }
      }
    }
  }
  lastGravarState = leitura;
}

// ---------- Timer de memorização (IO35 continua bloqueado aqui) ----------
void tratarMemorizacao() {
  if (estado != MEMORIZANDO_PALAVRA) return;

  unsigned long decorrido = millis() - inicioMemorizacao;

  if (decorrido >= duracaoMemorizacao) {
    // Tempo acabou: some com a palavra e libera a gravação
    display.fillRect(0, 15, LARGURA_TELA, 20, SSD1306_BLACK); // apaga a palavra
    display.setTextSize(2);
    display.setCursor(0, 15);
    display.printf("Pronto?");
    display.setTextSize(1);
    display.fillRect(0, 40, LARGURA_TELA, 24, SSD1306_BLACK);
    display.setCursor(0, 40);
    display.print("Segure B para gravar");
    display.display();
    // Configura o estado inicial do pisca-pisca antes de mudar de estado
    estadoLedPisca = true;
    ultimoPiscaMatriz = millis();
    matrixShowText("0", matrix); // Centraliza o 0 sozinho
    singleNoteBuzzer(NOTE_A4, 800);
    estado = PALAVRA_PRONTA;
    return;
  }

  // Atualiza o contador na tela só quando o segundo restante mudar
  int segundosRestantes = (duracaoMemorizacao - decorrido + 999) / 1000; // arredonda pra cima
  if (segundosRestantes != ultimoSegundoMostrado) {
    segundosRestantes > 5 ? singleNoteBuzzer(NOTE_A4, 500) : singleNoteBuzzer(NOTE_A5, 500);
    ultimoSegundoMostrado = segundosRestantes;
    // Atualiza a matriz de LED com o módulo (texto centralizado).
    // Se o número for "10", ele vai se ajustar no espaço da matriz.
    matrixShowText(String(segundosRestantes), matrix);
  }
}

void tratarMatrix(){
  // Avança os frames da animação (matrixTick só faz efeito se o módulo
  // estiver em modo animação, então é seguro chamar sempre daqui)
  matrixTick(INTERVALO_FRAME_RESULTADO, matrix);

  if (estado != PALAVRA_PRONTA) return;

  // Lógica não-bloqueante para piscar a matriz no estado PALAVRA_PRONTA
  if (estado == PALAVRA_PRONTA) {
    if (millis() - ultimoPiscaMatriz >= 500) {
      ultimoPiscaMatriz = millis();
      estadoLedPisca = !estadoLedPisca;
      
      if (estadoLedPisca) {
        matrixShowText("0", matrix);
      } else {
        matrixClear(matrix);
      }
    }
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_GRAVAR_PIN, INPUT);
  pinMode(BUTTON_PALAVRA_PIN, INPUT);

  // Inicializa o barramento SPI com os pinos customizados do ESP32 (HSPI) e a Matriz LED
  hspi.begin(CLK_PIN, -1, DATA_PIN, CS_PIN); // SCK, MISO (não usado), MOSI, SS
  matrixInit(matrix, 1);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Inicializa o display I2C no endereço 0x3C 
    Serial.println(F("Falha ao inicializar o display SSD1306. Verifique as conexões!"));
    for(;;); // Trava o programa se houver erro
  }

  display.clearDisplay();         // Limpa o buffer do display
  display.setTextColor(WHITE, BLACK);    // Define a cor do texto (Branco)
  display.drawBitmap(0, 0, bmp_logo_SA, 128, 40, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.print("Conectando WiFi");
  display.display();              // Atualiza o display com as informações acima

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int textPos = 90;
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    if (textPos + 5 > 105) {
      textPos = 90;
      display.setCursor(textPos, 45);
      display.print("          "); // Limpa os pontos anteriores
      display.display();
    }
    else {
      display.setCursor(textPos, 45);
      display.print(".");
      display.display();
      textPos += 5;
    }
  }
  display.fillRect(0, 45, LARGURA_TELA, 10, SSD1306_BLACK); // Limpa a linha de status
  display.setCursor(0, 45);
  display.print("Conectado!");
  display.setCursor(0, 55);
  display.printf("IP:%s", WiFi.localIP().toString().c_str());
  display.display();

  i2sInstalar();

  filaAudio = xQueueCreate(FILA_TAMANHO, sizeof(ChunkAudio));

  xTaskCreatePinnedToCore(tarefaCapturaAudio, "CapturaAudio", 4096, NULL, 2, &tarefaCapturaHandle, 0);

  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  initBuzzer();       // Inicializa o módulo do buzzer
}

// ---------- Loop (núcleo 1) ----------
void loop() {
  webSocket.loop();
  updateBuzzerTick(); // Atualiza o buzzer (essencial rodar o tempo todo livremente)
  tratarBotaoPalavra();
  tratarBotaoGravar();
  tratarMemorizacao();
  tratarMatrix();

  ChunkAudio chunk;
  int enviosNestaIteracao = 0;
  while (enviosNestaIteracao < MAX_ENVIOS_POR_ITERACAO && xQueueReceive(filaAudio, &chunk, 0) == pdTRUE) {
    if (chunk.quantidade > 0) {
      webSocket.sendBIN((uint8_t*)chunk.dados, chunk.quantidade * sizeof(int16_t));
    }
    enviosNestaIteracao++;
  }
}