#include <PipCore/Config/Features.hpp>

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789

#include <PipCore/Displays/ST7789/Driver.hpp>
#include <algorithm>

namespace pipcore::st7789
{
    namespace
    {
        constexpr uint8_t CmdSWRESET = 0x01;
        constexpr uint8_t CmdSLPOUT = 0x11;
        constexpr uint8_t CmdNORON = 0x13;
        constexpr uint8_t CmdINVOFF = 0x20;
        constexpr uint8_t CmdINVON = 0x21;
        constexpr uint8_t CmdDISPON = 0x29;
        constexpr uint8_t CmdCASET = 0x2A;
        constexpr uint8_t CmdRASET = 0x2B;
        constexpr uint8_t CmdRAMWR = 0x2C;
        constexpr uint8_t CmdMADCTL = 0x36;
        constexpr uint8_t CmdCOLMOD = 0x3A;
        constexpr uint8_t Colmod16bpp = 0x55;
        constexpr uint8_t MadctlMY = 0x80;
        constexpr uint8_t MadctlMX = 0x40;
        constexpr uint8_t MadctlMV = 0x20;
        constexpr uint8_t MadctlBGR = 0x08;

        [[nodiscard]] inline constexpr uint16_t bswap16(uint16_t v) noexcept { return __builtin_bswap16(v); }

        inline void pack16BE(uint8_t *buf, uint16_t a, uint16_t b) noexcept
        {
            buf[0] = static_cast<uint8_t>(a >> 8);
            buf[1] = static_cast<uint8_t>(a & 0xFF);
            buf[2] = static_cast<uint8_t>(b >> 8);
            buf[3] = static_cast<uint8_t>(b & 0xFF);
        }

        inline void copySwap565(uint16_t *dst, const uint16_t *src, size_t pixels) noexcept
        {
            if (pixels == 0)
                return;

            const bool canUse32 = (((reinterpret_cast<uintptr_t>(src) | reinterpret_cast<uintptr_t>(dst)) & 1U) == 0U) &&
                                  ((reinterpret_cast<uintptr_t>(src) & 2U) == (reinterpret_cast<uintptr_t>(dst) & 2U));

            if (canUse32)
            {
                if ((reinterpret_cast<uintptr_t>(src) & 2U) != 0U)
                {
                    *dst++ = bswap16(*src++);
                    --pixels;
                }

                auto *dst32 = reinterpret_cast<uint32_t *>(dst);
                auto *src32 = reinterpret_cast<const uint32_t *>(src);
                size_t pairs = pixels >> 1;

                while (pairs--)
                {
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
    }

    const char *ioErrorText(IoError error) noexcept
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
            return "unknown st7789 io error";
        }
    }

    void Driver::reset()
    {
        _transport = nullptr;
        _width = 0;
        _height = 0;
        _physWidth = 0;
        _physHeight = 0;
        _rotation = 0;
        _xStart = 0;
        _yStart = 0;
        _xOffsetCfg = 0;
        _yOffsetCfg = 0;
        _order = 0;
        _invert = true;
        _swap = false;
        _initialized = false;
        _lastError = IoError::None;
    }

    bool Driver::failFromTransport(IoError fallback)
    {
        _lastError = (_transport && _transport->lastError() != IoError::None)
                         ? _transport->lastError()
                         : fallback;
        return false;
    }

    bool Driver::sendCommand(uint8_t cmd)
    {
        if (!_transport)
            return failFromTransport(IoError::NotReady);
        if (_transport->writeCommand(cmd))
            return true;
        return failFromTransport(IoError::CommandTransmit);
    }

    bool Driver::sendBytes(const void *data, size_t len)
    {
        if (!_transport)
            return failFromTransport(IoError::NotReady);
        if (_transport->write(data, len))
            return true;
        return failFromTransport(IoError::DataTransmit);
    }

    bool Driver::sendPixels(const void *data, size_t len)
    {
        if (!_transport)
            return failFromTransport(IoError::NotReady);
        if (_transport->writePixels(data, len))
            return true;
        return failFromTransport(IoError::DataTransmit);
    }

    bool Driver::flushTransport()
    {
        if (!_transport)
            return failFromTransport(IoError::NotReady);
        if (_transport->flush())
            return true;
        return failFromTransport(IoError::QueueResult);
    }

    bool Driver::configure(Transport *transport,
                           uint16_t width, uint16_t height, uint8_t order,
                           bool invert, bool swap,
                           int16_t xOffset, int16_t yOffset)
    {
        if (!transport || !width || !height || xOffset < 0 || yOffset < 0)
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
        _order = (order == 1) ? 1 : 0;
        _invert = invert;
        _swap = swap;
        _initialized = false;
        _lastError = IoError::None;
        _transport->clearError();
        return true;
    }

    bool Driver::hardReset()
    {
        if (!_transport)
            return failFromTransport(IoError::NotReady);
        if (!_transport->setRst(false))
            return failFromTransport(IoError::Gpio);
        _transport->delayMs(20);
        if (!_transport->setRst(true))
            return failFromTransport(IoError::Gpio);
        _transport->delayMs(150);
        return true;
    }

    bool Driver::begin(uint8_t rotation)
    {
        _initialized = false;
        _lastError = IoError::None;

        if (!_transport || !_width || !_height)
        {
            _lastError = IoError::InvalidConfig;
            return false;
        }

        _transport->clearError();
        if (!_transport->init())
            return failFromTransport(IoError::NotReady);

        if (!hardReset())
            return false;

        if (!sendCommand(CmdSWRESET))
            return false;
        _transport->delayMs(150);

        if (!sendCommand(CmdSLPOUT))
            return false;
        _transport->delayMs(200);

        if (!sendCommand(CmdCOLMOD))
            return false;
        {
            uint8_t v = Colmod16bpp;
            if (!sendBytes(&v, 1))
                return false;
        }
        _transport->delayMs(10);

        if (!sendCommand(_invert ? CmdINVON : CmdINVOFF))
            return false;
        _transport->delayMs(10);

        if (!sendCommand(CmdNORON))
            return false;
        _transport->delayMs(10);

        if (!setRotationInternal(rotation))
            return false;

        if (!sendCommand(CmdDISPON))
            return false;
        _transport->delayMs(100);

        _initialized = true;
        _lastError = IoError::None;
        return true;
    }

    bool Driver::setRotation(uint8_t rotation)
    {
        if (!_transport || !_initialized)
        {
            _lastError = IoError::NotReady;
            return false;
        }
        return setRotationInternal(rotation);
    }

    void Driver::setInversion(bool enabled)
    {
        _invert = enabled;
        if (!_transport || !_initialized)
            return;
        if (!sendCommand(_invert ? CmdINVON : CmdINVOFF))
            return;
        _transport->delayMs(10);
    }

    bool Driver::setRotationInternal(uint8_t rotation)
    {
        if (!_transport)
            return failFromTransport(IoError::NotReady);

        _rotation = rotation & 3U;

        const uint8_t bgr = (_order == 1) ? MadctlBGR : 0;

        uint8_t madctl;
        switch (_rotation)
        {
        case 0:
            madctl = bgr;
            _width = _physWidth;
            _height = _physHeight;
            _xStart = _xOffsetCfg;
            _yStart = _yOffsetCfg;
            break;
        case 1:
            madctl = MadctlMX | MadctlMV | bgr;
            _width = _physHeight;
            _height = _physWidth;
            _xStart = _yOffsetCfg;
            _yStart = _xOffsetCfg;
            break;
        case 2:
            madctl = MadctlMX | MadctlMY | bgr;
            _width = _physWidth;
            _height = _physHeight;
            _xStart = _xOffsetCfg;
            _yStart = _yOffsetCfg;
            break;
        case 3:
            madctl = MadctlMV | MadctlMY | bgr;
            _width = _physHeight;
            _height = _physWidth;
            _xStart = _yOffsetCfg;
            _yStart = _xOffsetCfg;
            break;
        default:
            _lastError = IoError::InvalidConfig;
            return false;
        }

        if (!sendCommand(CmdMADCTL))
            return false;
        if (!sendBytes(&madctl, 1))
            return false;
        return true;
    }

    bool Driver::setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
    {
        if (!_transport || !_initialized || !_width || !_height || x1 < x0 || y1 < y0)
        {
            _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
            return false;
        }
        if (x0 >= _width || y0 >= _height)
            return true;

        if (x1 >= _width)
            x1 = (uint16_t)(_width - 1U);
        if (y1 >= _height)
            y1 = (uint16_t)(_height - 1U);

        const int32_t xs32 = x0 + _xStart;
        const int32_t xe32 = x1 + _xStart;
        const int32_t ys32 = y0 + _yStart;
        const int32_t ye32 = y1 + _yStart;
        if (xs32 < 0 || ys32 < 0)
        {
            _lastError = IoError::InvalidConfig;
            return false;
        }

        uint8_t buf[4];
        if (!_transport->acquireBus())
            return failFromTransport(IoError::QueueTransmit);

        if (!sendCommand(CmdCASET))
        {
            _transport->releaseBus();
            return false;
        }
        pack16BE(buf, (uint16_t)xs32, (uint16_t)xe32);
        if (!sendBytes(buf, 4))
        {
            _transport->releaseBus();
            return false;
        }

        if (!sendCommand(CmdRASET))
        {
            _transport->releaseBus();
            return false;
        }
        pack16BE(buf, (uint16_t)ys32, (uint16_t)ye32);
        if (!sendBytes(buf, 4))
        {
            _transport->releaseBus();
            return false;
        }

        const bool ok = sendCommand(CmdRAMWR);
        _transport->releaseBus();
        return ok;
    }

    bool Driver::writePixels565(const uint16_t *pixels, size_t pixelCount, bool swapBytes)
    {
        if (!_transport || !_initialized || !pixels || !pixelCount)
        {
            _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
            return false;
        }

        if (!swapBytes)
            return sendPixels(pixels, pixelCount * sizeof(uint16_t));

        constexpr size_t Chunk = 2048;
        alignas(4) uint16_t tmp[Chunk];
        while (pixelCount)
        {
            const size_t n = std::min(pixelCount, Chunk);
            copySwap565(tmp, pixels, n);
            if (!sendPixels(tmp, n * sizeof(uint16_t)))
                return false;
            pixels += n;
            pixelCount -= n;
        }
        return true;
    }

    bool Driver::fillScreen565(uint16_t color565, bool swapBytes)
    {
        if (!_transport || !_initialized || !_width || !_height)
        {
            _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
            return false;
        }

        if (!setAddrWindow(0, 0, (uint16_t)(_width - 1U), (uint16_t)(_height - 1U)))
            return false;

        constexpr size_t Chunk = 2048;
        uint16_t tmp[Chunk];
        const uint16_t v = swapBytes ? bswap16(color565) : color565;
        for (size_t i = 0; i < Chunk; ++i)
            tmp[i] = v;

        size_t remaining = (size_t)_width * (size_t)_height;
        while (remaining)
        {
            const size_t n = std::min(remaining, Chunk);
            if (!sendPixels(tmp, n * sizeof(uint16_t)))
                return false;
            remaining -= n;
        }

        return flushTransport();
    }
}

#endif
