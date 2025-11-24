// recorder.cpp
// -----------------------------------------------------------------------------
// Implementácia funkcie writeStereoFrameToSerial:
// - vstup: left[] a right[] sú sign-extended 32-bit vzorky z INMP441,
// - výstup: uint8_t PCM buffer v tvare [L_lo, L_hi, R_lo, R_hi, ...],
// - prevod 24-bit PCM -> 16-bit PCM (posunom o 8 bitov),
// - odoslanie celého stereo rámca cez Serial.write().
//
// Toto je "debug/utility" modul, nie súčasť hlavného DSP.
// Slúži na vytvorenie WAV súboru na PC.
// -----------------------------------------------------------------------------

#include "recorder.h"

namespace recorder {

void writeStereoFrameToSerial(const int32_t* left,
                              const int32_t* right,
                              int count)
{
    // Stereo 16-bit PCM = 4 bajty na sample (2 L + 2 R)
    const int BYTES_PER_SAMPLE = 4;
    const int totalBytes = count * BYTES_PER_SAMPLE;

    // Alokujeme lokálny buffer na odoslanie
    uint8_t buffer[totalBytes];

    int idx = 0;
    for (int i = 0; i < count; i++) {
        // 24-bit -> 16-bit PCM
        int16_t l16 = left[i]  >> 8;   // stále sign-extended
        int16_t r16 = right[i] >> 8;

        // uloženie L
        buffer[idx++] = l16 & 0xFF;        // L low byte
        buffer[idx++] = (l16 >> 8) & 0xFF; // L high byte

        // uloženie R
        buffer[idx++] = r16 & 0xFF;        // R low byte
        buffer[idx++] = (r16 >> 8) & 0xFF; // R high byte
    }

    // Odošli celý blok naraz
    Serial.write(buffer, totalBytes);
}

} // namespace recorder
