#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPL3115A2.h>
#include <DHT.h>

// DHT11 konfigurácia
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// MPL3115A2 konfigurácia
Adafruit_MPL3115A2 baroSensor = Adafruit_MPL3115A2();

// HC-SR04 konfigurácia
#define TRIG_PIN 5
#define ECHO_PIN 18

// Časovače
unsigned long lastHCRead = 0;
unsigned long lastEnvRead = 0;

const unsigned long hcInterval = 100;       // 100ms pre HC-SR04
const unsigned long envInterval = 5000;     // 5s pre DHT11 a MPL3115A2

void setup() {
  Serial.begin(115200);

  // Inicializácia senzorov
  dht.begin();
  if (!baroSensor.begin()) {
    Serial.println("Nepodarilo sa nájsť MPL3115A2 senzor. Skontroluj spojenie.");
  }

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

float readHCSR04() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout po 30ms
  if (duration == 0) return -1; // neúspešné meranie

  float distance_cm = duration * 0.034 / 2.0;
  return distance_cm;
}

void loop() {
  unsigned long currentMillis = millis();

  // HC-SR04 každých 100ms
  if (currentMillis - lastHCRead >= hcInterval) {
    lastHCRead = currentMillis;
    float vzdialenost = readHCSR04();
    if (vzdialenost >= 0) {
      Serial.print("Vzdialenosť [cm]: ");
      Serial.println(vzdialenost);
    } else {
      Serial.println("Nepodarilo sa odčítať vzdialenosť.");
    }
  }

  // DHT11 a MPL3115A2 každých 5s
  if (currentMillis - lastEnvRead >= envInterval) {
    lastEnvRead = currentMillis;

    // DHT11
    float teplota = dht.readTemperature();
    float vlhkost = dht.readHumidity();

    if (isnan(teplota) || isnan(vlhkost)) {
      Serial.println("Chyba pri čítaní z DHT11.");
    } else {
      Serial.print("DHT11 -> Teplota: ");
      Serial.print(teplota);
      Serial.print(" °C, Vlhkosť: ");
      Serial.print(vlhkost);
      Serial.println(" %");
    }

    // MPL3115A2
    float tlak = baroSensor.getPressure();
    float vyska = baroSensor.getAltitude();
    float teplota2 = baroSensor.getTemperature();

    Serial.print("MPL3115A2 -> Tlak: ");
    Serial.print(tlak);
    Serial.print(" Pa, Výška: ");
    Serial.print(vyska);
    Serial.print(" m, Teplota: ");
    Serial.print(teplota2);
    Serial.println(" °C");
  }
}
