// recorder.h
// -----------------------------------------------------------------------------
// Modul pre prevod stereo 24-bit sign-extended vzoriek (int32_t) do 16-bit PCM
// stereo formátu a ich odoslanie cez Serial na PC. 
//
// Tento modul slúži len na overenie kvality zvuku (kontrola v Audacity).
// Nezasahuje do DSP pipeline ani FFT.
// -----------------------------------------------------------------------------

#pragma once
#include <Arduino.h>
#include <cstdint>

namespace recorder {
    // Konverzia jedného stereo rámca a odoslanie všetkých bajtov cez Serial.
    // left[i], right[i] -> int32 sign-extended 24-bit PCM
    void writeStereoFrameToSerial(const int32_t* left,
                                  const int32_t* right,
                                  int count);
}
