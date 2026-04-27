// packets.h
// -----------------------------------------------------------------------------
// ESP-NOW packet pre prenos audio features.
// Obsahuje RMS + bass/mid/treble pre L/R kanál a stavové flags.
// -----------------------------------------------------------------------------

#pragma once
#include <Arduino.h>
#include <cstdint>

namespace transport {

constexpr uint16_t AUDIO_PACKET_MAGIC = 0xA55A;
constexpr uint8_t  AUDIO_PACKET_VERSION = 1;

constexpr uint8_t FLAG_MIC_ENABLED = 0x01;

struct __attribute__((packed)) AudioPacket {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;

    uint32_t seq;
    uint32_t timestampMs;

    uint16_t lRms;
    uint16_t lBass;
    uint16_t lMid;
    uint16_t lTreble;

    uint16_t rRms;
    uint16_t rBass;
    uint16_t rMid;
    uint16_t rTreble;
};

inline uint16_t pack01(float x) {
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return (uint16_t)roundf(x * 65535.0f);
}

inline float unpack01(uint16_t x) {
    return (float)x / 65535.0f;
}

inline bool isValidAudioPacket(const AudioPacket& p) {
    return p.magic == AUDIO_PACKET_MAGIC && p.version == AUDIO_PACKET_VERSION;
}

}