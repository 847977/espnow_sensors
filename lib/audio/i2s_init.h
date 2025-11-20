// i2s_init.h
// Deklarácie funkcií na inicializáciu I2S a čítanie interleaved stereo bloku.

#pragma once
#include <cstddef>

namespace audio {
    void initI2S_STEREO();
    void readBlockInterleaved(void* dest, size_t sizeBytes, size_t* outBytes);
}
