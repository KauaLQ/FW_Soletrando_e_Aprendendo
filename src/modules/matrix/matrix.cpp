/*
TODO:   Adicionar nova função de alto nível para texto scroll
        Adicionar animação de vitória
        Adicionar tempos diferentes para as animações 
*/

#include "matrix.h"
#include "animations.h"

// Variáveis de controle de estado do módulo (compartilhadas pelas funções deste arquivo)
static uint32_t lastUpdateTime = 0;
static uint16_t currentFrame = 0;
static uint8_t lastAnimationId = 255;
// Modo atual do módulo. É isso que evita o conflito entre a API de baixo nível (buffer direto do MD_MAX72XX) e a API de alto nível do Parola (print/displayClear).
static MatrixMode currentMode = MATRIX_MODE_IDLE;
static uint8_t activeAnimationId = 255;

// ----------- Baixo Nível -----------
void matrixImage(uint8_t animationId, uint16_t frameIdx, MD_Parola &P) {
  const uint8_t* frameSelected = nullptr;

  switch (animationId) {
    case MATRIX_ANIM_CHECK:
      if (frameIdx < IMAGES_CHECK_LEN) frameSelected = IMAGES_CHECK[frameIdx];
      break;
    case MATRIX_ANIM_CROSS:
      if (frameIdx < IMAGES_CROSS_LEN) frameSelected = IMAGES_CROSS[frameIdx];
      break;
    case MATRIX_ANIM_ARROW:
      if (frameIdx < IMAGES_ARROW_LEN) frameSelected = IMAGES_ARROW[frameIdx];
      break;
    default:
      return;
  }

  if (frameSelected != nullptr) {
    MD_MAX72XX *mx = P.getGraphicObject();
    if (mx != nullptr) {
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
      mx->setBuffer(7, 8, (uint8_t*)frameSelected);
      mx->transform(MD_MAX72XX::TRC);
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
    }
  }
}

// Função de atualização Não-Bloqueante
void matrixUpdate(uint8_t animationId, uint32_t intervalDelay, MD_Parola &P) {
  // Se o usuário mudar a animação no meio da execução, reinicia o contador de frames
  if (animationId != lastAnimationId) {
    currentFrame = 0;
    lastAnimationId = animationId;
    lastUpdateTime = millis(); // Força a atualização imediata do primeiro frame
    matrixImage(animationId, currentFrame, P);
    return;
  }

  // Verifica se o tempo necessário decorreu (Abordagem não-bloqueante)
  if (millis() - lastUpdateTime >= intervalDelay) {
    lastUpdateTime = millis();
    // Determina o limite de frames baseado na animação atual para realizar o incremento rotativo
    int totalFrames = 0;
    if (animationId == MATRIX_ANIM_CHECK) totalFrames = IMAGES_CHECK_LEN;
    else if (animationId == MATRIX_ANIM_CROSS) totalFrames = IMAGES_CROSS_LEN;
    else if (animationId == MATRIX_ANIM_ARROW) totalFrames = IMAGES_ARROW_LEN;

    if (totalFrames > 0) {
      currentFrame++;
      if (currentFrame >= totalFrames) {
        currentFrame = 0;
      }
      // Chama a função interna para atualizar a matriz física
      matrixImage(animationId, currentFrame, P);
    }
  }
}

// ----------- Alto nível -----------
void matrixInit(MD_Parola &P, uint8_t intensity) {
  P.begin();
  P.setIntensity(intensity);
  P.displayClear();
  currentMode = MATRIX_MODE_IDLE;
}

void matrixShowText(const String &text, MD_Parola &P, textPosition_t align) {
  // Se estávamos numa animação de frames (buffer direto), o Parola não sabe
  // disso. Precisamos limpar antes de voltar a usar a API de texto dele,
  // senão o texto pode ser desenhado por cima de lixo do frame anterior.
  if (currentMode == MATRIX_MODE_ANIM) {
    P.displayClear();
  }

  P.setTextAlignment(align);
  P.print(text);

  currentMode = MATRIX_MODE_TEXT;
  // Zera o estado de animação para garantir que, se voltarmos a uma animação
  // depois, ela reinicie do frame 0 em vez de continuar de onde parou.
  lastAnimationId = 255;
  currentFrame = 0;
  activeAnimationId = 255;
}

void matrixStartAnimation(MatrixAnimation anim, MD_Parola &P) {
  // Se estávamos em modo texto, o Parola pode ter estado interno pendente
  // (texto em animação, cursor, etc.). Limpamos antes de tomar o controle
  // do buffer diretamente via matrixImage/matrixUpdate.
  if (currentMode == MATRIX_MODE_TEXT) {
    P.displayClear();
  }

  activeAnimationId = (uint8_t)anim;
  currentMode = MATRIX_MODE_ANIM;

  // Força o primeiro frame a ser desenhado imediatamente
  lastAnimationId = 255;
  matrixUpdate(activeAnimationId, 0, P);
}

void matrixTick(uint32_t intervalDelay, MD_Parola &P) {
  if (currentMode != MATRIX_MODE_ANIM) return;
  matrixUpdate(activeAnimationId, intervalDelay, P);
}

// Limpa fisicamente o display e zera completamente o estado interno do módulo
void matrixClear(MD_Parola &P) {
  P.displayClear();      // Apaga os LEDs fisicamente
  currentFrame = 0;      // Reseta para o primeiro frame
  lastUpdateTime = 0;    // Reseta o temporizador
  lastAnimationId = 255; // Reseta o ID (garante que qualquer ID enviado no update seja tratado como novo)
  activeAnimationId = 255;
  currentMode = MATRIX_MODE_IDLE;
}

MatrixMode matrixGetMode() {
  return currentMode;
}