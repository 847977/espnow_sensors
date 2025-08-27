#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_MPL3115A2.h>
#include <DHT.h>

// ---------- PINY ----------
#define DHTPIN   4
#define DHTTYPE  DHT11
#define TRIG_PIN 5
#define ECHO_PIN 18
// I2C: ESP32 DevKitC default -> SDA=21, SCL=22 (nepoužívaj ich na LED)

// ---------- MAC prijímača (UPRAV podľa RX) ----------
uint8_t rxMac[] = { 0xA8, 0x03, 0x2A, 0x6B, 0x6E, 0x28 };

// ---------- INTERVALY ----------
const unsigned long HC_INTERVAL  = 100;    // 100 ms
const unsigned long DHT_INTERVAL = 1000;   // 1 s (min pre DHT11)
const unsigned long MPL_INTERVAL = 5000;   // 5 s

// ---------- TYPY SPRÁV ----------
enum : uint8_t { MSG_DHT = 0, MSG_MPL = 1, MSG_HC = 2 };

typedef struct {
  uint8_t  typ;     // MSG_DHT / MSG_MPL / MSG_HC
  uint32_t seq;     // čítač pre daný typ
  uint32_t t_ms;    // millis() pri odoslaní
  float    a;       // DHT: teplota | MPL: tlak (Pa) | HC: vzdialenosť (cm)
  float    b;       // DHT: vlhkosť | MPL: výška (m) | HC: rezervované (0)
  float    c;       // DHT: rezerv. | MPL: teplota   | HC: rezervované (0)
} msg_t;

// ---------- SENZORY ----------
DHT dht(DHTPIN, DHTTYPE);
Adafruit_MPL3115A2 mpl;

// ---------- ČASOVAČE / SEKVENCIE ----------
unsigned long lastHC = 0, lastDHT = 0, lastMPL = 0;
uint32_t seqHC = 0, seqDHT = 0, seqMPL = 0;

// ---------- POMOCNÉ ----------
void onSent(const uint8_t* mac, esp_now_send_status_t status) {
  Serial.print("Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

bool sendMsg(const msg_t& m) {
  return esp_now_send(rxMac, (const uint8_t*)&m, sizeof(m)) == ESP_OK;
}

float readHCSR04cm() {
  // krátky, neblokujúci štýl: prudký timeout, nech nebrzdí 100ms tick
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  // timeout 20 ms (20000 us) — zodpovedá cca 3.4 m
  long duration = pulseIn(ECHO_PIN, HIGH, 20000);
  if (duration == 0) return -1.0f; // neúspech
  return (duration * 0.0343f / 2.0f);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n[TX] start (HC=100ms, DHT=1000ms, MPL=5000ms)");

  // HC-SR04 piny
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Senzory
  dht.begin();
  if (!mpl.begin()) {
    Serial.println("MPL3115A2 nenájdený (SDA=21, SCL=22?)");
  }

  // WiFi / ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAIL");
    while (true) delay(1000);
  }
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, rxMac, 6);
  peer.ifidx   = WIFI_IF_STA;
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("esp_now_add_peer FAIL");
    while (true) delay(1000);
  }

  Serial.print("My MAC: "); Serial.println(WiFi.macAddress());
}

void loop() {
  unsigned long now = millis();

  // --- HC-SR04 každých 100 ms ---
  if (now - lastHC >= HC_INTERVAL) {
    lastHC = now;
    float d = readHCSR04cm();
    if (d >= 0) {
      msg_t m{};
      m.typ = MSG_HC;
      m.seq = ++seqHC;
      m.t_ms = now;
      m.a = d;
      m.b = 0;
      m.c = 0;
      Serial.printf("[TX][HC]  seq=%lu  dist=%.1f cm\n", (unsigned long)m.seq, d);
      sendMsg(m);
    } else {
      // nič, ďalší pokus o 100 ms
    }
  }

  // --- DHT11 každých 1000 ms ---
  if (now - lastDHT >= DHT_INTERVAL) {
    lastDHT = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      msg_t m{};
      m.typ = MSG_DHT;
      m.seq = ++seqDHT;
      m.t_ms = now;
      m.a = t;
      m.b = h;
      m.c = 0;
      Serial.printf("[TX][DHT] seq=%lu  T=%.1f C  RH=%.1f %%\n", (unsigned long)m.seq, t, h);
      sendMsg(m);
    } else {
      Serial.println("[TX][DHT] read FAIL");
    }
  }

  // --- MPL3115A2 každých 5000 ms ---
  if (now - lastMPL >= MPL_INTERVAL) {
    lastMPL = now;
    // pozn.: ak MPL nie je pripojený, vynecháme
    if (mpl.begin()) { // re-init pre prípadný reconnect
      float pressure = mpl.getPressure();    // Pa
      float alt      = mpl.getAltitude();    // m
      float t2       = mpl.getTemperature(); // °C

      msg_t m{};
      m.typ = MSG_MPL;
      m.seq = ++seqMPL;
      m.t_ms = now;
      m.a = pressure;
      m.b = alt;
      m.c = t2;

      Serial.printf("[TX][MPL] seq=%lu  P=%.0f Pa  Alt=%.2f m  T=%.2f C\n",
                    (unsigned long)m.seq, pressure, alt, t2);
      sendMsg(m);
    } else {
      Serial.println("[TX][MPL] not ready");
    }
  }

  // žiadne delay(); nechajme CPU voľné pre WiFi/ESP-NOW
}
