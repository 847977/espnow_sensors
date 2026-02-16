#include <Arduino.h>
#include "board_pins.h"
#include "board_config.h"
#include "i2s_init.h"
#include "inmp441.h"
#include "frame_stats.h"
#include "recorder.h"
#include "window.h"
#include "preprocess.h"


static const int N = BLOCK_SIZE;

// stereo buffre
int32_t rawInterleaved[2 * N];  // R,L,R,L,... 32-bit words
int32_t leftBuf[N];
int32_t rightBuf[N];

// FFT vstupy (float, okienkované)
float fftInL[N];
float fftInR[N];


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
    dsp::window::initHann(N); // lib/dsp/window.cpp - predpočítaj Hann okno pre BLOCK_SIZE
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

    dsp::preprocess::prepareForFFT(leftBuf,  fftInL, N, L.mean);
    dsp::preprocess::prepareForFFT(rightBuf, fftInR, N, R.mean);

    /*
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 300) {
    lastPrint = millis();

    Serial.printf("L mean=%.0f  first5:", L.mean);
    for (int i = 0; i < 5; i++) Serial.printf(" %.6f", fftInL[i]);
    Serial.println();

    Serial.printf("R mean=%.0f  first5:", R.mean);
    for (int i = 0; i < 5; i++) Serial.printf(" %.6f", fftInR[i]);
    Serial.println();

    auto rmsFloat = [&](float* x) {
    double acc = 0;
    for (int i = 0; i < N; i++) acc += (double)x[i] * (double)x[i];
    return sqrt(acc / (double)N);
    };

    Serial.printf("RMSf L=%.5f  RMSf R=%.5f\n", rmsFloat(fftInL), rmsFloat(fftInR));
    Serial.printf("Edges L: %.6f ... %.6f | R: %.6f ... %.6f\n",
              fftInL[0], fftInL[N-1], fftInR[0], fftInR[N-1]);

    }
    */

    
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
