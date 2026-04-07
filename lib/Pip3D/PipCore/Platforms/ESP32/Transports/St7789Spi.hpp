#pragma once

#include <PipCore/Displays/ST7789/Driver.hpp>

#if !defined(ESP32)
#error "pipcore::esp32::St7789Spi requires ESP32"
#endif

struct spi_transaction_t;

namespace pipcore::esp32
{
    class St7789Spi final : public st7789::Transport
    {
    public:
        St7789Spi() = default;
        ~St7789Spi();

        void configure(int8_t mosi, int8_t sclk, int8_t cs, int8_t dc, int8_t rst, uint32_t hz = 80000000) noexcept;

        [[nodiscard]] bool init() override;
        void deinit() override;
        [[nodiscard]] st7789::IoError lastError() const noexcept override { return _lastError; }
        void clearError() noexcept override { _lastError = st7789::IoError::None; }
        [[nodiscard]] bool setRst(bool level) override;
        void delayMs(uint32_t ms) override;
        [[nodiscard]] bool write(const void *data, size_t len) override;
        [[nodiscard]] bool writeCommand(uint8_t cmd) override;
        [[nodiscard]] bool writePixels(const void *data, size_t len) override;
        [[nodiscard]] bool acquireBus() override;
        void releaseBus() override;
        [[nodiscard]] bool flush() override;

    private:
        [[nodiscard]] bool initSpi();
        [[nodiscard]] bool waitQueued();
        [[nodiscard]] bool flushQueued();
        [[nodiscard]] bool fail(st7789::IoError error);
        [[nodiscard]] inline bool setDcCached(int level);

        int8_t _pinMosi = -1;
        int8_t _pinSclk = -1;
        int8_t _pinCs = -1;
        int8_t _pinDc = -1;
        int8_t _pinRst = -1;
        uint32_t _hz = 80000000U;

        void *_spiHandle = nullptr;
        size_t _dmaBufSize = 8192;
        uint8_t *_dmaBuf[2] = {nullptr, nullptr};
        spi_transaction_t *_trans[2] = {nullptr, nullptr};
        bool _transInFlight[2] = {false, false};
        int _dmaNext = 0;
        int _dmaInflight = 0;
        int8_t _dcLevel = -1;
        bool _busAcquired = false;
        bool _initialized = false;
        st7789::IoError _lastError = st7789::IoError::None;
    };
}
