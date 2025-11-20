#include <esp_now.h>
#include <WiFi.h>
#include "DHT.h"

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

uint8_t prijimacMac[] = {0xA8, 0x03, 0x2A, 0x6B, 0x6E, 0x28};

typedef struct struct_message {
  float teplota;
  float vlhkost;
} struct_message;

struct_message dataNaOdoslanie;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Stav odoslania: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Úspešné" : "Neúspešné");
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  WiFi.mode(WIFI_STA);

if (esp_now_init() != ESP_OK) {
  Serial.println("Chyba pri inicializácii ESP-NOW");
  return;
}

esp_now_register_send_cb(OnDataSent);

esp_now_peer_info_t peerInfo = {};
memcpy(peerInfo.peer_addr, prijimacMac, 6);
peerInfo.channel = 0;
peerInfo.encrypt = false;
peerInfo.ifidx = WIFI_IF_STA;  // <- bez tohto to padne

if (esp_now_add_peer(&peerInfo) != ESP_OK) {
  Serial.println("Chyba pri pridávaní prijímača");
  return;
}

}

void loop() {
  float vlhkost = dht.readHumidity();
  float teplota = dht.readTemperature();

  if (isnan(vlhkost) || isnan(teplota)) {
    Serial.println("Chyba pri čítaní z DHT11 senzora");
    delay(2000);
    return;
  }

  dataNaOdoslanie.teplota = teplota;
  dataNaOdoslanie.vlhkost = vlhkost;

  Serial.print("Odosielam teplotu: ");
  Serial.print(teplota);
  Serial.print(" °C, vlhkosť: ");
  Serial.print(vlhkost);
  Serial.println(" %");

  esp_now_send(prijimacMac, (uint8_t *) &dataNaOdoslanie, sizeof(dataNaOdoslanie));

  delay(5000);
}
