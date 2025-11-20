// i2s_init.cpp
// Implementácia inicializácie I2S0 v stereo režime a čítania interleaved blokov.

#include "i2s_init.h"
#include "board_pins.h"
#include "board_config.h"
#include "driver/i2s.h"

namespace audio {

static const i2s_port_t I2S_PORT = I2S_NUM_0;

void initI2S_STEREO() {
    i2s_config_t cfg = {};
    cfg.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate = SAMPLE_RATE;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT; // interleaved R,L,R,L...
    cfg.communication_format =
        i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_I2S_MSB);
    cfg.intr_alloc_flags = 0;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 256;
    cfg.use_apll = true;
    cfg.tx_desc_auto_clear = false;
    cfg.fixed_mclk = 0;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = PIN_I2S_SCK;
    pins.ws_io_num    = PIN_I2S_WS;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num  = PIN_I2S_SD;

    i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
    i2s_set_pin(I2S_PORT, &pins);
    i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
}

void readBlockInterleaved(void* dest, size_t sizeBytes, size_t* outBytes) {
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, dest, sizeBytes, &bytesRead, portMAX_DELAY);
    if (outBytes) {
        *outBytes = bytesRead;
    }
}

}
