// frame_stats.cpp
// Výpočet mean (DC), RMS, peak a dBFS pre jeden rámec vzoriek.

#include "frame_stats.h"
#include <cmath>

namespace {
    constexpr double FULL_SCALE = 8388608.0; // 2^23 pre 24-bit PCM
}

namespace dsp {

FrameStats analyzeFrame(const int32_t* buf, int N) {
    // 1) mean (DC)
    long double sum = 0.0L;
    for (int i = 0; i < N; ++i) {
        sum += static_cast<long double>(buf[i]);
    }
    double mean = static_cast<double>(sum / static_cast<long double>(N));

    // 2) RMS + peak po odčítaní DC
    long double acc = 0.0L;
    double peakAbs = 0.0;

    for (int i = 0; i < N; ++i) {
        double x = static_cast<double>(buf[i]) - mean;
        acc += x * x;
        double ax = std::fabs(x);
        if (ax > peakAbs) {
            peakAbs = ax;
        }
    }

    double rms = std::sqrt(static_cast<double>(acc / static_cast<long double>(N)));
    double dbfs = (rms > 1.0)
                    ? 20.0 * std::log10(rms / FULL_SCALE)
                    : -120.0;

    FrameStats stats;
    stats.mean    = mean;
    stats.rms     = rms;
    stats.peakAbs = static_cast<int32_t>(peakAbs);
    stats.dbfs    = dbfs;
    return stats;
}

} // namespace dsp
