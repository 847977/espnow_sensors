// mapping.h
// -----------------------------------------------------------------------------
// Mapovanie FFT spektra na pásma (bass/mid/high).
// Vstup: magnitúdy FFT binov (0..N/2-1).
// Výstup: energia v troch pásmach.
// Toto slúži ako "feature extraction" pre LED/ESP-NOW.
// -----------------------------------------------------------------------------

#pragma once
#include <cstddef>

namespace dsp::mapping {

struct Bands {
    float bass;
    float mid;
    float high;
};

// Spočíta pásma z magnitúd spektra.
// mag: pole veľkosti N/2 (DC bin je 0)
// N: FFT veľkosť (napr. 256)
// sampleRate: Fs (napr. 16000)
Bands computeBands(const float* mag, std::size_t N, float sampleRate);

} // namespace dsp::mapping
