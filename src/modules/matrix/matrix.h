#ifndef MATRIX_H
#define MATRIX_H

#include <Arduino.h>
#include <MD_Parola.h>

// ---------- Animações de frames disponíveis ----------
enum MatrixAnimation : uint8_t {
  MATRIX_ANIM_CHECK  = 0, // animação de onda/pulso -> usada para ACERTO
  MATRIX_ANIM_CROSS  = 1, // X piscando -> usada para ERRO
  MATRIX_ANIM_ARROW  = 2
};

// ---------- Modo atual do módulo (uso interno / consulta opcional) ----------
enum MatrixMode : uint8_t {
  MATRIX_MODE_IDLE, // display limpo, nada sendo controlado
  MATRIX_MODE_TEXT, // texto/número via Parola (contagem, "0" piscando etc.)
  MATRIX_MODE_ANIM  // animação de frames customizada (acerto/erro)
};

void matrixImage(uint8_t animationId, uint16_t frameIdx, MD_Parola &P);
void matrixUpdate(uint8_t animationId, uint32_t intervalDelay, MD_Parola &P);

void matrixInit(MD_Parola &P, uint8_t intensity = 1);
void matrixShowText(const String &text, MD_Parola &P, textPosition_t align = PA_CENTER);
void matrixStartAnimation(MatrixAnimation anim, MD_Parola &P);
void matrixTick(uint32_t intervalDelay, MD_Parola &P);
void matrixClear(MD_Parola &P);

// Consulta opcional do modo atual, caso o main.cpp precise decidir algo com base nisso.
MatrixMode matrixGetMode();

#endif