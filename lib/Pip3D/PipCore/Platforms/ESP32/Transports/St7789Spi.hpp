#pragma once

#include <PipCore/Displays/ST7789/Driver.hpp>

#if !defined(ESP32)
#error "pipcore::esp32::St7789Spi requires ESP32"
#endif

#include <driver/spi_master.h>

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
        [[nodiscard]] bool fillPixels(uint16_t color, size_t count) override;
        [[nodiscard]] bool acquireBus() override;
        void releaseBus() override;
        [[nodiscard]] inline bool flush() override { return waitComplete(); }

        [[nodiscard]] inline bool IRAM_ATTR __attribute__((always_inline)) writePixels(const void *data, size_t len) override
        {
            return writePixelsImpl(data, len, true);
        }

        [[nodiscard]] inline bool IRAM_ATTR __attribute__((always_inline)) writePixelsAsync(const void *data, size_t len) override
        {
            return writePixelsImpl(data, len, false);
        }

        [[nodiscard]] bool waitComplete() override;
        [[nodiscard]] bool waitOldest() override;
        [[nodiscard]] bool writeAddrWindow(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye) override;

    private:
        [[nodiscard]] bool initSpi();
        [[nodiscard]] bool drainQueue();
        [[nodiscard]] bool fail(st7789::IoError error);
        [[nodiscard]] bool writePixelsImpl(const void *data, size_t len, bool useDmaBufferIfNonCapable);

        int8_t _pinMosi = -1;
        int8_t _pinSclk = -1;
        int8_t _pinCs = -1;
        int8_t _pinDc = -1;
        int8_t _pinRst = -1;
        uint32_t _hz = 80000000U;

        void *_spiHandle = nullptr;
        uint8_t *_dmaBuf[2] = {nullptr, nullptr};
        bool _busAcquired = false;
        bool _initialized = false;
        st7789::IoError _lastError = st7789::IoError::None;

        static constexpr size_t HardwareMaxDmaBytes = 65536U;
        static constexpr size_t DmaBufferBytes = 16384U;
        static constexpr int MaxAsyncTrans = 2;
        static constexpr int MaxDmaBufs = 2;

        spi_transaction_t _asyncTrans[MaxAsyncTrans]{};
        spi_transaction_t _addrTrans[5]{};
        int _asyncNext = 0;
        int _asyncInFlight = 0;

        uint16_t _lastXs = 0xFFFF;
        uint16_t _lastXe = 0xFFFF;
        uint16_t _lastYs = 0xFFFF;
        uint16_t _lastYe = 0xFFFF;
    };
}