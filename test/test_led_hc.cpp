#include <Arduino.h>

#define TRIG_PIN 5
#define ECHO_PIN 18

#define RED_PIN 21
#define GREEN_PIN 22
#define BLUE_PIN 23

long duration;
float distance;

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // PWM setup
  ledcAttachPin(RED_PIN, 0);
  ledcAttachPin(GREEN_PIN, 1);
  ledcAttachPin(BLUE_PIN, 2);

  ledcSetup(0, 5000, 8);  // RED
  ledcSetup(1, 5000, 8);  // GREEN
  ledcSetup(2, 5000, 8);  // BLUE
}

void loop() {
  // Trigger ultrazvuku
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Echo meranie
  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration * 0.0343 / 2;

  Serial.print("Vzdialenost: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Farebné zóny
  if (distance <= 77.3) {
    // Blízko – červená
    ledcWrite(0, 255 - 204);  // RED ON (80 %)
    ledcWrite(1, 255);        // GREEN OFF
    ledcWrite(2, 255);        // BLUE OFF

  } else if (distance <= 152.6) {
    // Stred – žltá (červená + zelená)
    ledcWrite(0, 255 - 204);  // RED ON
    ledcWrite(1, 255 - 204);  // GREEN ON
    ledcWrite(2, 255);        // BLUE OFF

  } else {
    // Ďaleko – zelená
    ledcWrite(0, 255);        // RED OFF
    ledcWrite(1, 255 - 204);  // GREEN ON
    ledcWrite(2, 255);        // BLUE OFF
  }

  delay(500);
}
