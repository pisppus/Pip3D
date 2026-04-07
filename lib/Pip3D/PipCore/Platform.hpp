#pragma once

#include <PipCore/Display.hpp>
#include <cstdint>
#include <cstddef>

namespace pipcore
{
    namespace net
    {
        class Backend;
        enum class WifiState : uint8_t;
        struct WifiConfig;
    }

    namespace ota
    {
        class Backend;
        enum class Channel : uint8_t;
        enum class CheckMode : uint8_t;
        struct Options;
        struct Status;
    }

    enum class InputMode : uint8_t
    {
        Floating = 0,
        Pullup = 1,
        Pulldown = 2
    };

    enum class AllocCaps : uint8_t
    {
        Default = 0,
        PreferInternal = 1
    };

    enum class PlatformError : uint8_t
    {
        None = 0,
        PrefsOpenFailed,
        InvalidDisplayConfig,
        DisplayConfigureFailed,
        DisplayBeginFailed,
        DisplayIoFailed
    };

    [[nodiscard]] inline constexpr const char *platformErrorText(PlatformError error) noexcept
    {
        switch (error)
        {
        case PlatformError::None:
            return "ok";
        case PlatformError::PrefsOpenFailed:
            return "preferences open failed";
        case PlatformError::InvalidDisplayConfig:
            return "invalid display config";
        case PlatformError::DisplayConfigureFailed:
            return "display configure failed";
        case PlatformError::DisplayBeginFailed:
            return "display begin failed";
        case PlatformError::DisplayIoFailed:
            return "display io failed";
        default:
            return "unknown platform error";
        }
    }

    struct DisplayConfig
    {
        int8_t mosi = -1;
        int8_t sclk = -1;
        int8_t cs = -1;
        int8_t dc = -1;
        int8_t rst = -1;
        uint16_t width = 0;
        uint16_t height = 0;
        uint32_t hz = 0;
        uint8_t order = 0;
        bool invert = true;
        bool swap = false;
        int16_t xOffset = 0;
        int16_t yOffset = 0;
    };

    class Platform
    {
    public:
        virtual ~Platform() = default;
        [[nodiscard]] virtual uint32_t nowMs() noexcept = 0;

        virtual void pinModeInput(uint8_t, InputMode) noexcept {}
        [[nodiscard]] virtual bool digitalRead(uint8_t) noexcept { return false; }

        virtual void configureBacklightPin(uint8_t, uint8_t = 0, uint32_t = 5000, uint8_t = 12) noexcept {}
        [[nodiscard]] virtual uint8_t loadMaxBrightnessPercent() noexcept { return 100; }
        virtual void storeMaxBrightnessPercent(uint8_t) noexcept {}
        virtual void setBacklightPercent(uint8_t) noexcept {}

        virtual void *alloc(size_t bytes, AllocCaps caps = AllocCaps::Default) noexcept = 0;
        virtual void free(void *ptr) noexcept = 0;

        [[nodiscard]] virtual bool configDisplay(const DisplayConfig &) noexcept
        {
            return false;
        }

        [[nodiscard]] virtual bool beginDisplay(uint8_t) noexcept
        {
            return false;
        }

        [[nodiscard]] virtual bool setDisplayRotation(uint8_t) noexcept
        {
            return false;
        }

        [[nodiscard]] virtual Display *display() noexcept { return nullptr; }

        [[nodiscard]] virtual uint32_t freeHeapTotal() noexcept { return 0; }
        [[nodiscard]] virtual uint32_t freeHeapInternal() noexcept { return 0; }
        [[nodiscard]] virtual uint32_t largestFreeBlock() noexcept { return 0; }
        [[nodiscard]] virtual uint32_t minFreeHeap() noexcept { return 0; }

        [[nodiscard]] virtual PlatformError lastError() const noexcept { return PlatformError::None; }
        [[nodiscard]] virtual const char *lastErrorText() const noexcept { return platformErrorText(lastError()); }

        [[nodiscard]] virtual uint8_t readProgmemByte(const void *addr) noexcept { return *static_cast<const uint8_t *>(addr); }

        [[nodiscard]] virtual net::Backend *network() noexcept { return nullptr; }
        [[nodiscard]] virtual const net::Backend *network() const noexcept { return nullptr; }

        [[nodiscard]] virtual ota::Backend *update() noexcept { return nullptr; }
        [[nodiscard]] virtual const ota::Backend *update() const noexcept { return nullptr; }
    };
}
