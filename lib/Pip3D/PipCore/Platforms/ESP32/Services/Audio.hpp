#pragma once

#include <PipCore/Features.hpp>

#if !PIPCORE_TARGET_ESP32
#error "pipcore::esp32::services::Audio requires ESP32 target"
#endif

#include <PipCore/Audio/Backend.hpp>

#include <driver/i2s.h>
#include <stdint.h>

namespace pipcore::esp32::services
{
    class Audio final : public audio::Backend
    {
    public:
        Audio() = default;
        ~Audio() override;

        [[nodiscard]] bool init(const audio::BackendConfig &cfg) noexcept override;
        void deinit() noexcept override;

        [[nodiscard]] bool ready() const noexcept override { return _ready; }
        [[nodiscard]] uint32_t sampleRate() const noexcept override { return _sampleRate; }

        void writeInterleavedS16(const int16_t *interleavedLR, size_t frames) noexcept override;

    private:
        i2s_port_t _port = I2S_NUM_0;
        uint32_t _sampleRate = 44100;
        bool _installed = false;
        bool _ready = false;
    };
}
