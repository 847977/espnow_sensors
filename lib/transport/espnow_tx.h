// espnow_tx.h
// -----------------------------------------------------------------------------
// Deklarácie pre ESP-NOW odosielanie zo sender ESP32.
// Sender posiela AudioPacket cez broadcast.
// -----------------------------------------------------------------------------

#pragma once
#include "packets.h"

namespace transport::espnow_tx {
    bool init();
    bool sendAudioPacket(const AudioPacket& packet);
}