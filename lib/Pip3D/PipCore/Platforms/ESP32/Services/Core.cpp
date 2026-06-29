#include <PipCore/Platforms/ESP32/Services/Core.hpp>
#include <PipCore/Debug/MemoryHooks.hpp>

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>

#include <algorithm>

namespace pipcore::esp32::services
{
    void Gpio::pinModeInput(uint8_t pin, InputMode mode) const noexcept
    {
        uint8_t arduinoMode = INPUT;

        if (mode == InputMode::Pullup)
            arduinoMode = INPUT_PULLUP;
#ifdef INPUT_PULLDOWN
        else if (mode == InputMode::Pulldown)
            arduinoMode = INPUT_PULLDOWN;
#endif

        pinMode(pin, arduinoMode);
    }

    bool Gpio::digitalRead(uint8_t pin) const noexcept
    {
        return ::digitalRead(pin) != 0;
    }

    int16_t Gpio::analogRead(uint8_t pin) const noexcept
    {
        return ::analogRead(pin);
    }

    void *Heap::alloc(size_t bytes, AllocCaps caps) const noexcept
    {
        if (bytes == 0)
            return nullptr;

        if (caps == AllocCaps::PreferInternal)
        {
            void *ptr = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (ptr)
            {
                pipcore::debug::memoryEvent(pipcore::debug::MemoryEvent::Alloc, "platform.alloc.internal", ptr, nullptr, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                return ptr;
            }
        }

        void *ptr = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
        pipcore::debug::memoryEvent(ptr ? pipcore::debug::MemoryEvent::Alloc : pipcore::debug::MemoryEvent::AllocFail,
                                    "platform.alloc", ptr, nullptr, bytes, MALLOC_CAP_8BIT);
        return ptr;
    }

    void Heap::free(void *ptr) const noexcept
    {
        heap_caps_free(ptr);
        pipcore::debug::memoryEvent(pipcore::debug::MemoryEvent::Free, "platform.free", ptr, nullptr, 0, 0);
    }

    void *Heap::allocAligned(size_t bytes, size_t align, AllocCaps caps) const noexcept
    {
        if (bytes == 0)
            return nullptr;

        uint32_t caps_val = MALLOC_CAP_8BIT;
        const char *tag = "platform.allocAligned";
        if (caps == AllocCaps::PreferInternal)
        {
            caps_val = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
            tag = "platform.allocAligned.internal";
        }

        void *ptr = heap_caps_aligned_alloc(align, bytes, caps_val);
        pipcore::debug::memoryEvent(ptr ? pipcore::debug::MemoryEvent::Alloc : pipcore::debug::MemoryEvent::AllocFail,
                                    tag, ptr, nullptr, bytes, caps_val);
        return ptr;
    }

    void Heap::freeAligned(void *ptr) const noexcept
    {
        if (!ptr)
            return;
        heap_caps_free(ptr);
        pipcore::debug::memoryEvent(pipcore::debug::MemoryEvent::Free, "platform.freeAligned", ptr, nullptr, 0, 0);
    }

    uint32_t Heap::freeHeapTotal() const noexcept
    {
        return esp_get_free_heap_size();
    }

    uint32_t Heap::freeHeapInternal() const noexcept
    {
        return heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    }

    uint32_t Heap::largestFreeBlock() const noexcept
    {
        return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    }

    uint32_t Heap::minFreeHeap() const noexcept
    {
        return esp_get_minimum_free_heap_size();
    }

    uint32_t Time::nowMs() const noexcept
    {
        return millis();
    }

    uint64_t Time::nowUs() const noexcept
    {
        return esp_timer_get_time();
    }

    void Backlight::configurePin(uint8_t pin, uint8_t channel, uint32_t freqHz, uint8_t resolutionBits, bool activeLow) noexcept
    {
        const uint8_t resolvedBits = (resolutionBits >= 1 && resolutionBits <= 16) ? resolutionBits : 12;
        ledcSetup(channel, freqHz, resolvedBits);
        ledcAttachPin(pin, channel);
        _configured = true;
        _channel = channel;
        _resolutionBits = resolvedBits;
        _activeLow = activeLow;
    }

    void Backlight::setPercent(uint8_t percent) noexcept
    {
        if (!_configured)
            return;

        percent = std::min<uint8_t>(percent, 100);

        const uint32_t dutyMax = (1U << _resolutionBits) - 1U;
        const uint32_t duty = (dutyMax * static_cast<uint32_t>(percent) + 50U) / 100U;

        ledcWrite(_channel, _activeLow ? (dutyMax - duty) : duty);
    }
}