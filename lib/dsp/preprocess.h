// preprocess.h
// -----------------------------------------------------------------------------
// Predspracovanie rámca pre FFT:
// - odstránenie DC (mean) a prevod na float
// - normalizácia do približného rozsahu [-1..+1]
// - aplikácia Hann okna
//
// Používa sa pre každý kanál zvlášť (L aj R).
// -----------------------------------------------------------------------------

#pragma once
#include <cstddef>
#include <cstdint>

namespace dsp::preprocess {
    void prepareForFFT(const int32_t* in, float* out, std::size_t N, double mean);
}
