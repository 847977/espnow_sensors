// mapping.cpp
// -----------------------------------------------------------------------------
// Výpočet energie v pásmach z FFT magnitúd.
// Používame power integráciu: sum(mag[k]^2) v rozsahu binov.
// Potom urobíme sqrt( average_power ) -> "RMS energia pásma".
// Prečo:
// - stabilnejšie ako len mag suma
// - menej citlivé na bin misalignment
// -----------------------------------------------------------------------------

#include "mapping.h"
#include <cmath>

namespace dsp::mapping {

    // std::clamp nie je dostupný
    static inline int clampInt(int x, int lo, int hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }


    static inline int hzToBin(float hz, float sampleRate, std::size_t N) {
        // bin k predstavuje frekvenciu k * Fs / N
        float k = hz * (float)N / sampleRate;
        return (int)std::lround(k);
    }

    static float bandEnergyRMS(const float* mag, int k0, int k1) {
        // integrujeme power v rozsahu <k0, k1> inclusive
        // ignorujeme záporné alebo nerealistické indexy
        if (k1 < k0) return 0.0f;
        long double acc = 0.0L;
        int count = 0;

        for (int k = k0; k <= k1; ++k) {
            float m = mag[k];
            acc += (long double)m * (long double)m;
            count++;
        }
        if (count <= 0) return 0.0f;

        long double meanPower = acc / (long double)count;
        return (float)std::sqrt((double)meanPower);
    }

    Bands computeBands(const float* mag, std::size_t N, float sampleRate) {
        const int half = (int)(N / 2);

        // Navrhnuté pásma pre Fs=16kHz, N=256:
        // Bass:  60–250 Hz
        // Mid:   250–2000 Hz
        // High:  2000–8000 Hz (Nyquist)
        int b0 = hzToBin(60.0f, sampleRate, N);
        int b1 = hzToBin(250.0f, sampleRate, N);

        int m0 = hzToBin(250.0f, sampleRate, N);
        int m1 = hzToBin(2000.0f, sampleRate, N);

        int h0 = hzToBin(2000.0f, sampleRate, N);
        int h1 = half - 1; // posledný bin pod Nyquist

        // ochrany + ignoruj DC bin (0)
        b0 = clampInt(b0, 1, half - 1);
        b1 = clampInt(b1, 1, half - 1);
        m0 = clampInt(m0, 1, half - 1);
        m1 = clampInt(m1, 1, half - 1);
        h0 = clampInt(h0, 1, half - 1);
        h1 = clampInt(h1, 1, half - 1);


        Bands out;
        out.bass = bandEnergyRMS(mag, b0, b1);
        out.mid  = bandEnergyRMS(mag, m0, m1);
        out.high = bandEnergyRMS(mag, h0, h1);
        return out;
    }

} // namespace dsp::mapping
