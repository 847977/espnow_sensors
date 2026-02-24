// fft_engine.h
// -----------------------------------------------------------------------------
// FFT engine založený na ArduinoFFT.
// Vstup: float buffer (N vzoriek) pripravený preprocessom (DC removed + Hann).
// Výstup: magnitúda spektra pre biny 0..N/2-1 (do Nyquist frekvencie).
// -----------------------------------------------------------------------------

#pragma once
#include <cstddef>

namespace dsp::fft {

    // Inicializácia interných bufferov (volať raz v setup)
    void init(std::size_t N);

    // Vypočíta magnitúdu spektra z input bufferu.
    // input:  N float samples
    // magOut: N/2 magnitudes
    void computeMagnitude(const float* input, float* magOut, std::size_t N);

}
