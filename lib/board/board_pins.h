// board_pins.h
// Pinová mapa pre I2S (INMP441) a neskôr LED atď.

#pragma once
#include <Arduino.h>

constexpr int PIN_I2S_WS  = 25;  // LRCLK / WS
constexpr int PIN_I2S_SCK = 26;  // BCLK
constexpr int PIN_I2S_SD  = 34;  // SD (data in)
constexpr int PIN_BTN_MIC_ENABLE = 32;  // Sender button: zapnutie/vypnutie mikrofónov
constexpr int PIN_BTN_LED_ENABLE = 18;  // LED ON/OFF
constexpr int PIN_BTN_PALETTE    = 19;  // prepínanie paliet