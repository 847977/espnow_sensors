// espnow_tx.cpp
// -----------------------------------------------------------------------------
// Implementácia ESP-NOW TX.
// ESP32 sa nastaví do WiFi STA režimu, inicializuje ESP-NOW a pridá broadcast peer.
// Sender následne odosiela AudioPacket na všetky zariadenia na rovnakom kanáli.
// -----------------------------------------------------------------------------

#include "espnow_tx.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace transport::espnow_tx {

static uint8_t BROADCAST_ADDR[] = {0xA8, 0x03, 0x2A, 0x6B, 0x6E, 0x28};
static constexpr uint8_t ESPNOW_CHANNEL = 1;

bool init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW TX init failed");
        return false;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_ADDR, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(BROADCAST_ADDR)) {
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("ESP-NOW add broadcast peer failed");
            return false;
        }
    }

    Serial.println("ESP-NOW TX ready");
    return true;
}

bool sendAudioPacket(const AudioPacket& packet) {
    esp_err_t result = esp_now_send(BROADCAST_ADDR,
                                    reinterpret_cast<const uint8_t*>(&packet),
                                    sizeof(packet));
    return result == ESP_OK;
}

}