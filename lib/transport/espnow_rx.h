// espnow_rx.h
// -----------------------------------------------------------------------------
// Deklarácie pre ESP-NOW príjem na receiver ESP32.
// Receiver prijíma AudioPacket a uchováva posledný platný paket.
// -----------------------------------------------------------------------------

#pragma once
#include "packets.h"

namespace transport::espnow_rx {
    bool init();
    bool hasPacket();
    bool getLatestPacket(AudioPacket& out);
}