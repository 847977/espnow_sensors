#include <Arduino.h>
#include "board_pins.h"
#include "board_config.h"
#include "i2s_init.h"
#include "inmp441.h"
#include "frame_stats.h"
#include "recorder.h"

static const int N = BLOCK_SIZE;

// stereo buffre
int32_t rawInterleaved[2 * N];  // R,L,R,L,... 32-bit words
int32_t leftBuf[N];
int32_t rightBuf[N];

// ---- Recording control ----
static bool g_recording = false;

// Debounce
static int g_lastReading = HIGH;
static int g_lastStable  = HIGH;
static unsigned long g_lastDebounceMs = 0;
static const unsigned long DEBOUNCE_MS = 40;

// Markers (nesmú sa objaviť v PCM, preto sú ako ASCII a posielame ich samostatne)
static const char MARK_START[] = "###START###\n";
static const char MARK_STOP[]  = "###STOP###\n";



void setup() {
    Serial.begin(921600);
    delay(200);

    //Serial.println("=== Stereo INMP441 test (krok B) ===");

    audio::initI2S_STEREO();  // lib/audio/i2s_init.cpp
    pinMode(PIN_BTN_REC, INPUT_PULLUP);
    pinMode(PIN_LED_REC, OUTPUT);
    digitalWrite(PIN_LED_REC, LOW);


}

void loop() {
    // ---- Button handling (toggle) ----
    int reading = digitalRead(PIN_BTN_REC);

    if (reading != g_lastReading) {
        g_lastDebounceMs = millis();
        g_lastReading = reading;
    }

    if ((millis() - g_lastDebounceMs) > DEBOUNCE_MS) {
        if (reading != g_lastStable) {
            g_lastStable = reading;

            // toggle iba pri stlačení (LOW)
            if (g_lastStable == LOW) {
                g_recording = !g_recording;

                digitalWrite(PIN_LED_REC, g_recording ? HIGH : LOW);

                if (g_recording) {
                    Serial.write((const uint8_t*)MARK_START, sizeof(MARK_START) - 1);
                } else {
                    Serial.write((const uint8_t*)MARK_STOP, sizeof(MARK_STOP) - 1);
                }
            }
        }
    }


    size_t bytesRead = 0;

    // 1) načítaj interleaved stereo DMA blok
    audio::readBlockInterleaved(rawInterleaved, sizeof(rawInterleaved), &bytesRead);

    const int words = bytesRead / 4;  
    if (words < 2) return; // istota

    // 2) rozdeleň na Left / Right (sign-extend priamo vo funkcii)
    inmp441::demuxLR(rawInterleaved, leftBuf, rightBuf, N);

    // 3) spočítaj štatistiky
    FrameStats L = dsp::analyzeFrame(leftBuf, N);
    FrameStats R = dsp::analyzeFrame(rightBuf, N);

    if (g_recording) {
    recorder::writeStereoFrameToSerial(leftBuf, rightBuf, N);
    }




    // 4) vypíš stereo štatistiky
    //Serial.printf("[L] mean=%7.0f  rms=%7.0f  peak=%9d  dBFS=%6.1f\n",
    //              L.mean, L.rms, L.peakAbs, L.dbfs);

    //Serial.printf("[R] mean=%7.0f  rms=%7.0f  peak=%9d  dBFS=%6.1f\n\n",
    //              R.mean, R.rms, R.peakAbs, R.dbfs);

    //delay(20); // iba diagnostika
}
