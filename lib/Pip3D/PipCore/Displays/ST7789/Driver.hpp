#pragma once

#include <cstdint>
#include <cstddef>

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
        [[nodiscard]] virtual bool acquireBus() = 0;
        virtual void releaseBus() = 0;
        [[nodiscard]] virtual bool flush() = 0;
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

        [[nodiscard]] bool writePixels565(const uint16_t *pixels, size_t pixelCount, bool swapBytes = false);

        [[nodiscard]] bool fillScreen565(uint16_t color565, bool swapBytes = false);

        void setInversion(bool enabled);

    private:
        [[nodiscard]] bool hardReset();
        [[nodiscard]] bool setRotationInternal(uint8_t rotation);
        [[nodiscard]] bool sendCommand(uint8_t cmd);
        [[nodiscard]] bool sendBytes(const void *data, size_t len);
        [[nodiscard]] bool sendPixels(const void *data, size_t len);
        [[nodiscard]] bool flushTransport();
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
