#include <esp_now.h>
#include <WiFi.h>

typedef struct struct_message {
  float teplota;
  float vlhkost;
} struct_message;

void OnDataRecv(const uint8_t *info, const uint8_t *incomingData, int len) {
  struct_message prijateData;
  memcpy(&prijateData, incomingData, sizeof(prijateData));

  Serial.print("Prijatá teplota: ");
  Serial.print(prijateData.teplota);
  Serial.print(" °C, vlhkosť: ");
  Serial.print(prijateData.vlhkost);
  Serial.println(" %");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Chyba pri inicializácii ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
}
