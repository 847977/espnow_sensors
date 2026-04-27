// espnow_rx.cpp
// -----------------------------------------------------------------------------
// Implementácia ESP-NOW RX.
// Receiver je v STA režime na rovnakom kanáli ako sender.
// Pri prijatí dát callback overí veľkosť a magic/version packetu,
// potom uloží posledný platný AudioPacket.
// -----------------------------------------------------------------------------

#include "espnow_rx.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace transport::espnow_rx {

static constexpr uint8_t ESPNOW_CHANNEL = 1;

static volatile bool g_hasPacket = false;
static AudioPacket g_latestPacket;

static void handlePacket(const uint8_t* data, int len) {
    if (len != sizeof(AudioPacket)) return;

    AudioPacket p;
    memcpy(&p, data, sizeof(AudioPacket));

    if (!isValidAudioPacket(p)) return;

    g_latestPacket = p;
    g_hasPacket = true;
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    handlePacket(data, len);
}
#else
static void onReceive(const uint8_t* mac, const uint8_t* data, int len) {
    handlePacket(data, len);
}
#endif

bool init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW RX init failed");
        return false;
    }

    esp_now_register_recv_cb(onReceive);

    Serial.println("ESP-NOW RX ready");
    return true;
}

bool hasPacket() {
    return g_hasPacket;
}

bool getLatestPacket(AudioPacket& out) {
    if (!g_hasPacket) return false;
    out = g_latestPacket;
    g_hasPacket = false;
    return true;
}

}