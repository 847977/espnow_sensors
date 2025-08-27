#include <Arduino.h>
#include "DHT.h"

#define DHTPIN 4         // Pin, kde je pripojený DHT11
#define DHTTYPE DHT11    // Typ senzora

#define RED_PIN 21
#define GREEN_PIN 22
#define BLUE_PIN 23

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();

  // PWM kanály
  ledcAttachPin(RED_PIN, 0);
  ledcAttachPin(GREEN_PIN, 1);
  ledcAttachPin(BLUE_PIN, 2);

  ledcSetup(0, 5000, 8);  // Red
  ledcSetup(1, 5000, 8);  // Green (nepoužíva sa)
  ledcSetup(2, 5000, 8);  // Blue (nepoužíva sa)
}

void loop() {
  float humidity = dht.readHumidity();

  if (isnan(humidity)) {
    Serial.println("Chyba pri čítaní z DHT senzora!");
    delay(1000);
    return;
  }

  Serial.print("Vlhkost: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Default mapovanie vlhkosti na jas (0 – 204, čo je 80 % zo 255)
  // zakomentovat ak chceme kontrolkovu
  //int brightness = map(humidity, 0, 100, 0, 204);
  //brightness = constrain(brightness, 0, 204);

  // Iba kontrolkova LED
  
  if (humidity >= 80) {
    // LED svieti podľa vlhkosti od 90 % do 100 % (max 80 % svietivosti)
    int brightness = map(humidity, 80, 100, 0, 204);  // 204 = 80 % z 255
    brightness = constrain(brightness, 0, 204);
    ledcWrite(0, 255 - brightness);  // invertované pre common anode
  } else {
    // LED vypnutá
    ledcWrite(0, 255);  // HIGH = LED OFF (common anode)
  }

  // Pre common anode LED – invertujeme jas
  //ledcWrite(0, 255 - brightness);   // zakomentovat ak chceme kontrolkovu
  ledcWrite(1, 255);  // zelená vypnutá
  ledcWrite(2, 255);  // modrá vypnutá

  delay(1000);
}
