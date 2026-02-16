// window.h
// -----------------------------------------------------------------------------
// DSP okná (windowing) pre FFT.
// Hann okno sa používa na zníženie "spectral leakage" tým, že potlačí okraje rámca.
// Implementácia používa predpočítané koeficienty (lookup table) vo float.
// -----------------------------------------------------------------------------

#pragma once
#include <cstddef>

namespace dsp::window {

    // Inicializuje (predpočíta) Hann okno pre dané N.
    // Volá sa raz pri štarte alebo pri zmene BLOCK_SIZE.
    void initHann(std::size_t N);

    // Aplikuje predpočítané Hann okno na buffer x (in-place).
    // x[n] *= hann[n]
    void applyHann(float* x, std::size_t N);

}
