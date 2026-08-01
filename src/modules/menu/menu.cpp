#include "menu.h"
#include "../oled/oled.h"
#include "../wifi_config/wifi_config.h"

// ---------- Item: Informações de WiFi ----------
static void renderizarInfoWifi() {
  // Sempre lê os valores atuais na hora de desenhar -- então tanto faz se
  // mudaram há 1 segundo ou há 1 hora, a tela nunca mostra dado velho.
  String ssid = wifiConfigObterSSID();
  String senha = wifiConfigObterSenha();
  oledSetDuasLinhas(OLED_PALAVRA, "SSID: " + ssid, "Pass: " + senha);

  String ip = wifiConfigObterIPPortal().toString();
  oledSetDuasLinhas(OLED_INSTRUCAO, "Acesse " + ip, "p/ alterar   ^ v");
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