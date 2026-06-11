#pragma once

#include <cstdint>
#include <cstddef>

#if defined(ESP_PLATFORM) || defined(ESP32)
#include <esp_attr.h>
#else
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#endif

namespace pipcore::st7789
{
    enum class IoError : uint8_t
    {
        None = 0,
        InvalidConfig,
        NotReady,
        Gpio,
        SpiBusInit,
        SpiDeviceAdd,
        DmaBufferAlloc,
        TransactionAlloc,
        CommandTransmit,
        DataTransmit,
        QueueTransmit,
        QueueResult,
        UnexpectedTransaction
    };

    [[nodiscard]] const char *ioErrorText(IoError error) noexcept;

    [[nodiscard]] inline constexpr uint16_t bswap16(uint16_t v) noexcept { return __builtin_bswap16(v); }

    inline void IRAM_ATTR copySwap565(uint16_t *dst, const uint16_t *src, size_t pixels) noexcept
    {
        if (pixels == 0)
            return;

        const bool canUse32 = (((reinterpret_cast<uintptr_t>(src) | reinterpret_cast<uintptr_t>(dst)) & 3U) == 0U);

        if (canUse32)
        {
            auto *dst32 = reinterpret_cast<uint32_t *>(dst);
            auto *src32 = reinterpret_cast<const uint32_t *>(src);
            size_t pairs = pixels >> 1;

            while (pairs--)
            {
                __builtin_prefetch(src32 + 8, 0, 0);
                const uint32_t p = __builtin_bswap32(*src32++);
                *dst32++ = (p >> 16) | (p << 16);
            }

            src = reinterpret_cast<const uint16_t *>(src32);
            dst = reinterpret_cast<uint16_t *>(dst32);
            pixels &= 1U;
        }

        while (pixels--)
            *dst++ = bswap16(*src++);
    }

    class Transport
    {
    public:
        virtual ~Transport() = default;
        [[nodiscard]] virtual bool init() = 0;
        virtual void deinit() = 0;
        [[nodiscard]] virtual IoError lastError() const = 0;
        virtual void clearError() = 0;
        [[nodiscard]] virtual bool setRst(bool level) = 0;
        virtual void delayMs(uint32_t ms) = 0;
        [[nodiscard]] virtual bool write(const void *data, size_t len) = 0;
        [[nodiscard]] virtual bool writeCommand(uint8_t cmd) = 0;
        [[nodiscard]] virtual bool writePixels(const void *data, size_t len) = 0;
        [[nodiscard]] virtual bool fillPixels(uint16_t color, size_t count) = 0;
        [[nodiscard]] virtual bool acquireBus() = 0;
        virtual void releaseBus() = 0;
        [[nodiscard]] virtual bool flush() = 0;
        [[nodiscard]] virtual bool writePixelsAsync(const void *data, size_t len) = 0;
        [[nodiscard]] virtual bool waitComplete() = 0;
        [[nodiscard]] virtual bool writeAddrWindow(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye) = 0;
    };

    class Driver
    {
    public:
        Driver() = default;

        [[nodiscard]] bool configure(Transport *transport,
                                     uint16_t width,
                                     uint16_t height,
                                     uint8_t order = 0,
                                     bool invert = true,
                                     bool swap = false,
                                     int16_t xOffset = 0,
                                     int16_t yOffset = 0);

        [[nodiscard]] bool begin(uint8_t rotation);
        [[nodiscard]] bool setRotation(uint8_t rotation);
        void reset();
        [[nodiscard]] IoError lastError() const noexcept { return _lastError; }
        [[nodiscard]] const char *lastErrorText() const noexcept { return ioErrorText(_lastError); }

        [[nodiscard]] uint16_t width() const noexcept { return _width; }
        [[nodiscard]] uint16_t height() const noexcept { return _height; }
        [[nodiscard]] bool swapBytes() const noexcept { return _swap; }

        [[nodiscard]] bool setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

        [[nodiscard]] bool writePixels565(const uint16_t *pixels, size_t pixelCount);
        [[nodiscard]] bool writePixels565Async(const uint16_t *pixels, size_t pixelCount);
        [[nodiscard]] bool waitComplete();

        [[nodiscard]] bool fillScreen565(uint16_t color565, bool swapBytes = false);

        void setInversion(bool enabled);

    private:
        [[nodiscard]] bool hardReset();
        [[nodiscard]] bool setRotationInternal(uint8_t rotation);
        [[nodiscard]] bool sendCommand(uint8_t cmd);
        [[nodiscard]] bool sendBytes(const void *data, size_t len);
        [[nodiscard]] bool sendPixels(const void *data, size_t len);
        [[nodiscard]] bool failFromTransport(IoError fallback);

    private:
        Transport *_transport = nullptr;

        uint16_t _width = 0;
        uint16_t _height = 0;
        uint16_t _physWidth = 0;
        uint16_t _physHeight = 0;

        uint8_t _rotation = 0;
        int16_t _xStart = 0;
        int16_t _yStart = 0;
        int16_t _xOffsetCfg = 0;
        int16_t _yOffsetCfg = 0;

        uint8_t _order = 0;
        bool _invert = true;
        bool _swap = false;
        bool _initialized = false;
        IoError _lastError = IoError::None;
    };
}