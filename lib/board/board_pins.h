// board_pins.h
// Pinová mapa pre I2S (INMP441) a neskôr LED atď.

#pragma once
#include <Arduino.h>

constexpr int PIN_I2S_WS  = 25;  // LRCLK / WS
constexpr int PIN_I2S_SCK = 26;  // BCLK
constexpr int PIN_I2S_SD  = 34;  // SD (data in)
constexpr int PIN_BTN_REC = 32;  // Tlačidlo (momentary) pre toggle nahrávania
constexpr int PIN_LED_REC = 27;  // LEDka, ktorá svieti počas nahrávania
