// fft_engine.cpp
// -----------------------------------------------------------------------------
// Implementácia FFT nad float vzorkami pomocou ArduinoFFT.
// Robí:
// 1) skopíruje input do vReal[]
// 2) nastaví vImag[] = 0
// 3) FFT.Compute()
// 4) FFT.ComplexToMagnitude()
// 5) vráti magnitúdy pre biny 0..N/2-1
// -----------------------------------------------------------------------------

#include "fft_engine.h"
#include <ArduinoFFT.h>
#include <vector>
#include <cstring>

namespace dsp::fft {

static std::vector<float> vReal;
static std::vector<float> vImag;

// ArduinoFFT objekt (nové API používa triedu ArduinoFFT)
static ArduinoFFT<float> FFT;

void init(std::size_t N) {
    vReal.assign(N, 0.0f);
    vImag.assign(N, 0.0f);
}

void computeMagnitude(const float* input, float* magOut, std::size_t N) {
    if (vReal.size() != N || vImag.size() != N) {
        init(N);
    }

    // copy input -> vReal, imag = 0
    std::memcpy(vReal.data(), input, N * sizeof(float));
    std::memset(vImag.data(), 0, N * sizeof(float));

    // FFT forward
    FFT.compute(vReal.data(), vImag.data(), N, FFTDirection::Forward);

    // Convert complex spectrum -> magnitude
    FFT.complexToMagnitude(vReal.data(), vImag.data(), N);

    // Keep only 0..N/2-1 (Nyquist)
    const std::size_t half = N / 2;
    for (std::size_t i = 0; i < half; ++i) {
        magOut[i] = vReal[i];
    }
}

} // namespace dsp::fft
