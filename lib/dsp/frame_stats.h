// frame_stats.h
// Štatistiky rámca: mean (DC), RMS, peak, dBFS.

#pragma once
#include <cstdint>

struct FrameStats {
    double   mean;
    double   rms;
    int32_t  peakAbs;
    double   dbfs;
};

namespace dsp {
    FrameStats analyzeFrame(const int32_t* buf, int count);
}
