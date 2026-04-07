#include <PipCore/Config/Features.hpp>

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ILI9488

#include <PipCore/Displays/ILI9488/Driver.hpp>
#include <algorithm>

namespace pipcore::ili9488
{
    namespace
    {
        constexpr uint8_t CmdSWRESET = 0x01;
        constexpr uint8_t CmdSLPOUT = 0x11;
        constexpr uint8_t CmdINVOFF = 0x20;
        constexpr uint8_t CmdINVON = 0x21;
        constexpr uint8_t CmdDISPON = 0x29;
        constexpr uint8_t CmdCASET = 0x2A;
        constexpr uint8_t CmdPASET = 0x2B;
        constexpr uint8_t CmdRAMWR = 0x2C;
        constexpr uint8_t CmdMADCTL = 0x36;
        constexpr uint8_t CmdCOLMOD = 0x3A;
        constexpr uint8_t Colmod18bpp = 0x66;
        constexpr uint8_t MadctlMY = 0x80;
        constexpr uint8_t MadctlMX = 0x40;
        constexpr uint8_t MadctlMV = 0x20;
        constexpr uint8_t MadctlBGR = 0x08;

        inline void pack16BE(uint8_t *buf, uint16_t a, uint16_t b) noexcept
        {
            buf[0] = static_cast<uint8_t>(a >> 8);
            buf[1] = static_cast<uint8_t>(a & 0xFF);
            buf[2] = static_cast<uint8_t>(b >> 8);
            buf[3] = static_cast<uint8_t>(b & 0xFF);
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
            return "unknown ili9488 io error";
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
        _order = 1;
        _invert = true;
        _swap = false;
        _initialized = false;
        _lastError = IoError::None;
    }

    bool Driver::failFromTransport(IoError fallback)
    {
        _lastError = (_transport && _transport->lastError() != IoError::None) ? _transport->lastError() : fallback;
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

    bool Driver::configure(Transport *transport, uint16_t width, uint16_t height, uint8_t order, bool invert, bool swap, int16_t xOffset, int16_t yOffset)
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
        if (!sendCommand(CmdCOLMOD))
            return false;
        {
            uint8_t v = Colmod18bpp;
            if (!sendBytes(&v, 1))
                return false;
        }
        _transport->delayMs(10);
        if (!sendCommand(CmdSLPOUT))
            return false;
        _transport->delayMs(120);
        if (!sendCommand(_invert ? CmdINVON : CmdINVOFF))
            return false;
        _transport->delayMs(10);
        if (!sendCommand(CmdDISPON))
            return false;
        _transport->delayMs(25);
        if (!setRotationInternal(rotation))
            return false;

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

    bool Driver::setRotationInternal(uint8_t rotation)
    {
        if (!_transport)
            return failFromTransport(IoError::NotReady);

        _rotation = rotation & 3U;
        const uint8_t order = (_order == 1U) ? MadctlBGR : 0U;

        uint8_t madctl = 0;
        switch (_rotation)
        {
        case 0:
            madctl = order;
            _width = _physWidth;
            _height = _physHeight;
            _xStart = _xOffsetCfg;
            _yStart = _yOffsetCfg;
            break;
        case 1:
            madctl = MadctlMX | MadctlMV | order;
            _width = _physHeight;
            _height = _physWidth;
            _xStart = _yOffsetCfg;
            _yStart = _xOffsetCfg;
            break;
        case 2:
            madctl = MadctlMX | MadctlMY | order;
            _width = _physWidth;
            _height = _physHeight;
            _xStart = _xOffsetCfg;
            _yStart = _yOffsetCfg;
            break;
        case 3:
            madctl = MadctlMX | MadctlMY | MadctlMV | order;
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
            x1 = static_cast<uint16_t>(_width - 1U);
        if (y1 >= _height)
            y1 = static_cast<uint16_t>(_height - 1U);

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
        const bool ownBus = !_transport->isBusAcquired();
        if (ownBus && !_transport->acquireBus())
            return failFromTransport(IoError::QueueTransmit);
        if (!sendCommand(CmdCASET))
        {
            if (ownBus)
                _transport->releaseBus();
            return false;
        }
        pack16BE(buf, static_cast<uint16_t>(xs32), static_cast<uint16_t>(xe32));
        if (!sendBytes(buf, 4))
        {
            if (ownBus)
                _transport->releaseBus();
            return false;
        }
        if (!sendCommand(CmdPASET))
        {
            if (ownBus)
                _transport->releaseBus();
            return false;
        }
        pack16BE(buf, static_cast<uint16_t>(ys32), static_cast<uint16_t>(ye32));
        if (!sendBytes(buf, 4))
        {
            if (ownBus)
                _transport->releaseBus();
            return false;
        }

        const bool ok = sendCommand(CmdRAMWR);
        if (ownBus)
            _transport->releaseBus();
        return ok;
    }

    bool Driver::beginWriteWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
    {
        if (!_transport)
            return failFromTransport(IoError::NotReady);
        const bool ownBus = !_transport->isBusAcquired();
        if (ownBus && !_transport->acquireBus())
            return failFromTransport(IoError::QueueTransmit);
        if (setAddrWindow(x0, y0, x1, y1))
            return true;
        if (ownBus)
            _transport->releaseBus();
        return false;
    }

    void Driver::endWrite() noexcept
    {
        if (!_transport)
            return;
        (void)flushTransport();
        _transport->releaseBus();
    }

    bool Driver::writePixels666(const uint8_t *bytes, size_t len)
    {
        if (!_transport || !_initialized || !bytes || !len)
        {
            _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
            return false;
        }
        return sendPixels(bytes, len);
    }

    bool Driver::fillScreen565(uint16_t color565)
    {
        if (!_transport || !_initialized || !_width || !_height)
        {
            _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
            return false;
        }
        if (!beginWriteWindow(0, 0, static_cast<uint16_t>(_width - 1U), static_cast<uint16_t>(_height - 1U)))
            return false;

        constexpr size_t ChunkPixels = 128;
        uint8_t tmp[ChunkPixels * 3U];
        const uint8_t r = static_cast<uint8_t>((color565 >> 8) & 0xF8);
        const uint8_t g = static_cast<uint8_t>((color565 >> 3) & 0xFC);
        const uint8_t b = static_cast<uint8_t>((color565 << 3) & 0xF8);
        for (size_t i = 0; i < ChunkPixels; ++i)
        {
            const size_t base = i * 3U;
            tmp[base] = r;
            tmp[base + 1U] = g;
            tmp[base + 2U] = b;
        }

        size_t remaining = static_cast<size_t>(_width) * static_cast<size_t>(_height);
        while (remaining)
        {
            const size_t pixels = std::min(remaining, ChunkPixels);
            if (!sendPixels(tmp, pixels * 3U))
            {
                endWrite();
                return false;
            }
            remaining -= pixels;
        }

        endWrite();
        return true;
    }
}

#endif
