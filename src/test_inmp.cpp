#include <Arduino.h>
#include "driver/i2s.h"

#define I2S_WS  25
#define I2S_SD  34
#define I2S_SCK 26

#define SAMPLE_RATE     44100
#define SAMPLES         1024

int32_t samples[SAMPLES];

void setup() {
  Serial.begin(115200);

  // Konfigurácia I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  Serial.println("START");
}

void loop() {
  size_t bytes_read;
  i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytes_read, portMAX_DELAY);
  
  long sum = 0;
  int count = bytes_read / 4; // 32 bit vzorka = 4 byty

  for (int i = 0; i < count; i++) {
    int32_t sample = samples[i] >> 14; // Normalizácia
    sum += sample * sample;
  }

  float rms = sqrt((float)sum / count);

  Serial.print("RMS: ");
  Serial.println(rms);

  delay(50);
}
