#pragma once

#include <PipCore/Features.hpp>
#include <cstdint>
#include <cstddef>

#if defined(ESP_PLATFORM) || defined(ESP32)
#include <esp_attr.h>
#else
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#endif

namespace pipcore::st
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

    [[nodiscard]] inline const char *ioErrorText(IoError error) noexcept
    {
        switch (error)
        {
        case IoError::None:
            return "ok";
        case IoError::InvalidConfig:
            return "invalid config";
        case IoError::NotReady:
            return "transport not ready";
        case IoError::Gpio:
            return "gpio operation failed";
        case IoError::SpiBusInit:
            return "spi bus init failed";
        case IoError::SpiDeviceAdd:
            return "spi device add failed";
        case IoError::DmaBufferAlloc:
            return "dma buffer alloc failed";
        case IoError::TransactionAlloc:
            return "spi transaction alloc failed";
        case IoError::CommandTransmit:
            return "spi command transmit failed";
        case IoError::DataTransmit:
            return "spi data transmit failed";
        case IoError::QueueTransmit:
            return "spi queue transmit failed";
        case IoError::QueueResult:
            return "spi queue result failed";
        case IoError::UnexpectedTransaction:
            return "unexpected spi transaction result";
        default:
            return "unknown st io error";
        }
    }

    [[nodiscard]] inline constexpr uint16_t bswap16(uint16_t v) noexcept 
    { 
        return __builtin_bswap16(v); 
    }

    inline void IRAM_ATTR __attribute__((always_inline)) copySwap565(uint16_t *dst, const uint16_t *src, size_t pixels) noexcept
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
        [[nodiscard]] virtual bool waitOldest() = 0;
        [[nodiscard]] virtual bool writeAddrWindow(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye) = 0;
    };
}

namespace pipcore::detail
{
    enum class StDisplayType : uint8_t
    {
        ST7789,
        ST7796
    };

    template <StDisplayType Type>
    class StDriver
    {
    public:
        using Transport = pipcore::st::Transport;
        using IoError = pipcore::st::IoError;

        StDriver() = default;

        [[nodiscard]] bool configure(Transport *transport,
                                     uint16_t width,
                                     uint16_t height,
                                     uint8_t order = 0,
                                     bool invert = true,
                                     bool swap = false,
                                     int16_t xOffset = 0,
                                     int16_t yOffset = 0)
        {
            if (__builtin_expect(!transport || !width || !height || xOffset < 0 || yOffset < 0, 0))
            {
                reset();
                _lastError = IoError::InvalidConfig;
                return false;
            }

            _transport = transport;
            _width = _physWidth = width;
            _height = _physHeight = height;
            _xStart = _xOffsetCfg = xOffset;
            _yStart = _yOffsetCfg = yOffset;

            _order = static_cast<uint8_t>(order == 1);
            _invert = invert;
            _swap = swap;
            _initialized = false;
            _lastError = IoError::None;
            _transport->clearError();
            return true;
        }

        void reset()
        {
            *this = StDriver();
        }

        [[nodiscard]] bool begin(uint8_t rotation)
        {
            _initialized = false;
            _lastError = IoError::None;

            if (__builtin_expect(!_transport || !_width || !_height, 0))
            {
                _lastError = IoError::InvalidConfig;
                return false;
            }

            _transport->clearError();
            if (__builtin_expect(!_transport->init(), 0))
                return failFromTransport(IoError::NotReady);

            if (__builtin_expect(!hardReset(), 0))
                return false;

            constexpr uint8_t CmdSWRESET = 0x01;
            constexpr uint8_t CmdSLPOUT = 0x11;
            constexpr uint8_t CmdCOLMOD = 0x3A;
            constexpr uint8_t CmdINVON = 0x21;
            constexpr uint8_t CmdINVOFF = 0x20;
            constexpr uint8_t CmdNORON = 0x13;
            constexpr uint8_t CmdDISPON = 0x29;
            constexpr uint8_t Colmod16bpp = 0x55;

            if (__builtin_expect(!sendCommand(CmdSWRESET), 0))
                return false;
            _transport->delayMs(120);

            if (__builtin_expect(!sendCommand(CmdSLPOUT), 0))
                return false;

            if constexpr (Type == StDisplayType::ST7796)
            {
                _transport->delayMs(120);
            }
            else
            {
                _transport->delayMs(10);
            }

            if (__builtin_expect(!sendCommand(CmdCOLMOD), 0))
                return false;
            {
                uint8_t v = Colmod16bpp;
                if (__builtin_expect(!sendBytes(&v, 1), 0))
                    return false;
            }

            if constexpr (Type == StDisplayType::ST7789)
            {
                if (__builtin_expect(!sendCommand(_invert ? CmdINVON : CmdINVOFF), 0))
                    return false;

                if (__builtin_expect(!sendCommand(CmdNORON), 0))
                    return false;

                if (__builtin_expect(!setRotationInternal(rotation), 0))
                    return false;
            }
            else // ST7796
            {
                if (__builtin_expect(!setRotationInternal(rotation), 0))
                    return false;

                if (__builtin_expect(!sendCommand(_invert ? CmdINVON : CmdINVOFF), 0))
                    return false;

                if (__builtin_expect(!sendCommand(CmdNORON), 0))
                    return false;
            }

            if (__builtin_expect(!sendCommand(CmdDISPON), 0))
                return false;
            _transport->delayMs(20);

            _initialized = true;
            _lastError = IoError::None;
            return true;
        }

        [[nodiscard]] bool setRotation(uint8_t rotation)
        {
            if (__builtin_expect(!_transport || !_initialized, 0))
            {
                _lastError = IoError::NotReady;
                return false;
            }
            if (__builtin_expect(!_transport->waitComplete(), 0))
                return false;

            return setRotationInternal(rotation);
        }

        [[nodiscard]] IoError lastError() const noexcept { return _lastError; }
        [[nodiscard]] const char *lastErrorText() const noexcept { return pipcore::st::ioErrorText(_lastError); }

        [[nodiscard]] uint16_t width() const noexcept { return _width; }
        [[nodiscard]] uint16_t height() const noexcept { return _height; }
        [[nodiscard]] bool swapBytes() const noexcept { return _swap; }

        [[nodiscard]] inline bool __attribute__((always_inline)) waitComplete()
        {
            if (!_transport)
                return false;
            return _transport->waitComplete();
        }

        [[nodiscard]] inline bool __attribute__((always_inline)) waitOldest()
        {
            if (!_transport)
                return false;
            return _transport->waitOldest();
        }

        [[nodiscard]] inline bool __attribute__((always_inline)) setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
        {
            if (!_transport || !_initialized || !_width || !_height || x1 < x0 || y1 < y0)
            {
                _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
                return false;
            }
            if (x0 >= _width || y0 >= _height)
                return true;

            if (x1 >= _width)
                x1 = static_cast<uint16_t>(_width - 1U);
            if (y1 >= _height)
                y1 = static_cast<uint16_t>(_height - 1U);

            const uint16_t xs = static_cast<uint16_t>(x0 + _xStart);
            const uint16_t xe = static_cast<uint16_t>(x1 + _xStart);
            const uint16_t ys = static_cast<uint16_t>(y0 + _yStart);
            const uint16_t ye = static_cast<uint16_t>(y1 + _yStart);

            if (!_transport->writeAddrWindow(xs, xe, ys, ye))
            {
                return failFromTransport(IoError::CommandTransmit);
            }

            return true;
        }

        [[nodiscard]] inline bool __attribute__((always_inline)) writePixels565(const uint16_t *pixels, size_t pixelCount)
        {
            if (!_transport || !_initialized || !pixels || !pixelCount)
            {
                _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
                return false;
            }
            return sendPixels(pixels, pixelCount * sizeof(uint16_t));
        }

        [[nodiscard]] inline bool __attribute__((always_inline)) writePixels565Async(const uint16_t *pixels, size_t pixelCount)
        {
            if (!_transport || !_initialized || !pixels || !pixelCount)
            {
                _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
                return false;
            }
            return _transport->writePixelsAsync(pixels, pixelCount * sizeof(uint16_t));
        }

        [[nodiscard]] bool fillScreen565(uint16_t color565, bool swapBytes = false)
        {
            if (__builtin_expect(!_transport || !_initialized || !_width || !_height, 0))
            {
                _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
                return false;
            }

            if (__builtin_expect(!setAddrWindow(0, 0, static_cast<uint16_t>(_width - 1U), static_cast<uint16_t>(_height - 1U)), 0))
                return false;

            const uint16_t v = swapBytes ? pipcore::st::bswap16(color565) : color565;
            const size_t totalPixels = static_cast<size_t>(_width) * static_cast<size_t>(_height);

            if (__builtin_expect(!_transport->fillPixels(v, totalPixels), 0))
                return failFromTransport(IoError::DataTransmit);

            return true;
        }

        void setInversion(bool enabled)
        {
            _invert = enabled;
            if (__builtin_expect(!_transport || !_initialized, 0))
                return;
            if (__builtin_expect(!_transport->waitComplete(), 0))
                return;

            constexpr uint8_t CmdINVON = 0x21;
            constexpr uint8_t CmdINVOFF = 0x20;
            (void)sendCommand(_invert ? CmdINVON : CmdINVOFF);
        }

    private:
        [[nodiscard]] bool hardReset()
        {
            if (__builtin_expect(!_transport, 0))
                return failFromTransport(IoError::NotReady);

            if (__builtin_expect(!_transport->setRst(false), 0))
                return failFromTransport(IoError::Gpio);

            _transport->delayMs(10);

            if (__builtin_expect(!_transport->setRst(true), 0))
                return failFromTransport(IoError::Gpio);

            _transport->delayMs(120);

            return true;
        }

        [[nodiscard]] bool setRotationInternal(uint8_t rotation)
        {
            if (__builtin_expect(!_transport, 0))
                return failFromTransport(IoError::NotReady);

            _rotation = rotation & 3U;

            const bool isOdd = (_rotation & 1U);

            _width = isOdd ? _physHeight : _physWidth;
            _height = isOdd ? _physWidth : _physHeight;
            _xStart = isOdd ? _yOffsetCfg : _xOffsetCfg;
            _yStart = isOdd ? _xOffsetCfg : _yOffsetCfg;

            constexpr uint8_t MadctlMY = 0x80;
            constexpr uint8_t MadctlMX = 0x40;
            constexpr uint8_t MadctlMV = 0x20;
            constexpr uint8_t MadctlBGR = 0x08;
            constexpr uint8_t CmdMADCTL = 0x36;

            const uint8_t bgr = (_order == 1) ? MadctlBGR : 0;
            uint8_t madctl = bgr;

            if constexpr (Type == StDisplayType::ST7789)
            {
                switch (_rotation)
                {
                case 1:
                    madctl |= MadctlMX | MadctlMV;
                    break;
                case 2:
                    madctl |= MadctlMX | MadctlMY;
                    break;
                case 3:
                    madctl |= MadctlMV | MadctlMY;
                    break;
                default:
                    break;
                }
            }
            else // ST7796
            {
                switch (_rotation)
                {
                case 1:
                    madctl |= MadctlMV;
                    break;
                case 2:
                    madctl |= MadctlMX | MadctlMY;
                    break;
                case 3:
                    madctl |= MadctlMX | MadctlMY | MadctlMV;
                    break;
                default:
                    break;
                }
            }

            if (__builtin_expect(!sendCommand(CmdMADCTL), 0))
                return false;
            if (__builtin_expect(!sendBytes(&madctl, 1), 0))
                return false;

            return true;
        }

        [[nodiscard]] inline bool __attribute__((always_inline)) failFromTransport(IoError fallback)
        {
            IoError err = fallback;
            if (_transport)
            {
                const IoError transportErr = _transport->lastError();
                if (transportErr != IoError::None)
                {
                    err = transportErr;
                }
            }
            _lastError = err;
            return false;
        }

        inline bool __attribute__((always_inline)) sendCommand(uint8_t cmd)
        {
            if (!_transport)
                return failFromTransport(IoError::NotReady);
            if (_transport->writeCommand(cmd))
                return true;
            return failFromTransport(IoError::CommandTransmit);
        }

        inline bool __attribute__((always_inline)) sendBytes(const void *data, size_t len)
        {
            if (!_transport)
                return failFromTransport(IoError::NotReady);
            if (_transport->write(data, len))
                return true;
            return failFromTransport(IoError::DataTransmit);
        }

        inline bool __attribute__((always_inline)) sendPixels(const void *data, size_t len)
        {
            if (!_transport)
                return failFromTransport(IoError::NotReady);
            if (_transport->writePixels(data, len))
                return true;
            return failFromTransport(IoError::DataTransmit);
        }

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