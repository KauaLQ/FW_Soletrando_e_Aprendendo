#include "menu.h"
#include "../oled/oled.h"
#include "../wifi_config/wifi_config.h"
#include "../buzzer/buzzer.h"

// O nível do jogo pertence ao estado do jogo, que mora no main.cpp, essas
// duas funções são implementadas lá (perto de nivelAtual/NIVEL_MAXIMO) e só
// expõem o necessário aqui, sem vazar a variável bruta pro menu.
extern int obterNivelAtual();
extern void avancarNivelCiclico();

// ---------- Item: Informações de WiFi ----------
static void renderizarInfoWifi() {
  // Sempre lê os valores atuais na hora de desenhar -- então tanto faz se
  // mudaram há 1 segundo ou há 1 hora, a tela nunca mostra dado velho.
  String ssid = wifiConfigObterSSID();
  String senha = wifiConfigObterSenha();
  oledSetDuasLinhas(OLED_PALAVRA, "SSID: " + ssid, "Pass: " + senha);

  String ip = wifiConfigObterIPPortal().toString();
  oledSetTexto(OLED_INSTRUCAO, "Conecte: ESP32-Config - Senha: config1234 - IP: " + ip, 1, false);
}

// ---------- Item: Efeitos sonoros (silenciar/ativar o buzzer) ----------
static void renderizarEfeitosSonoros() {
  bool mudo = buzzerEstaMudo();
  oledMostrarIconeAudio(mudo);
 
  // A opção destacada com [colchetes] sempre reflete o estado atual do som:
  // "Sim" em destaque = efeitos ativos, "Nao" em destaque = efeitos mudos.
  // Apertar B alterna o estado (não há navegação entre Sim/Nao aqui).
  String linha2 = mudo ? "  Sim   [Nao]" : " [Sim]   Nao";
  oledSetDuasLinhas(OLED_INSTRUCAO, "Silenciar efeitos?", linha2);
}
 
static void selecionarEfeitosSonoros() {
  if (buzzerEstaMudo()) {
    buzzerDesmutar();
  } else {
    buzzerMutar();
  }
}
 
// ---------- Item: Alterar nível do jogo ----------
static void renderizarNivel() {
  oledSetTexto(OLED_PALAVRA, "Nivel: " + String(obterNivelAtual()), 2, true);
  oledSetTexto(OLED_INSTRUCAO, "Pressione B para mudar o nivel");
}
 
static void selecionarNivel() {
  avancarNivelCiclico();
}

// ---------- Item: Parear Seção ----------
static String pinPareamentoAtual = "----"; // PIN inicial antes da primeira geração

// Retorna o PIN atual para ser consultado/enviado via WebSocket ou API
String menuObterPinPareamento() {
  if (pinPareamentoAtual == "----") {
    // Se ainda não gerou nenhum, gera um PIN na primeira consulta
    int pin = random(0, 10000);
    char buffer[5];
    snprintf(buffer, sizeof(buffer), "%04d", pin);
    pinPareamentoAtual = String(buffer);
  }
  return pinPareamentoAtual;
}

static void renderizarParearSecao() {
  oledSetTexto(OLED_PALAVRA, pinPareamentoAtual, 2, true);
  oledSetTexto(OLED_INSTRUCAO, "Pressione B para gerar novo PIN");
}

static void selecionarParearSecao() {
  // Gera número aleatório de 0 a 9999 e formata com zeros à esquerda (%04d)
  int pin = random(0, 10000);
  char buffer[5];
  snprintf(buffer, sizeof(buffer), "%04d", pin);
  pinPareamentoAtual = String(buffer);
}

// ---------- Lista de itens do menu ----------
struct ItemMenu {
  const char *titulo;    // vai pro OLED_STATUS
  void (*renderizar)();  // desenha OLED_PALAVRA + OLED_INSTRUCAO
  void (*selecionar)();  // ação ao apertar SELECT (nullptr = sem ação)
};

// Pra adicionar uma opção nova no futuro (silenciar buzzer, URL do servidor,
// etc.): crie a função "renderizarX" (e "selecionarX" se precisar de ação)
// acima, e acrescente uma linha aqui. Nada mais precisa mudar.
static const ItemMenu ITENS_MENU[] = {
  { "Config WiFi", renderizarInfoWifi, nullptr },
  { "Efeitos Sonoros", renderizarEfeitosSonoros, selecionarEfeitosSonoros },
  { "Alterar nivel do jogo", renderizarNivel, selecionarNivel },
  { "Parear servidor", renderizarParearSecao, selecionarParearSecao }
};
static const int NUM_ITENS_MENU = sizeof(ITENS_MENU) / sizeof(ITENS_MENU[0]);

static int indiceAtual = 0;

// Rastreia a versão das credenciais de WiFi que já está refletida na tela,
// pra saber quando precisa redesenhar (ver comentário em menuTick()).
static uint32_t ultimaVersaoWifiVista = 0;

static void desenharItemAtual() {
  oledIniciarTelaJogo(); // remonta os separadores do zero, como qualquer outra tela do jogo
  oledSetTexto(OLED_STATUS, ITENS_MENU[indiceAtual].titulo);
  ITENS_MENU[indiceAtual].renderizar();
}

void menuEntrar() {
  indiceAtual = 0;
  ultimaVersaoWifiVista = wifiConfigObterVersao(); // sincroniza pra não redesenhar à toa logo em seguida
  desenharItemAtual();
}

void menuNavegarCima() {
  indiceAtual = (indiceAtual - 1 + NUM_ITENS_MENU) % NUM_ITENS_MENU;
  desenharItemAtual();
}

void menuNavegarBaixo() {
  indiceAtual = (indiceAtual + 1) % NUM_ITENS_MENU;
  desenharItemAtual();
}

void menuSelecionar() {
  if (ITENS_MENU[indiceAtual].selecionar != nullptr) {
    ITENS_MENU[indiceAtual].selecionar();
    desenharItemAtual(); // reflete o efeito da seleção na tela imediatamente
  }
}

void menuTick() {
  // Hoje só o item de WiFi depende de algo que muda por fora (o portal web),
  // mas o padrão serve pra qualquer item futuro que precise do mesmo
  // comportamento: comparar uma "versão" e redesenhar se ela mudou.
  uint32_t versaoAtual = wifiConfigObterVersao();
  if (versaoAtual != ultimaVersaoWifiVista) {
    ultimaVersaoWifiVista = versaoAtual;
    desenharItemAtual();
  }
}