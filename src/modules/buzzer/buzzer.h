#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "notes.h"

#define PWM_CHANEL 0
#define RESOLUTION 8
#define BUZZER_PIN 18

void initBuzzer();
void startBuzzerSong(uint8_t number_melody);                        // Dispara o início da música (chamado uma vez)
void singleNoteBuzzer(int frequency, unsigned long durationMs);     // Toca Nota isolada
void updateBuzzerTick();                                            // Atualiza a máquina de estados (chamado no loop)

#endif