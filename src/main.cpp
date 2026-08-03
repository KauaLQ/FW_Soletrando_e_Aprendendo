#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <Adafruit_SSD1306.h>
#include <MD_Parola.h>
#include "modules/buzzer/buzzer.h"
#include "modules/matrix/matrix.h"
#include "modules/oled/oled.h"
#include "modules/wifi_config/wifi_config.h"
#include "modules/menu/menu.h"
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// ---------- Configurações de rede ----------
// Usadas só na primeiríssima vez que o dispositivo liga (antes de qualquer
// configuração feita pelo portal web). Depois disso, as credenciais reais
// moram na NVS e são gerenciadas pelo módulo wifi_config.
const char* WIFI_SSID_PADRAO  = "KAUA_LQ";
const char* WIFI_SENHA_PADRAO = "12345678";
const char* WS_HOST       = "192.168.1.105"; // IP do PC rodando backend.py
const uint16_t WS_PORT    = 8080;
const char* WS_PATH       = "/";

// ---------- Display OLED ----------
#define LARGURA_TELA 128 
#define ALTURA_TELA 64
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
#define BUTTON_CONFIG_PIN    36   // abre o menu de configurações / navega para CIMA dentro dele
#define BUTTON_CANCEL_PIN    39   // só tem ação dentro do menu: VOLTAR / SAIR
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

// Usadas pelo menu de configurações (item "Alterar nivel do jogo")
int obterNivelAtual() {
  return nivelAtual;
}
 
void avancarNivelCiclico() {
  nivelAtual++;
  if (nivelAtual > NIVEL_MAXIMO) nivelAtual = NIVEL_INICIAL;
}

// ---------- Estado da conexão WiFi (independente da conexão com o backend) ----------
// A queda do backend (WStype_DISCONNECTED) já é tratada pelo webSocketEvent.
// Aqui cuidamos especificamente da queda do WiFi em si, com sua própria tela
// e reconexão automática não-bloqueante.
#define INTERVALO_RECONEXAO_WIFI_MS 5000
#define INTERVALO_PONTOS_WIFI_MS    400
bool wifiEstavaConectado = false; // valor real é definido no setup(), após a tentativa de boot
bool wifiNuncaConectado = false;  // flag que serve para atualizar corretamente o display dentro da função tratarWifi()
unsigned long ultimaTentativaReconexaoWifi = 0;
unsigned long ultimoPontoWifi = 0;
String pontosWifi = "";

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
  AGUARDANDO_RESULTADO,         // já mandamos stop_audio, esperando feedback
  CONFIGURACAO                  // usuário navegando o menu de config. tem prioridade sobre qualquer outra tela
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

volatile bool estadoConfirmadoConfig = HIGH;
bool lastConfigState = HIGH;
unsigned long lastDebounceConfigTime = 0;

volatile bool estadoConfirmadoCancel = HIGH;
bool lastCancelState = HIGH;
unsigned long lastDebounceCancelTime = 0;

// ---------- Tempo de memorização por nível (ms) ----------
#define TEMPO_MEMORIZACAO_NIVEL1 10000
#define TEMPO_MEMORIZACAO_NIVEL2 5000
#define TEMPO_MEMORIZACAO_NIVEL3 3000

unsigned long inicioMemorizacao = 0;
unsigned long duracaoMemorizacao = 0;
int ultimoSegundoMostrado = -1; // pra só redesenhar quando o segundo mudar

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
      if (estado != CONFIGURACAO) {
        oledMostrarMensagemCheia("Conectado ao backend", "Aperte A para", "Iniciar/Continuar");
      }
      break;
    case WStype_DISCONNECTED:
      // Se o usuário estiver no menu de configurações, essa tela tem
      // prioridade: não reseta o estado do jogo nem desenha por cima dela.
      // A reconexão do backend continua acontecendo normalmente por trás;
      // quando o jogador sair do menu, ele já encontra tudo em dia.
      if (estado != CONFIGURACAO) {
        estado = AGUARDANDO_PEDIDO_PALAVRA; // reseta a máquina de estados para evitar que o jogo fique travado em um "estado fantasma"
      }
      singleNoteBuzzer(REST, 100);          // Desliga o buzzer a qualquer custo
      matrixClear(matrix);                  // Apaga a matriz ao perder a conexão com o backend
      if (WiFi.status() == WL_CONNECTED && estado != CONFIGURACAO) {  // Se o WiFi também caiu, a tela específica de WiFi já está sendo exibida
        oledMostrarMensagemCheia("Aguardando backend");
      }
      break;
    case WStype_TEXT: {
      String resposta = String((char*)payload).substring(0, length);
      // O texto recebido significa coisas diferentes dependendo do que estávamos esperando
      switch (estado) {
        case AGUARDANDO_RESPOSTA_PALAVRA:
          palavraAtual = resposta;
          oledIniciarTelaJogo(); // remonta o layout de jogo (separadores) do zero
          oledSetTexto(OLED_STATUS, "Nivel " + String(nivelAtual));
          // Palavra centralizada em texto grande; se for maior que a tela,
          // o próprio módulo ativa o scroll automático em vez de vazar.
          oledSetTexto(OLED_PALAVRA, palavraAtual, 2, true);
          oledSetTexto(OLED_INSTRUCAO, "Memorize a palavra!");
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
            oledSetTexto(OLED_PALAVRA, "Acertou!", 2, true);
            if (nivelAtual >= NIVEL_MAXIMO) {
              // fechou o ciclo: acertou no nível máximo -> recomeça do 1
              startBuzzerSong(2);
              oledSetTexto(OLED_STATUS, "Jogo concluido!");
              nivelAtual = NIVEL_INICIAL;
              matrixStartScroll("PARABENS!", matrix, PA_SCROLL_LEFT, 80);
            } else {
              singleNoteBuzzer(NOTE_A5, 600);
              nivelAtual++;
              oledSetTexto(OLED_STATUS, "Proximo nivel: " + String(nivelAtual));
              matrixStartAnimation(MATRIX_ANIM_CHECK, matrix); // Mostra a animação de acerto
            }
          } else {
            singleNoteBuzzer(NOTE_C4, 600);
            oledSetTexto(OLED_STATUS, "Nivel resetado");
            // Duas linhas curtas; se algum nome vier grande demais, é truncado com "..."
            oledSetDuasLinhas(OLED_PALAVRA, "Palavra: " + palavraAtual, "Resposta: " + resposta);
            nivelAtual = NIVEL_INICIAL;
            matrixStartAnimation(MATRIX_ANIM_CROSS, matrix); // Mostra a animação de erro
          }
          oledSetTexto(OLED_INSTRUCAO, "Aperte A para pedir");
          estado = AGUARDANDO_PEDIDO_PALAVRA;
          break;

        default:
          if (estado != CONFIGURACAO) {
            oledSetTexto(OLED_PALAVRA, "ERRO", 2, true);
          }
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

// Tela padrão de "esperando o jogador pedir uma palavra". Usada ao sair do
// menu de configurações, já que a tela do menu ocupa a tela inteira e some
// com o que estava desenhado antes.
void mostrarTelaAguardandoPedido() {
  oledIniciarTelaJogo();
  oledSetTexto(OLED_STATUS, "Nivel " + String(nivelAtual));
  oledSetTexto(OLED_PALAVRA, "Pronto?", 2, true);
  oledSetTexto(OLED_INSTRUCAO, "Aperte A para pedir");
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
        if (estado == CONFIGURACAO) {
          singleNoteBuzzer(NOTE_D5, 300);
          menuNavegarBaixo();
        }
        else if (wifiNuncaConectado) {
          // Evita que, de alguma forma por bug do webSocket, o buzzer ou o oled
          // Seja acionado em um momento inconveniente
        }
        // Verifica, primeiramente, se o server está conectado
        else if(!webSocket.isConnected() || estado == MEMORIZANDO_PALAVRA){
          singleNoteBuzzer(NOTE_C4, 300);
          oledSetDuasLinhas(OLED_INSTRUCAO, "Botao indisponivel", "Por favor, aguarde.");
        } 
        // só permite pedir palavra nova se não estivermos no meio de uma gravação ou já esperando resposta
        else if (estado == AGUARDANDO_PEDIDO_PALAVRA) {
          // Serial.printf("[BOTAO] Pedindo palavra (nivel %d)...\n", nivelAtual);
          webSocket.sendTXT(("pedir_palavra " + String(nivelAtual)).c_str());
          estado = AGUARDANDO_RESPOSTA_PALAVRA;
        } else {
          singleNoteBuzzer(NOTE_C4, 300);
          oledSetDuasLinhas(OLED_INSTRUCAO, "Botao indisponivel", "Segure B para gravar");
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
          if (estado == CONFIGURACAO) {
            singleNoteBuzzer(NOTE_E5, 300);
            menuSelecionar();
          }
          else if (wifiNuncaConectado) {
            // Evita que, de alguma forma por bug do webSocket, o buzzer ou o oled
            // Seja acionado em um momento inconveniente
          }
          // Verifica, primeiramente, se o server está conectado
          else if(!webSocket.isConnected() || estado == MEMORIZANDO_PALAVRA){
            singleNoteBuzzer(NOTE_C4, 300);
            oledSetDuasLinhas(OLED_INSTRUCAO, "Botao indisponivel", "Por favor, aguarde.");
          } else if (estado != PALAVRA_PRONTA) {
            singleNoteBuzzer(NOTE_C4, 300);
            oledSetDuasLinhas(OLED_INSTRUCAO, "Botao indisponivel", "Aperte A para pedir");
          } else {
            diagMin = INT32_MAX;
            diagMax = INT32_MIN;
            xQueueReset(filaAudio);
            gravando = true;
            estado = GRAVANDO;
            matrixClear(matrix); // Apaga a matriz ao pressionar o botão B para gravar
            oledSetDuasLinhas(OLED_INSTRUCAO, "Gravando: Solte B", "para finalizar");
            webSocket.sendTXT("start_audio");
          }
        }
      } else if (estadoConfirmadoGravar == HIGH) { // Acabou de SOLTAR o botão (Transição para HIGH)
        if (gravando) {
          gravando = false;
          oledSetDuasLinhas(OLED_INSTRUCAO, "Gravacao finalizada", "Aguardando resultado");

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

// ---------- Botão de menu (IO36): abre o menu / navega para CIMA dentro dele ----------
void tratarBotaoConfig() {
  bool leitura = digitalRead(BUTTON_CONFIG_PIN);
  if (leitura != lastConfigState) {
    lastDebounceConfigTime = millis();
  }
  if ((millis() - lastDebounceConfigTime) > DEBOUNCE_MS) {
    if (leitura != estadoConfirmadoConfig) {
      estadoConfirmadoConfig = leitura;
      if (estadoConfirmadoConfig == LOW) {
        if (estado == CONFIGURACAO) {
          singleNoteBuzzer(NOTE_D5, 300);
          menuNavegarCima();
        } else if (estado == AGUARDANDO_PEDIDO_PALAVRA) {
          // Só pode abrir o menu se não houver nada em andamento (regra de entrada)
          estado = CONFIGURACAO;
          singleNoteBuzzer(NOTE_G3, 300);
          menuEntrar();
        }
        // Em qualquer outro estado, o botão CONFIG simplesmente não faz nada
      }
    }
  }
  lastConfigState = leitura;
}

// ---------- Botão de cancelar (IO39): só tem ação dentro do menu ----------
void tratarBotaoCancel() {
  bool leitura = digitalRead(BUTTON_CANCEL_PIN);
  if (leitura != lastCancelState) {
    lastDebounceCancelTime = millis();
  }
  if ((millis() - lastDebounceCancelTime) > DEBOUNCE_MS) {
    if (leitura != estadoConfirmadoCancel) {
      estadoConfirmadoCancel = leitura;
      if (estadoConfirmadoCancel == LOW && estado == CONFIGURACAO) {
        estado = AGUARDANDO_PEDIDO_PALAVRA;
        singleNoteBuzzer(NOTE_G3, 300);
        mostrarTelaAguardandoPedido();
      }
      // Fora do menu, CANCEL não tem ação nenhuma
    }
  }
  lastCancelState = leitura;
}

// ---------- Timer de memorização (IO35 continua bloqueado aqui) ----------
void tratarMemorizacao() {
  if (estado != MEMORIZANDO_PALAVRA) return;

  unsigned long decorrido = millis() - inicioMemorizacao;

  if (decorrido >= duracaoMemorizacao) {
    // Tempo acabou: some com a palavra e libera a gravação
    oledSetTexto(OLED_PALAVRA, "Pronto?", 2, true);
    oledSetTexto(OLED_INSTRUCAO, "Segure B para gravar");
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
  matrixTick(matrix);

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

// ---------- WiFi: detecta queda e reconecta sozinho, sem travar o loop ----------
void tratarWifi() {
  // Credenciais novas acabaram de ser salvas pelo portal web: tenta conectar
  // com elas imediatamente, em vez de esperar o próximo ciclo de retry.
  static uint32_t ultimaVersaoWifiVista = 0;
  uint32_t versaoAtual = wifiConfigObterVersao();
  if (versaoAtual != ultimaVersaoWifiVista) {
    ultimaVersaoWifiVista = versaoAtual;
    WiFi.begin(wifiConfigObterSSID().c_str(), wifiConfigObterSenha().c_str());
    ultimaTentativaReconexaoWifi = millis();
  }

  bool conectado = (WiFi.status() == WL_CONNECTED);

  if (conectado && !wifiEstavaConectado) {
    // Acabou de voltar. O jogo em si só volta a responder quando o
    // webSocket reconectar sozinho (ele já tem retry automático), então só
    // avisamos que o WiFi voltou e deixamos o webSocketEvent cuidar do resto.
    wifiNuncaConectado = false; // Se ele já conseguiu conectar uma vez, então a tela de boot já pode sair
    wifiEstavaConectado = true;
    if (estado != CONFIGURACAO) {
      oledMostrarMensagemCheia("WiFi reconectado!", "Conectando ao", "servidor...");
    }
    return;
  }

  if (!conectado && wifiEstavaConectado && !wifiNuncaConectado) {
    // Acabou de cair: reseta o jogo (igual fazemos quando o backend cai) e
    // mostra a tela específica de queda de WiFi (a menos que o usuário
    // esteja no menu de config, que tem prioridade sobre essa tela).
    wifiEstavaConectado = false;
    if (estado != CONFIGURACAO) {
      estado = AGUARDANDO_PEDIDO_PALAVRA;
    }
    singleNoteBuzzer(REST, 100);
    matrixClear(matrix);
    pontosWifi = "";
    ultimoPontoWifi = millis();
    ultimaTentativaReconexaoWifi = millis();
    if (estado != CONFIGURACAO) {
      oledMostrarMensagemCheia("WiFi desconectado", "Reconectando");
    }
    return;
  }

  if (!conectado && !wifiNuncaConectado) {
    // Pontinhos animados de "Reconectando...", só atualiza o corpo da
    // mensagem (nunca redesenha o título), então nunca sobra "lixo" na tela
    if (millis() - ultimoPontoWifi >= INTERVALO_PONTOS_WIFI_MS) {
      ultimoPontoWifi = millis();
      pontosWifi += ".";
      if (pontosWifi.length() > 3) pontosWifi = "";
      if (estado != CONFIGURACAO) {
        oledAtualizarCorpoMensagemCheia("Reconectando" + pontosWifi);
      }
    }

    // Tenta reconectar periodicamente, sem bloquear o resto do loop
    if (millis() - ultimaTentativaReconexaoWifi >= INTERVALO_RECONEXAO_WIFI_MS) {
      ultimaTentativaReconexaoWifi = millis();
      WiFi.reconnect();
    }
  }
}


void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_GRAVAR_PIN, INPUT);
  pinMode(BUTTON_PALAVRA_PIN, INPUT);
  pinMode(BUTTON_CONFIG_PIN, INPUT);
  pinMode(BUTTON_CANCEL_PIN, INPUT);

  // Inicializa o barramento SPI com os pinos customizados do ESP32 (HSPI) e a Matriz LED
  hspi.begin(CLK_PIN, -1, DATA_PIN, CS_PIN); // SCK, MISO (não usado), MOSI, SS
  matrixInit(matrix, 1);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Inicializa o display I2C no endereço 0x3C 
    Serial.println(F("Falha ao inicializar o display SSD1306. Verifique as conexões!"));
    for(;;); // Trava o programa se houver erro
  }

  oledInit(display);
  oledMostrarBoot(128, 40, "Conectando WiFi");
  wifiConfigInit(WIFI_SSID_PADRAO, WIFI_SENHA_PADRAO); // Sobe o AP fixo + portal de configuração
  WiFi.setAutoReconnect(true); // ajuda o próprio driver a já tentar sozinho
  WiFi.begin(wifiConfigObterSSID().c_str(), wifiConfigObterSenha().c_str());

  String pontos = "";
  unsigned long ultimoPontoBoot = millis();
  unsigned long inicioTentativaWifi = millis();
  const unsigned long TIMEOUT_WIFI_BOOT_MS = 20000; // não trava pra sempre se as credenciais estiverem erradas

  while (WiFi.status() != WL_CONNECTED && millis() - inicioTentativaWifi < TIMEOUT_WIFI_BOOT_MS) {
    wifiConfigTick(); // mantém o portal respondendo mesmo durante essa espera
    if (millis() - ultimoPontoBoot >= 300) {
      ultimoPontoBoot = millis();
      pontos += ".";
      if (pontos.length() > 3) pontos = "";
      // Rodapé é sempre limpo por completo antes de escrever, então os pontos
      // nunca deixam "resto" na tela, independente de quantos forem
      oledAtualizarRodapeBoot("Conectando WiFi" + pontos);
    }
    delay(10); // laço curto só pra não saturar a CPU enquanto atende o portal
  }

  if (WiFi.status() == WL_CONNECTED) {
    oledAtualizarRodapeBoot("Conectado!", "IP:" + WiFi.localIP().toString());
  } else {
    // Não conectou dentro do tempo
    wifiNuncaConectado = true;
    oledAtualizarRodapeBoot("Sem WiFi ainda", "Config: " + wifiConfigObterIPPortal().toString());
  }
  wifiEstavaConectado = (WiFi.status() == WL_CONNECTED);

  i2sInstalar();

  filaAudio = xQueueCreate(FILA_TAMANHO, sizeof(ChunkAudio));

  xTaskCreatePinnedToCore(tarefaCapturaAudio, "CapturaAudio", 4096, NULL, 2, &tarefaCapturaHandle, 0);

  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  initBuzzer(); // Inicializa o módulo do buzzer
}

// ---------- Loop (núcleo 1) ----------
void loop() {
  webSocket.loop();
  updateBuzzerTick(); // Atualiza o buzzer (essencial rodar o tempo todo livremente)
  tratarBotaoPalavra();
  tratarBotaoGravar();
  tratarBotaoConfig();
  tratarBotaoCancel();
  tratarMemorizacao();
  tratarMatrix();
  tratarWifi();
  wifiConfigTick(); // atende requisições da página de configuração de WiFi
  if (estado == CONFIGURACAO) {
    menuTick(); // detecta mudanças (ex.: WiFi reconfigurado) e redesenha a tela do menu na hora
  }
  oledTick(); // avança o scroll de textos grandes, se houver algum ativo

  ChunkAudio chunk;
  int enviosNestaIteracao = 0;
  while (enviosNestaIteracao < MAX_ENVIOS_POR_ITERACAO && xQueueReceive(filaAudio, &chunk, 0) == pdTRUE) {
    if (chunk.quantidade > 0) {
      webSocket.sendBIN((uint8_t*)chunk.dados, chunk.quantidade * sizeof(int16_t));
    }
    enviosNestaIteracao++;
  }
}