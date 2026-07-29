#include <PipCore/Features.hpp>

#if PIPCORE_TARGET_ESP32

#include <PipCore/Platforms/ESP32/Services/Audio.hpp>

#include <Arduino.h>

namespace pipcore::esp32::services
{
    Audio::~Audio() { deinit(); }

    bool Audio::init(const audio::BackendConfig &cfg) noexcept
    {
        deinit();

        _port = static_cast<i2s_port_t>(cfg.i2sPort);
        _sampleRate = cfg.sampleRate;

        Serial.printf("[AUDIO] I2S port=%u rate=%u bck=%d ws=%d dout=%d\n",
                      (unsigned)cfg.i2sPort, (unsigned)_sampleRate,
                      (int)cfg.bck, (int)cfg.ws, (int)cfg.dataOut);

        i2s_config_t i2s_config = {
            .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = _sampleRate,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = 64,
            .use_apll = false,
            .tx_desc_auto_clear = true,
            .fixed_mclk = 0};

        i2s_pin_config_t pin_config = {
            .bck_io_num = cfg.bck,
            .ws_io_num = cfg.ws,
            .data_out_num = cfg.dataOut,
            .data_in_num = I2S_PIN_NO_CHANGE};

        esp_err_t err = i2s_driver_install(_port, &i2s_config, 0, nullptr);
        if (err != ESP_OK)
        {
            Serial.printf("[AUDIO ERROR] i2s_driver_install: %d\n", err);
            return false;
        }
        _installed = true;

        err = i2s_set_pin(_port, &pin_config);
        if (err != ESP_OK)
        {
            Serial.printf("[AUDIO ERROR] i2s_set_pin: %d\n", err);
            i2s_driver_uninstall(_port);
            _installed = false;
            return false;
        }

        i2s_zero_dma_buffer(_port);
        _ready = true;
        Serial.println("[AUDIO] I2S backend ready");
        return true;
    }

    void Audio::deinit() noexcept
    {
        _ready = false;
        if (_installed)
        {
            i2s_driver_uninstall(_port);
            _installed = false;
        }
    }

    void Audio::writeInterleavedS16(const int16_t *interleavedLR, size_t frames) noexcept
    {
        if (!_ready || !interleavedLR || frames == 0)
            return;

        const size_t bytesWanted = frames * 2 * sizeof(int16_t);
        size_t bytesWritten = 0;

        (void)i2s_write(_port, interleavedLR, bytesWanted, &bytesWritten, portMAX_DELAY);
    }
}

#endif
