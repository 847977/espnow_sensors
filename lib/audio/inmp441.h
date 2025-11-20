// inmp441.h
// Pomocné funkcie pre INMP441: sign-extend 24→32 a demux L/R z interleaved dát.

#pragma once
#include <cstdint>

namespace inmp441 {
    int32_t signExtend24(int32_t raw32);
    void demuxLR(const int32_t* rawInterleaved, int32_t* left, int32_t* right, int countPerChannel);
}
