#include <PipCore/Config/Features.hpp>

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789

#include <PipCore/Displays/ST7789/Driver.hpp>

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
        constexpr uint8_t CmdMADCTL = 0x36;
        constexpr uint8_t CmdCOLMOD = 0x3A;
        constexpr uint8_t Colmod16bpp = 0x55;
        constexpr uint8_t MadctlMY = 0x80;
        constexpr uint8_t MadctlMX = 0x40;
        constexpr uint8_t MadctlMV = 0x20;
        constexpr uint8_t MadctlBGR = 0x08;
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
        *this = Driver();
    }

    bool Driver::configure(Transport *transport,
                           uint16_t width, uint16_t height, uint8_t order,
                           bool invert, bool swap,
                           int16_t xOffset, int16_t yOffset)
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

    bool Driver::hardReset()
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

    bool Driver::begin(uint8_t rotation)
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

        if (__builtin_expect(!sendCommand(CmdSWRESET), 0))
            return false;
        _transport->delayMs(120);

        if (__builtin_expect(!sendCommand(CmdSLPOUT), 0))
            return false;
        _transport->delayMs(10);

        if (__builtin_expect(!sendCommand(CmdCOLMOD), 0))
            return false;
        {
            uint8_t v = Colmod16bpp;
            if (__builtin_expect(!sendBytes(&v, 1), 0))
                return false;
        }

        if (__builtin_expect(!sendCommand(_invert ? CmdINVON : CmdINVOFF), 0))
            return false;

        if (__builtin_expect(!sendCommand(CmdNORON), 0))
            return false;

        if (__builtin_expect(!setRotationInternal(rotation), 0))
            return false;

        if (__builtin_expect(!sendCommand(CmdDISPON), 0))
            return false;
        _transport->delayMs(20);

        _initialized = true;
        _lastError = IoError::None;
        return true;
    }

    bool Driver::setRotation(uint8_t rotation)
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

    void Driver::setInversion(bool enabled)
    {
        _invert = enabled;
        if (__builtin_expect(!_transport || !_initialized, 0))
            return;
        if (__builtin_expect(!_transport->waitComplete(), 0))
            return;

        (void)sendCommand(_invert ? CmdINVON : CmdINVOFF);
    }

    bool Driver::setRotationInternal(uint8_t rotation)
    {
        if (__builtin_expect(!_transport, 0))
            return failFromTransport(IoError::NotReady);

        _rotation = rotation & 3U;

        const bool isOdd = (_rotation & 1U);

        _width = isOdd ? _physHeight : _physWidth;
        _height = isOdd ? _physWidth : _physHeight;
        _xStart = isOdd ? _yOffsetCfg : _xOffsetCfg;
        _yStart = isOdd ? _xOffsetCfg : _yOffsetCfg;

        const uint8_t bgr = (_order == 1) ? MadctlBGR : 0;
        uint8_t madctl = bgr;

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

        if (__builtin_expect(!sendCommand(CmdMADCTL), 0))
            return false;
        if (__builtin_expect(!sendBytes(&madctl, 1), 0))
            return false;

        return true;
    }

    bool Driver::fillScreen565(uint16_t color565, bool swapBytes)
    {
        if (__builtin_expect(!_transport || !_initialized || !_width || !_height, 0))
        {
            _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
            return false;
        }

        if (__builtin_expect(!setAddrWindow(0, 0, static_cast<uint16_t>(_width - 1U), static_cast<uint16_t>(_height - 1U)), 0))
            return false;

        const uint16_t v = swapBytes ? bswap16(color565) : color565;
        const size_t totalPixels = static_cast<size_t>(_width) * static_cast<size_t>(_height);

        if (__builtin_expect(!_transport->fillPixels(v, totalPixels), 0))
            return failFromTransport(IoError::DataTransmit);

        return true;
    }
}

#endif