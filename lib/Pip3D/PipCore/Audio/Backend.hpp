#pragma once

#include <PipCore/Features.hpp>
#include <cstdint>
#include <cstddef>

namespace pipcore::audio
{
    struct BackendConfig
    {
        uint32_t sampleRate = 44100;
        uint8_t i2sPort = 0;
        int8_t bck = -1;
        int8_t ws = -1;
        int8_t dataOut = -1;
    };

    class Backend
    {
    public:
        virtual ~Backend() = default;

        [[nodiscard]] virtual bool init(const BackendConfig &cfg) noexcept = 0;

        virtual void deinit() noexcept = 0;

        [[nodiscard]] virtual bool ready() const noexcept = 0;
        [[nodiscard]] virtual uint32_t sampleRate() const noexcept = 0;

        virtual void writeInterleavedS16(const int16_t *interleavedLR,
                                         size_t frames) noexcept = 0;
    };
}
