// inmp441.cpp
// Sign-extend 24-bit PCM z INMP441 a demux interleaved stereo (R,L,R,L...) na L[] a R[].

#include "inmp441.h"

namespace inmp441 {

int32_t signExtend24(int32_t v32) {
    int32_t s = v32 >> 8;          // zahodíme 8 LSB padov
    if (s & 0x00800000) {          // ak je nastavený sign bit 24-bit hodnoty
        s |= 0xFF000000;           // doplníme horné bity na 1 (negatívne číslo)
    }
    return s;
}

// rawInterleaved: [R0, L0, R1, L1, ...], dĺžka = 2*N slov (32-bit)
void demuxLR(const int32_t* raw, int32_t* left, int32_t* right, int N) {
    for (int i = 0; i < N; ++i) {
        int32_t r32 = raw[2 * i + 0];
        int32_t l32 = raw[2 * i + 1];
        right[i] = signExtend24(r32);
        left[i]  = signExtend24(l32);
    }
}

} // namespace inmp441
