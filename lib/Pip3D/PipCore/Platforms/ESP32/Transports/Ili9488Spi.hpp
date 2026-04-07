#pragma once

#include <PipCore/Displays/ILI9488/Driver.hpp>

#if !defined(ESP32)
#error "pipcore::esp32::Ili9488Spi requires ESP32"
#endif

struct spi_transaction_t;

namespace pipcore::esp32
{
    class Ili9488Spi final : public ili9488::Transport
    {
    public:
        Ili9488Spi() = default;
        ~Ili9488Spi();

        void configure(int8_t mosi, int8_t sclk, int8_t cs, int8_t dc, int8_t rst, uint32_t hz = 60000000U) noexcept;

        [[nodiscard]] bool init() override;
        void deinit() override;
        [[nodiscard]] ili9488::IoError lastError() const override { return _lastError; }
        void clearError() override { _lastError = ili9488::IoError::None; }
        [[nodiscard]] bool setRst(bool level) override;
        void delayMs(uint32_t ms) override;
        [[nodiscard]] bool write(const void *data, size_t len) override;
        [[nodiscard]] bool writeCommand(uint8_t cmd) override;
        [[nodiscard]] bool writePixels(const void *data, size_t len) override;
        [[nodiscard]] bool supportsDirectPixels() const noexcept override { return _useDma; }
        [[nodiscard]] uint8_t *directPixelsBuffer(size_t &capacity) override;
        [[nodiscard]] bool submitDirectPixels(size_t len) override;
        [[nodiscard]] bool acquireBus() override;
        void releaseBus() override;
        [[nodiscard]] bool isBusAcquired() const noexcept override { return _busAcquired; }
        [[nodiscard]] bool flush() override;
        [[nodiscard]] bool useDma() const noexcept override { return _useDma; }
        [[nodiscard]] size_t preferredChunkBytes() const noexcept override { return _useDma ? DmaChunkBytes : PollChunkBytes; }

    private:
        [[nodiscard]] bool initSpi();
        [[nodiscard]] bool initDmaPath();
        [[nodiscard]] bool initPollingPath();
        [[nodiscard]] bool waitQueued();
        [[nodiscard]] bool flushQueued();
        [[nodiscard]] bool fail(ili9488::IoError error);
        [[nodiscard]] inline bool setDcCached(int level);
        [[nodiscard]] inline bool setCsCached(int level);

    private:
        static constexpr size_t DmaChunkBytes = 2304U;
        static constexpr size_t PollChunkBytes = 96U;

        int8_t _pinMosi = -1;
        int8_t _pinSclk = -1;
        int8_t _pinCs = -1;
        int8_t _pinDc = -1;
        int8_t _pinRst = -1;
        uint32_t _hz = 60000000U;
        uint32_t _effectiveHz = 0U;

        void *_spiHandle = nullptr;
        uint8_t *_dmaBuf[2] = {nullptr, nullptr};
        spi_transaction_t *_trans[2] = {nullptr, nullptr};
        bool _transInFlight[2] = {false, false};
        int _dmaNext = 0;
        int _dmaInflight = 0;
        int8_t _csLevel = -1;
        int8_t _dcLevel = -1;
        bool _busAcquired = false;
        bool _initialized = false;
        bool _useDma = false;
        ili9488::IoError _lastError = ili9488::IoError::None;
    };
}
