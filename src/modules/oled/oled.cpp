#include "oled.h"
#include "img.h"

static Adafruit_SSD1306 *tela = nullptr;        // Referência pro display já criado no main.cpp
struct RetanguloRegiao { int16_t x, y, w, h; }; // Geometria fixa de cada região (x, y, largura, altura)

static const RetanguloRegiao REGIOES[OLED_NUM_REGIOES] = {
  /* OLED_STATUS    */ { 0,  0, 128, 10 },
  /* OLED_PALAVRA   */ { 0, 15, 128, 20 },
  /* OLED_INSTRUCAO */ { 0, 40, 128, 24 },
};

static const int16_t Y_SEPARADOR_1  = 10; // entre STATUS e PALAVRA
static const int16_t Y_SEPARADOR_2  = 35; // entre PALAVRA e INSTRUCAO
static const int16_t RODAPE_BOOT_Y  = 45; // linha de rodapé usada na tela de boot
static const int16_t RODAPE_BOOT_H  = 19; // até o fim da tela (64 - 45)
static const int16_t CORPO_MENSAGEM_Y = 15; // corpo da "mensagem cheia" (abaixo do título/separador)
static const int16_t CORPO_MENSAGEM_H = 49; // até o fim da tela (64 - 15)

// ---------- Estado de scroll não-bloqueante (um slot por região) ----------
struct ScrollRegiao {
  bool ativo = false;
  String texto;
  uint8_t tamanho = 1;
  int16_t larguraTexto = 0;
  int16_t posX = 0;
  unsigned long ultimoPasso = 0;
};
static ScrollRegiao scrolls[OLED_NUM_REGIOES];

static const unsigned long INTERVALO_SCROLL_MS = 40; // velocidade do marquee
static const int16_t PASSO_SCROLL_PX = 2;
static const int16_t ESPACO_ENTRE_VOLTAS = 20; // "respiro" antes de repetir

// ---------- Helpers internos ----------
static void limparRetangulo(const RetanguloRegiao &r) {
  tela->fillRect(r.x, r.y, r.w, r.h, SSD1306_BLACK);
}

static int16_t medirLargura(const String &texto, uint8_t tamanho) {
  int16_t x1, y1;
  uint16_t w, h;
  tela->setTextSize(tamanho);
  tela->getTextBounds(texto, 0, 0, &x1, &y1, &w, &h);
  return (int16_t)w;
}

static void pararScroll(OledRegiao regiao) {
  scrolls[regiao].ativo = false;
}

static void zerarTodosScrolls() {
  for (int i = 0; i < OLED_NUM_REGIOES; i++) scrolls[i].ativo = false;
}

static String truncarComReticencias(const String &texto, uint8_t tamanho, int16_t larguraMax) {
  if (medirLargura(texto, tamanho) <= larguraMax) return texto;
  String cortado = texto;
  while (cortado.length() > 1 && medirLargura(cortado + "...", tamanho) > larguraMax) {
    cortado.remove(cortado.length() - 1);
  }
  return cortado + "...";
}

// ----------- Alto nível -----------
// Chamado uma vez no setup(), depois do display.begin()
void oledInit(Adafruit_SSD1306 &telaRef) {
  tela = &telaRef;
  tela->setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  tela->setTextWrap(false);
}

// Chamado sempre no loop() -- avança o scroll dos textos que não couberam
void oledTick() {
  if (!tela) return;
  bool precisaAtualizar = false;

  for (int i = 0; i < OLED_NUM_REGIOES; i++) {
    ScrollRegiao &s = scrolls[i];
    if (!s.ativo) continue;
    if (millis() - s.ultimoPasso < INTERVALO_SCROLL_MS) continue;

    s.ultimoPasso = millis();
    s.posX -= PASSO_SCROLL_PX;
    // Texto saiu completamente pela esquerda: reinicia entrando pela direita
    if (s.posX < -(s.larguraTexto)) {
      s.posX = REGIOES[i].w + ESPACO_ENTRE_VOLTAS;
    }

    limparRetangulo(REGIOES[i]);
    tela->setTextSize(s.tamanho);
    tela->setCursor(REGIOES[i].x + s.posX, REGIOES[i].y);
    tela->print(s.texto);
    precisaAtualizar = true;
  }

  if (precisaAtualizar) tela->display();
}

// Desenha os separadores entre as regiões. Chamado ao entrar na tela de jogo
void oledIniciarTelaJogo() {
  zerarTodosScrolls();
  tela->clearDisplay();
  tela->drawFastHLine(0, Y_SEPARADOR_1, tela->width(), SSD1306_WHITE);
  tela->drawFastHLine(0, Y_SEPARADOR_2, tela->width(), SSD1306_WHITE);
  tela->display();
}

// Escreve um texto numa região, sempre limpando-a antes. Se não couber,
// ativa scroll automático em vez de deixar o texto vazar.
void oledSetTexto(OledRegiao regiao, const String &texto, uint8_t tamanho, bool centralizado) {
  const RetanguloRegiao &r = REGIOES[regiao];
  pararScroll(regiao);
  limparRetangulo(r);

  int16_t w = medirLargura(texto, tamanho);
  if (w <= r.w) {
    // Cabe direitinho: desenha estático, sem risco de vazar pra fora da região
    tela->setTextSize(tamanho);
    int16_t x = r.x;
    if (centralizado && w < r.w) x = r.x + (r.w - w) / 2;
    tela->setCursor(x, r.y);
    tela->print(texto);
  } else {
    // Não cabe: em vez de estourar a região, ativa scroll não-bloqueante
    ScrollRegiao &s = scrolls[regiao];
    s.ativo = true;
    s.texto = texto;
    s.tamanho = tamanho;
    s.larguraTexto = w;
    s.posX = 0;
    s.ultimoPasso = millis();
    tela->setTextSize(tamanho);
    tela->setCursor(r.x, r.y);
    tela->print(texto); // primeiro quadro, antes do scroll começar a andar
  }

  tela->display();
}

// Escreve duas linhas numa região (ex.: "Palavra: X" / "Resposta: Y").
// Linhas grandes demais são truncadas com "..." -- usado para textos curtos
// e já controlados pelo firmware, então truncar é suficiente (não precisa scroll).
void oledSetDuasLinhas(OledRegiao regiao, const String &linha1, const String &linha2, uint8_t tamanho) {
  const RetanguloRegiao &r = REGIOES[regiao];
  pararScroll(regiao);
  limparRetangulo(r);

  // Aqui truncamos em vez de rolar: são sempre textos curtos definidos pelo
  // próprio firmware, então "..." já garante que nunca vaza pra fora.
  String l1 = truncarComReticencias(linha1, tamanho, r.w);
  String l2 = truncarComReticencias(linha2, tamanho, r.w);

  tela->setTextSize(tamanho);
  tela->setCursor(r.x, r.y);
  tela->print(l1);
  if (l2.length() > 0) {
    tela->setCursor(r.x, r.y + 10 * tamanho);
    tela->print(l2);
  }
  tela->display();
}

// Limpa só uma região, sem escrever nada
void oledLimparRegiao(OledRegiao regiao) {
  pararScroll(regiao);
  limparRetangulo(REGIOES[regiao]);
  tela->display();
}

// Mensagem simples ocupando a tela toda (ex.: "Conectado ao backend").
// Desenha o título + separador uma vez, e delega o corpo pra função de
// atualização (assim ela pode ser chamada de novo depois, sozinha).
void oledMostrarMensagemCheia(const String &titulo, const String &linha1, const String &linha2) {
  zerarTodosScrolls();
  tela->clearDisplay();
  tela->setTextSize(1);
  tela->setCursor(0, 0);
  tela->print(titulo);
  tela->drawFastHLine(0, Y_SEPARADOR_1, tela->width(), SSD1306_WHITE);
  tela->display();
  oledAtualizarCorpoMensagemCheia(linha1, linha2); // desenha o corpo e já dá o display()
}

// Atualiza só o corpo (abaixo do título/separador) de uma "mensagem cheia".
// Sempre limpa o retângulo do corpo inteiro antes, então nunca vaza/sobra
// texto de uma chamada anterior -- seguro pra chamar em loop (ex.: pontinhos
// de "Reconectando...").
void oledAtualizarCorpoMensagemCheia(const String &linha1, const String &linha2) {
  tela->fillRect(0, CORPO_MENSAGEM_Y, tela->width(), CORPO_MENSAGEM_H, SSD1306_BLACK);
  tela->setTextSize(1);
  if (linha1.length() > 0) {
    tela->setCursor(0, CORPO_MENSAGEM_Y);
    tela->print(linha1);
  }
  if (linha2.length() > 0) {
    tela->setCursor(0, CORPO_MENSAGEM_Y + 10);
    tela->print(linha2);
  }
  tela->display();
}

// Tela de boot: desenha o logo no topo + até 2 linhas de rodapé
void oledMostrarBoot(int16_t largura, int16_t altura, const String &rodape1, const String &rodape2) {
  zerarTodosScrolls();
  tela->clearDisplay();
  tela->drawBitmap(0, 0, bmp_logo_SA, largura, altura, SSD1306_WHITE);
  oledAtualizarRodapeBoot(rodape1, rodape2); // já desenha e dá o display()
}

// Atualiza só o rodapé da tela de boot (sem redesenhar o bitmap de novo)
void oledAtualizarRodapeBoot(const String &rodape1, const String &rodape2) {
  tela->fillRect(0, RODAPE_BOOT_Y, tela->width(), RODAPE_BOOT_H, SSD1306_BLACK);
  tela->setTextSize(1);
  tela->setCursor(0, RODAPE_BOOT_Y);
  tela->print(rodape1);
  if (rodape2.length() > 0) {
    tela->setCursor(0, RODAPE_BOOT_Y + 10);
    tela->print(rodape2);
  }
  tela->display();
}

// Limpa a tela inteira
void oledLimparTudo() {
  zerarTodosScrolls();
  tela->clearDisplay();
  tela->display();
}