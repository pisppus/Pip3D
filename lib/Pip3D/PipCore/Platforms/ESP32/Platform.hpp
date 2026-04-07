#pragma once

#include <PipCore/Config/Features.hpp>
#include <PipCore/Platform.hpp>

#if !defined(ESP32)
#error "pipcore::esp32::Platform requires ESP32"
#endif

#include <PipCore/Displays/Select.hpp>
#include <PipCore/Platforms/ESP32/Services/Core.hpp>
#if PIPCORE_ENABLE_PREFS
#include <PipCore/Platforms/ESP32/Services/Prefs.hpp>
#endif
#if PIPCORE_ENABLE_WIFI
#include <PipCore/Platforms/ESP32/Services/Wifi.hpp>
#endif
#if PIPCORE_ENABLE_OTA
#include <PipCore/Platforms/ESP32/Services/Ota.hpp>
#endif

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789
#include <PipCore/Platforms/ESP32/Transports/St7789Spi.hpp>
#elif PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ILI9488
#include <PipCore/Platforms/ESP32/Transports/Ili9488Spi.hpp>
#else
#error "Unsupported display transport for selected PIPCORE_DISPLAY"
#endif

namespace pipcore::esp32
{
#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789
    using SelectedDisplayTransport = St7789Spi;
#elif PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ILI9488
    using SelectedDisplayTransport = Ili9488Spi;
#endif

    class Platform final : public pipcore::Platform
    {
    public:
        Platform()
        {
#if PIPCORE_ENABLE_OTA
            _ota.bindWifi(&_wifi);
#endif
        }
        ~Platform() override = default;

        void pinModeInput(uint8_t pin, InputMode mode) noexcept override;
        [[nodiscard]] bool digitalRead(uint8_t pin) noexcept override;

        void configureBacklightPin(uint8_t pin, uint8_t channel = 0, uint32_t freqHz = 5000, uint8_t resolutionBits = 12) noexcept override;

        [[nodiscard]] uint32_t nowMs() noexcept override;
        [[nodiscard]] uint8_t loadMaxBrightnessPercent() noexcept override;
        void storeMaxBrightnessPercent(uint8_t percent) noexcept override;

        void setBacklightPercent(uint8_t percent) noexcept override;

        void *alloc(size_t bytes, AllocCaps caps = AllocCaps::Default) noexcept override;
        void free(void *ptr) noexcept override;

        [[nodiscard]] bool configDisplay(const DisplayConfig &cfg) noexcept override;
        [[nodiscard]] bool beginDisplay(uint8_t rotation) noexcept override;
        [[nodiscard]] bool setDisplayRotation(uint8_t rotation) noexcept override;
        [[nodiscard]] pipcore::Display *display() noexcept override;

        [[nodiscard]] uint32_t freeHeapTotal() noexcept override;
        [[nodiscard]] uint32_t freeHeapInternal() noexcept override;
        [[nodiscard]] uint32_t largestFreeBlock() noexcept override;
        [[nodiscard]] uint32_t minFreeHeap() noexcept override;
        [[nodiscard]] PlatformError lastError() const noexcept override;
        [[nodiscard]] const char *lastErrorText() const noexcept override;

        [[nodiscard]] uint8_t readProgmemByte(const void *addr) noexcept override;

        [[nodiscard]] pipcore::net::Backend *network() noexcept override;
        [[nodiscard]] const pipcore::net::Backend *network() const noexcept override;

        [[nodiscard]] pipcore::ota::Backend *update() noexcept override;
        [[nodiscard]] const pipcore::ota::Backend *update() const noexcept override;

    private:
        services::Time _time;
        services::Gpio _gpio;
        services::Backlight _backlight;
        services::Heap _heap;
#if PIPCORE_ENABLE_PREFS
        services::Prefs _prefs;
#endif
#if PIPCORE_ENABLE_WIFI
        services::Wifi _wifi;
#endif
#if PIPCORE_ENABLE_OTA
        services::Ota _ota;
#endif
        SelectedDisplayTransport _transport;
        SelectedDisplay _display;
        bool _displayConfigured = false;
        bool _displayReady = false;
        PlatformError _lastError = PlatformError::None;
    };
}
