// window.cpp
// -----------------------------------------------------------------------------
// Implementácia Hann okna (float).
// - initHann(N): prepočíta koeficienty hann[n] a uloží ich do interného poľa.
// - applyHann(x, N): vynásobí každý prvok x[n] koeficientom hann[n].
//
// Prepojenie:
// Tento modul bude používaný v predspracovaní pred FFT (krok C),
// aby FFT dostala "okienkovaný" rámec so zníženou spectral leakage.
// -----------------------------------------------------------------------------

#include "window.h"
#include <cmath>     // cosf
#include <vector>

namespace dsp::window {

static std::vector<float> g_hann;

void initHann(std::size_t N) {
    g_hann.resize(N);

    if (N == 0) return;
    if (N == 1) {
        g_hann[0] = 1.0f;
        return;
    }

    // Hann: w[n] = 0.5 * (1 - cos(2*pi*n/(N-1)))
    const float twoPi = 6.28318530717958647692f;
    for (std::size_t n = 0; n < N; ++n) {
        float phase = twoPi * static_cast<float>(n) / static_cast<float>(N - 1);
        g_hann[n] = 0.5f * (1.0f - cosf(phase));
    }
}

void applyHann(float* x, std::size_t N) {
    // bezpečnosť: ak initHann nebolo zavolané alebo N nesedí
    if (g_hann.size() != N) {
        initHann(N);
    }

    for (std::size_t n = 0; n < N; ++n) {
        x[n] *= g_hann[n];
    }
}

}
