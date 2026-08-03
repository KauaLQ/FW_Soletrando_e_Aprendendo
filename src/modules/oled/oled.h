#ifndef OLED_H
#define OLED_H

#include <Adafruit_SSD1306.h>
#include <Arduino.h>

enum OledRegiao {
  OLED_STATUS,      // linha de cima: nível, mensagens curtas de status
  OLED_PALAVRA,     // miolo da tela: a palavra a decorar (ou 2 linhas de info)
  OLED_INSTRUCAO,   // rodapé: instruções pro jogador (até 2 linhas)
  OLED_NUM_REGIOES
};

void oledInit(Adafruit_SSD1306 &telaRef);
void oledTick();                            

// ---------- Layout de jogo (regiões + separadores fixos) ----------
void oledIniciarTelaJogo();                 
void oledSetTexto(OledRegiao regiao, const String &texto, uint8_t tamanho = 1, bool centralizado = false);
void oledSetDuasLinhas(OledRegiao regiao, const String &linha1, const String &linha2, uint8_t tamanho = 1);
void oledLimparRegiao(OledRegiao regiao);

// ---------- Telas fora do layout de regiões ----------
void oledMostrarMensagemCheia(const String &titulo, const String &linha1 = "", const String &linha2 = "");
void oledAtualizarCorpoMensagemCheia(const String &linha1, const String &linha2 = "");
void oledMostrarBoot(int16_t largura, int16_t altura, const String &rodape1, const String &rodape2 = "");
void oledAtualizarRodapeBoot(const String &rodape1, const String &rodape2 = "");
void oledMostrarIconeAudio(bool mudo);
void oledLimparTudo();

#endif