// src/test_multi_tx.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_MPL3115A2.h>
#include <DHT.h>
#include "messages.h"

// ------- PINY (uprav len ak potrebuješ) -------
#define TX_LED    25   // LED cez 2k2 do GND (GPIO -> rezistor -> anóda LED)
#define DHTPIN     4
#define DHTTYPE DHT11
#define TRIG_PIN   5
#define ECHO_PIN  18
// I2C MPL: SDA=21, SCL=22 (default ESP32 DevKitC)

// ------- INTERVALY -------
static const uint32_t INT_HC  = 50;   
static const uint32_t INT_DHT = 100;  
static const uint32_t INT_MPL = 500;  

// ------- MAC PRIJÍMAČA (UPRAV NA TVOJE RX) -------
uint8_t rxMac[] = { 0xA8, 0x03, 0x2A, 0x6B, 0x6E, 0x28 };

// ------- SENZORY -------
DHT dht(DHTPIN, DHTTYPE);
Adafruit_MPL3115A2 mpl;

// ------- ČASOVAČE + SEQ -------
uint32_t lastHC=0, lastDHT=0, lastMPL=0;
uint32_t seqBatch=0;

// ------- JEDNODUCHÝ LED pulz (bez delay) -------
struct OneShot { int pin; uint32_t off_at=0;
  void init(int p){ pin=p; pinMode(pin,OUTPUT); digitalWrite(pin,LOW); }
  void pulse(uint16_t ms=3){ digitalWrite(pin,HIGH); off_at=millis()+ms; }
  void tick(){ if(off_at && millis()>=off_at){ digitalWrite(pin,LOW); off_at=0; } }
} ledTX;

// ------- FIFO (ring buffer packetov) -------
struct QItem { uint8_t bytes[32]; uint8_t len; };   // 32 B stačí (hdr 10 + MPL 8 + DHT 4 + HC 2)
static const uint8_t QCAP = 16;
QItem q[QCAP]; volatile uint8_t q_head=0, q_tail=0; volatile bool sending=false;

inline bool q_empty(){ return q_head==q_tail; }
inline bool q_full(){ return (uint8_t)(q_head+1)==q_tail; }
bool q_push(const uint8_t* b, uint8_t len){
  uint8_t nh = (uint8_t)(q_head+1);
  if(nh==q_tail) return false;
  memcpy(q[q_head].bytes, b, len); q[q_head].len = len; q_head = nh; return true;
}
bool q_front(uint8_t*& b, uint8_t& len){
  if(q_empty()) return false;
  b = q[q_tail].bytes; len = q[q_tail].len; return true;
}
void q_pop(){ if(!q_empty()) q_tail = (uint8_t)(q_tail+1); }

// ------- ESP-NOW -------
void onSent(const uint8_t*, esp_now_send_status_t){
  // nepotrebujeme Serial; rýchlosť uprednostňujeme
  sending = false;   // uvoľni link pre ďalší balík
}

bool try_send_front(){
  if(sending) return true;
  uint8_t* b; uint8_t len;
  if(!q_front(b,len)) return false;
  ledTX.pulse(3);              // "odoslal som batch"
  sending = (esp_now_send(rxMac, b, len) == ESP_OK);
  if(sending) q_pop();         // pop hneď, aby sme neblokovali (ACK neriešime, ideme na rýchlosť)
  return sending;
}

// ------- HC-SR04 -------
static inline float readHCSR04cm(){
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 20000); // ~3.4 m timeout
  if(dur==0) return -1;
  return dur * 0.0343f / 2.f;
}

void setup(){
  ledTX.init(TX_LED);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  dht.begin();
  mpl.begin(); // ak nie je pripojený, batch ho proste vynechá

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) while(1){}
  esp_now_peer_info_t peer{}; memcpy(peer.peer_addr, rxMac, 6);
  peer.ifidx=WIFI_IF_STA; peer.channel=0; peer.encrypt=false;
  esp_now_add_peer(&peer);
  esp_now_register_send_cb(onSent);
}

void loop(){
  const uint32_t now = millis();

  // 1) Zožeň dostupné dáta (bez Serialu) ---------------------------
  PayDHT dhtp; PayMPL mplp; PayHC hcp;
  const PayDHT* pDHT = nullptr; const PayMPL* pMPL = nullptr; const PayHC* pHC = nullptr;

  if (now - lastHC >= INT_HC) {
    lastHC = now;
    float d_cm = readHCSR04cm();
    if (d_cm >= 0) { hcp.dist_mm = (uint16_t) (d_cm * 10.0f); pHC = &hcp; }
  }

  if (now - lastDHT >= INT_DHT) {
    lastDHT = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      dhtp.t_dC       = (int16_t) lroundf(t * 10.0f);
      dhtp.rh_permille= (uint16_t) lroundf(h * 10.0f);
      pDHT = &dhtp;
    }
  }

  if (now - lastMPL >= INT_MPL) {
    lastMPL = now;
    if (mpl.begin()) { // re-init pre prípad, že bol off
      float P = mpl.getPressure();
      float A = mpl.getAltitude();
      float T = mpl.getTemperature();
      mplp.p_Pa  = (int32_t) llround(P);
      mplp.alt_cm= (int16_t) llround(A * 100.0f);
      mplp.t_dC  = (int16_t) llround(T * 10.0f);
      pMPL = &mplp;
    }
  }

  // 2) Ak máme aspoň jeden senzor, zbalíme batch a pushneme do FIFO ---
  if (pDHT || pMPL || pHC) {
    uint8_t buf[32];
    size_t len = build_batch(buf, sizeof(buf), pDHT, pMPL, pHC, ++seqBatch, now);
    if (len) (void)q_push(buf, (uint8_t)len);
  }

  // 3) Skús odoslať čelo fronty --------------------------------------
  try_send_front();

  // 4) Udržiavaj LED pulz
  ledTX.tick();
}
