#include "buzzer.h"

// ----------- variáveis de controle de estado -----------
static int thisNote = 0;
static unsigned long previousMillis = 0;
static unsigned long targetDuration = 0;
static bool mudo = false; // controle de silenciar/ativar os efeitos sonoros (menu de configurações)

enum SongState { PLAYING_NOTE, PAUSE_BETWEEN_NOTES, PLAYING_SINGLE_NOTE, SONG_FINISHED };
static SongState currentState = SONG_FINISHED; 

// ----------- Ponteiros e variáveis globais da melodia atual -----------
static int* currentMelody = nullptr; // Aponta para a melodia ativa
static int totalNotes = 0;           // Guarda a quantidade de notas da música ativa
static int wholenote = 0;            // Guarda o tempo calculado da fórmula

// Protótipos das funções internas locais
static void proximaNota();

void initBuzzer() {
    pinMode(BUZZER_PIN, OUTPUT);
    ledcSetup(PWM_CHANEL, 2000, RESOLUTION);
    ledcAttachPin(BUZZER_PIN, PWM_CHANEL);
}

// ----------- Controle de mudo -----------
void buzzerMutar() {
    mudo = true;
    ledcWriteTone(PWM_CHANEL, 0); // corta qualquer som que esteja tocando agora
    currentState = SONG_FINISHED; // e encerra a "melodia"/nota em andamento
}

void buzzerDesmutar() {
    mudo = false;
}

bool buzzerEstaMudo() {
    return mudo;
}

void singleNoteBuzzer(int frequency, unsigned long durationMs) {
    if (mudo) return; // efeitos sonoros desativados: ignora o pedido silenciosamente

    if (frequency != REST) {
        ledcWriteTone(PWM_CHANEL, frequency);
    } else {
        ledcWriteTone(PWM_CHANEL, 0);
    }
    
    targetDuration = durationMs;
    previousMillis = millis();
    currentState = PLAYING_SINGLE_NOTE; // Atualiza para o novo estado
}

// Dispara o início da música baseado no ID informado
void startBuzzerSong(uint8_t number_melody) {
    if (mudo) return; // efeitos sonoros desativados: ignora o pedido silenciosamente

    int tempo = 0;
    int arraySize = 0;

    switch (number_melody) {
        case 1:
            currentMelody = melody_nokia;
            arraySize = sizeof(melody_nokia);
            tempo = 180;
            break;
        case 2:
            currentMelody = melody_zeldatheme;
            arraySize = sizeof(melody_zeldatheme);
            tempo = 88;
            break;
        default:
            currentMelody = melody_nokia;
            arraySize = sizeof(melody_nokia);
            tempo = 180;
            break;
    }

    // Calcula os dados baseados na música escolhida
    totalNotes = arraySize / sizeof(currentMelody[0]) / 2;
    wholenote = (60000 * 4) / tempo;
    
    thisNote = 0;
    currentState = PLAYING_NOTE;
    proximaNota(); 
}

// Roda direto no loop() principal
void updateBuzzerTick() {
    if (currentState == SONG_FINISHED || currentMelody == nullptr) return;

    unsigned long currentMillis = millis();

    // Monitorando o tempo de uma nota isolada
    if (currentState == PLAYING_SINGLE_NOTE) {
        if (currentMillis - previousMillis >= targetDuration) {
        ledcWriteTone(PWM_CHANEL, 0);   // Desliga o som
        currentState = SONG_FINISHED;   // Finaliza o estado
        }
    }
    // Monitorando a melodia completa
    else if (currentState == PLAYING_NOTE) {
        if (currentMillis - previousMillis >= targetDuration) {
            ledcWriteTone(PWM_CHANEL, 0); // Pausa/Respiro entre notas
            
            int divider = currentMelody[thisNote + 1];
            int totalDuration = (divider > 0) ? (wholenote / divider) : (wholenote / abs(divider) * 1.5);
            
            targetDuration = totalDuration * 0.10; 
            previousMillis = currentMillis;
            currentState = PAUSE_BETWEEN_NOTES;
        }
    } 
    else if (currentState == PAUSE_BETWEEN_NOTES) {
        if (currentMillis - previousMillis >= targetDuration) {
            thisNote += 2;
            proximaNota(); // Passa para a próxima nota
        }
    }
}

// Função interna auxiliar
static void proximaNota() {
    if (thisNote >= totalNotes * 2) {
        currentState = SONG_FINISHED;
        ledcWriteTone(PWM_CHANEL, 0); 
        return;
    }

    int divider = currentMelody[thisNote + 1];
    int noteDuration = 0;

    if (divider > 0) {
        noteDuration = wholenote / divider;
    } else if (divider < 0) {
        noteDuration = wholenote / abs(divider);
        noteDuration *= 1.5;
    }

    int toneFreq = currentMelody[thisNote];
    
    if (toneFreq != REST) {
        ledcWriteTone(PWM_CHANEL, toneFreq);
    } else {
        ledcWriteTone(PWM_CHANEL, 0);
    }

    targetDuration = noteDuration * 0.90;
    previousMillis = millis();
    currentState = PLAYING_NOTE;
}