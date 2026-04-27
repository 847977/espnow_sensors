#include <Arduino.h>
#include "board_pins.h"
#include "board_config.h"
#include "i2s_init.h"
#include "inmp441.h"
#include "frame_stats.h"
#include "recorder.h"
#include "window.h"
#include "preprocess.h"
#include "fft_engine.h"
#include "mapping.h"
#include "espnow_tx.h"
#include "packets.h"



static const int N = BLOCK_SIZE;

// stereo buffre
int32_t rawInterleaved[2 * N];  // R,L,R,L,... 32-bit words
int32_t leftBuf[N];
int32_t rightBuf[N];

// FFT vstupy (float, okienkované)
float fftInL[N];
float fftInR[N];

// FFT výstupy (buffre pre: magnitúda, bins 0..N/2)
float magL[N/2];
float magR[N/2];

// ---- Recording control ----
static bool g_recording = false;

// Debounce
static int g_lastReading = HIGH;
static int g_lastStable  = HIGH;
static unsigned long g_lastDebounceMs = 0;
static const unsigned long DEBOUNCE_MS = 40;

static bool g_micEnabled = true;

static int g_micBtnLastReading = HIGH;
static int g_micBtnStable = HIGH;
static unsigned long g_micBtnDebounceMs = 0;
static const unsigned long MIC_BTN_DEBOUNCE_MS = 40;

// Markers (nesmú sa objaviť v PCM, preto sú ako ASCII a posielame ich samostatne)
static const char MARK_START[] = "###START###\n";
static const char MARK_STOP[]  = "###STOP###\n";

static uint32_t seq = 0;
static uint32_t lastSendMs = 0;

float clamp01local(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

float dbfsTo01(float dbfs) {
    // -60 dBFS = ticho, -10 dBFS = silný signál
    return clamp01local((dbfs + 60.0f) / 50.0f);
}

void setup() {
    Serial.begin(921600);
    delay(200);

    //Serial.println("=== Stereo INMP441 test (krok B) ===");

    audio::initI2S_STEREO();  // lib/audio/i2s_init.cpp
    dsp::window::initHann(N); // lib/dsp/window.cpp - predpočítaj Hann okno pre BLOCK_SIZE
    dsp::fft::init(N);        // lib/dsp/fft_engine.cpp - inicializuj FFT engine
    transport::espnow_tx::init();
    
    pinMode(PIN_BTN_MIC_ENABLE, INPUT_PULLUP);


}

void loop() {
    // ---- Button handling (toggle) ----
    int micReading = digitalRead(PIN_BTN_MIC_ENABLE);

    if (micReading != g_micBtnLastReading) {
        g_micBtnDebounceMs = millis();
        g_micBtnLastReading = micReading;
    }

    if ((millis() - g_micBtnDebounceMs) > MIC_BTN_DEBOUNCE_MS) {
        if (micReading != g_micBtnStable) {
            g_micBtnStable = micReading;

            if (g_micBtnStable == LOW) {
                g_micEnabled = !g_micEnabled;
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

    dsp::fft::computeMagnitude(fftInL, magL, N);
    dsp::fft::computeMagnitude(fftInR, magR, N);

    auto bandsL = dsp::mapping::computeBands(magL, N, (float)SAMPLE_RATE);
    auto bandsR = dsp::mapping::computeBands(magR, N, (float)SAMPLE_RATE);

    if (millis() - lastSendMs >= 30) {
        lastSendMs = millis();

        transport::AudioPacket p = {};
    
        p.flags = g_micEnabled ? transport::FLAG_MIC_ENABLED : 0;
        
        p.magic = transport::AUDIO_PACKET_MAGIC;
        p.version = transport::AUDIO_PACKET_VERSION;
        p.seq = seq++;
        p.timestampMs = millis();

        constexpr float BASS_GAIN = 25.0f;
        constexpr float MID_GAIN  = 8.0f;
        constexpr float HIGH_GAIN = 30.0f;

        p.lRms    = transport::pack01(dbfsTo01(L.dbfs));
        p.lBass   = transport::pack01(clamp01local(bandsL.bass * BASS_GAIN));
        p.lMid    = transport::pack01(clamp01local(bandsL.mid  * MID_GAIN));
        p.lTreble = transport::pack01(clamp01local(bandsL.high * HIGH_GAIN));

        p.rRms    = transport::pack01(dbfsTo01(R.dbfs));
        p.rBass   = transport::pack01(clamp01local(bandsR.bass * BASS_GAIN));
        p.rMid    = transport::pack01(clamp01local(bandsR.mid  * MID_GAIN));
        p.rTreble = transport::pack01(clamp01local(bandsR.high * HIGH_GAIN));

        if (!g_micEnabled) {
            p.lRms = p.lBass = p.lMid = p.lTreble = 0;
            p.rRms = p.rBass = p.rMid = p.rTreble = 0;
        } else {
            p.lRms    = transport::pack01(dbfsTo01(L.dbfs));
            p.lBass   = transport::pack01(clamp01local(bandsL.bass * BASS_GAIN));
            p.lMid    = transport::pack01(clamp01local(bandsL.mid  * MID_GAIN));
            p.lTreble = transport::pack01(clamp01local(bandsL.high * HIGH_GAIN));

            p.rRms    = transport::pack01(dbfsTo01(R.dbfs));
            p.rBass   = transport::pack01(clamp01local(bandsR.bass * BASS_GAIN));
            p.rMid    = transport::pack01(clamp01local(bandsR.mid  * MID_GAIN));
            p.rTreble = transport::pack01(clamp01local(bandsR.high * HIGH_GAIN));
        }
        transport::espnow_tx::sendAudioPacket(p);
    }

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 300) {
        lastPrint = millis();

    // napr. bins 1..10 (0 je DC)

    /*
    Serial.print("L bins1-10:");
    for (int i = 1; i <= 10; i++) Serial.printf(" %.3f", magL[i]);
    Serial.println();

    Serial.print("R bins1-10:");
    for (int i = 1; i <= 10; i++) Serial.printf(" %.3f", magR[i]);
    Serial.println();
    */
    
        Serial.printf("Bands L: B=%.4f M=%.4f H=%.4f | R: B=%.4f M=%.4f H=%.4f\n",
                bandsL.bass, bandsL.mid, bandsL.high,
                bandsR.bass, bandsR.mid, bandsR.high);

    }

    /*
    if (g_recording) {
    recorder::writeStereoFrameToSerial(leftBuf, rightBuf, N);
    }
    */ 
    

    // 4) vypíš stereo štatistiky
    //Serial.printf("[L] mean=%7.0f  rms=%7.0f  peak=%9d  dBFS=%6.1f\n",
    //              L.mean, L.rms, L.peakAbs, L.dbfs);

    //Serial.printf("[R] mean=%7.0f  rms=%7.0f  peak=%9d  dBFS=%6.1f\n\n",
    //              R.mean, R.rms, R.peakAbs, R.dbfs);

    //delay(20); // iba diagnostika
}
