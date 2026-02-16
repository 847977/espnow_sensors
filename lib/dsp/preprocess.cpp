// preprocess.cpp
// -----------------------------------------------------------------------------
// Implementácia predspracovania rámca pre FFT.
// Vstup:
//  - in[]  : int32 sign-extended (reálne 24-bit PCM v rozsahu +-2^23)
//  - mean  : DC zložka vypočítaná inde (napr. frame_stats)
// Výstup:
//  - out[] : float pripravený pre FFT, okienkovaný Hann oknom
//
// Kroky:
// 1) out[i] = (in[i] - mean) / FULL_SCALE
// 2) aplikovať Hann okno (dsp::window::applyHann)
// -----------------------------------------------------------------------------

#include "preprocess.h"
#include "window.h"

namespace {
    constexpr float FULL_SCALE_F = 8388608.0f; // 2^23
}

namespace dsp::preprocess {

void prepareForFFT(const int32_t* in, float* out, std::size_t N, double mean) {
    const float meanF = static_cast<float>(mean);

    for (std::size_t i = 0; i < N; ++i) {
        float x = static_cast<float>(in[i]) - meanF;
        out[i] = x / FULL_SCALE_F;
    }

    dsp::window::applyHann(out, N);
}

} // namespace dsp::preprocess
