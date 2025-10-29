#include <Arduino.h>
#include "driver/i2s.h"

// ==== PIN DEFINÍCIE ====
// INMP441 → ESP32 (I2S0)
#define I2S_WS      25  // LRCLK (Word Select)
#define I2S_SD      34  // SD (Data In)
#define I2S_SCK     26  // BCLK (Bit Clock)

// ==== I2S KONFIGURÁCIA ====
const i2s_port_t I2S_PORT = I2S_NUM_0;
const int SAMPLE_RATE = 44100;
const int SAMPLES_PER_BLOCK = 1024;

// ==== BUFFER ====
int32_t samples[SAMPLES_PER_BLOCK];

// ---------- Pomocné výpočty ----------
static inline int32_t signExtend24(int32_t v32) {
  int32_t s = v32 >> 8;                 // zahodiť 8 LSB padov
  if (s & 0x00800000) s |= 0xFF000000;  // manuálny sign-extend z 24 na 32 bit
  return s;
}

struct FrameStats {
  double mean;      // DC pred odčítaním
  double rms;       // RMS po odstránení DC
  int32_t peakAbs;  // max |x - mean|
  double dbfs;      // 20*log10(rms/FS)
};

const double FULL_SCALE = 8388608.0; // 2^23

FrameStats analyzeFrame(const int32_t* raw32, int N) {
  // 1) DC (mean) z 24-bit sign-extended vzoriek
  long double sum = 0.0L;
  for (int i = 0; i < N; ++i) {
    sum += (long double)signExtend24(raw32[i]);
  }
  double mean = (double)(sum / (long double)N);

  // 2) RMS + peak po odčítaní DC
  long double acc = 0.0L;
  int32_t peakAbs = 0;
  for (int i = 0; i < N; ++i) {
    int32_t s = signExtend24(raw32[i]);
    double x = (double)s - mean;
    acc += (long double)(x * x);
    int32_t a = (int32_t)llabs((long long)(int32_t)x);
    if (a > peakAbs) peakAbs = a;
  }
  double rms = sqrt((double)(acc / (long double)N));

  // 3) dBFS (ochrana pred log(0))
  double dbfs = (rms > 1.0) ? 20.0 * log10(rms / FULL_SCALE) : -120.0;

  return FrameStats{ mean, rms, peakAbs, dbfs };
}

double g_noiseFloorRMS = -1.0;
// ---------- Jednorazová kalibrácia ticha ----------
double calibrateNoiseFloorRMS(int blocks, int samplesPerBlock, i2s_port_t port, int32_t* buf) {
  long double acc = 0.0L;
  int frames = 0;
  for (int b = 0; b < blocks; ++b) {
    size_t bytes = 0;
    i2s_read(port, buf, samplesPerBlock * sizeof(int32_t), &bytes, portMAX_DELAY);
    int N = bytes / 4;
    FrameStats st = analyzeFrame(buf, N);
    acc += st.rms;
    frames++;
  }
  return (double)(acc / (long double)frames);
}


void setup() {
  Serial.begin(115200);

  // 1️⃣ Nastavenie I2S rozhrania (formát + DMA)
  i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,         // default interrupt priority
    .dma_buf_count = 8,            // počet DMA bufferov
    .dma_buf_len = 256,            // dĺžka každého bufferu
    .use_apll = true,              // presnejší PLL pre audio
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  // 2️⃣ Nastavenie pinov pre I2S
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  // 3️⃣ Inicializácia I2S periférie
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);

  Serial.println("I2S inicializované. Čítam dáta z INMP441...");
}

void loop() {
  static bool calibrated = false;
  size_t bytes = 0;

  // Prečítaj rámec (napr. 1024 vzoriek po 32 bitoch)
  i2s_read(I2S_PORT, samples, sizeof(samples), &bytes, portMAX_DELAY);
  int N = bytes / 4;

  if (!calibrated) {
    delay(250); // nech sa ustáli
    g_noiseFloorRMS = calibrateNoiseFloorRMS(30, SAMPLES_PER_BLOCK, I2S_PORT, samples); // ~0.7 s
    calibrated = true;
    Serial.printf("Noise floor RMS (counts): %.0f  (≈ %.1f dBFS)\n",
                  g_noiseFloorRMS, (g_noiseFloorRMS>1.0)? 20*log10(g_noiseFloorRMS/FULL_SCALE) : -120.0);
  }

  FrameStats st = analyzeFrame(samples, N);

  // Jednoduchá indikácia "hrá/nehhrá" – prah 1.5× noise
  bool active = (st.rms > 1.5 * g_noiseFloorRMS);

  Serial.printf("mean=%.0f  rms=%.0f  peak=%d  dBFS=%.1f  %s\n",
                st.mean, st.rms, st.peakAbs, st.dbfs, active ? "[ACTIVE]" : "[quiet]");
}
