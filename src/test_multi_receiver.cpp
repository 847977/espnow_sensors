// src/test_multi_rx.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "messages.h"

#define RX_LED 33  // LED cez 2k2 do GND

struct OneShot { int pin; uint32_t off_at=0;
  void init(int p){ pin=p; pinMode(pin,OUTPUT); digitalWrite(pin,LOW); }
  void pulse(uint16_t ms=3){ digitalWrite(pin,HIGH); off_at=millis()+ms; }
  void tick(){ if(off_at && millis()>=off_at){ digitalWrite(pin,LOW); off_at=0; } }
} ledRX;

void onRecv(const uint8_t*, const uint8_t* data, int len){
  if (len < (int)sizeof(MsgHdr)) return;
  ledRX.pulse();   // indikácia: čokoľvek prišlo
  // tu môžeš (neskôr) pridať rýchle štatistiky/počítadlá bez Serialu
}

void setup(){
  ledRX.init(RX_LED);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) while(1){}
  esp_now_register_recv_cb(onRecv);
}

void loop(){ ledRX.tick(); }
