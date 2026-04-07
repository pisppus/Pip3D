#pragma once

#include <PipCore/Platform.hpp>

namespace pipcore::esp32::services
{
    class Gpio
    {
    public:
        void pinModeInput(uint8_t pin, InputMode mode) const noexcept;
        [[nodiscard]] bool digitalRead(uint8_t pin) const noexcept;
    };

    class Heap
    {
    public:
        void *alloc(size_t bytes, AllocCaps caps) const noexcept;
        void free(void *ptr) const noexcept;
        [[nodiscard]] uint32_t freeHeapTotal() const noexcept;
        [[nodiscard]] uint32_t freeHeapInternal() const noexcept;
        [[nodiscard]] uint32_t largestFreeBlock() const noexcept;
        [[nodiscard]] uint32_t minFreeHeap() const noexcept;
    };

    class Time
    {
    public:
        [[nodiscard]] uint32_t nowMs() const noexcept;
    };

    class Backlight
    {
    public:
        void configurePin(uint8_t pin, uint8_t channel, uint32_t freqHz, uint8_t resolutionBits) noexcept;
        void setPercent(uint8_t percent) noexcept;

    private:
        bool _configured = false;
        uint8_t _channel = 0;
        uint8_t _resolutionBits = 12;
    };
}
