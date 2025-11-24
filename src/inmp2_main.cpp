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

void setup() {
    Serial.begin(921600);
    delay(200);

    //Serial.println("=== Stereo INMP441 test (krok B) ===");

    audio::initI2S_STEREO();  // tvoje lib/audio/i2s_init.cpp
}

void loop() {
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

    recorder::writeStereoFrameToSerial(leftBuf, rightBuf, N);


    // 4) vypíš stereo štatistiky
    //Serial.printf("[L] mean=%7.0f  rms=%7.0f  peak=%9d  dBFS=%6.1f\n",
    //              L.mean, L.rms, L.peakAbs, L.dbfs);

    //Serial.printf("[R] mean=%7.0f  rms=%7.0f  peak=%9d  dBFS=%6.1f\n\n",
    //              R.mean, R.rms, R.peakAbs, R.dbfs);

    //delay(20); // iba diagnostika
}
