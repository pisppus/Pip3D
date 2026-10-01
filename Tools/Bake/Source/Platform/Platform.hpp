#pragma once

#include <cstdint>
#include <cstddef>

#include <Display.hpp>

namespace pipcore
{
    namespace net
    {
        class Backend;
        enum class WifiState : uint8_t;
        struct WifiConfig;
    }

    namespace prefs
    {
        class Backend;
    }

    namespace ota
    {
        class Backend;
        enum class Channel : uint8_t;
        enum class CheckMode : uint8_t;
        struct Options;
        struct Status;
    }

    class Touch;
    class Audio;

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
        [[nodiscard]] virtual uint64_t nowUs() noexcept = 0;

        virtual void delayMs(uint32_t) noexcept {}

        [[nodiscard]] virtual bool shouldQuit() const noexcept { return false; }

        virtual void pinModeInput(uint8_t, InputMode) noexcept {}
        [[nodiscard]] virtual bool digitalRead(uint8_t) noexcept { return false; }
        [[nodiscard]] virtual int16_t analogRead(uint8_t pin) noexcept { return 0; }

        virtual void *alloc(size_t bytes, AllocCaps caps = AllocCaps::Default) noexcept = 0;
        virtual void free(void *ptr) noexcept = 0;
        [[nodiscard]] virtual void *allocAligned(size_t bytes, size_t align,
                                                 AllocCaps caps = AllocCaps::Default) noexcept = 0;
        virtual void freeAligned(void *ptr) noexcept = 0;

        [[nodiscard]] virtual bool configDisplay(const DisplayConfig &) noexcept { return false; }

        [[nodiscard]] virtual bool beginDisplay(uint8_t) noexcept { return false; }

        [[nodiscard]] virtual bool setDisplayRotation(uint8_t) noexcept { return false; }

        [[nodiscard]] virtual Display *display() noexcept { return nullptr; }

        [[nodiscard]] virtual uint32_t freeHeapTotal() noexcept { return 0; }
        [[nodiscard]] virtual uint32_t freeHeapInternal() noexcept { return 0; }
        [[nodiscard]] virtual uint32_t largestFreeBlock() noexcept { return 0; }
        [[nodiscard]] virtual uint32_t minFreeHeap() noexcept { return 0; }

        [[nodiscard]] virtual PlatformError lastError() const noexcept { return PlatformError::None; }
        [[nodiscard]] virtual const char *lastErrorText() const noexcept { return platformErrorText(lastError()); }

        [[nodiscard]] virtual net::Backend *network() noexcept { return nullptr; }
        [[nodiscard]] virtual const net::Backend *network() const noexcept { return nullptr; }

        [[nodiscard]] virtual ota::Backend *update() noexcept { return nullptr; }
        [[nodiscard]] virtual const ota::Backend *update() const noexcept { return nullptr; }

        [[nodiscard]] virtual Touch *touch() noexcept { return nullptr; }
        [[nodiscard]] virtual const Touch *touch() const noexcept { return nullptr; }

        [[nodiscard]] virtual Audio *audio() noexcept { return nullptr; }
        [[nodiscard]] virtual const Audio *audio() const noexcept { return nullptr; }

        [[nodiscard]] virtual prefs::Backend *prefs() noexcept { return nullptr; }
        [[nodiscard]] virtual const prefs::Backend *prefs() const noexcept { return nullptr; }
    };

    [[nodiscard]] Platform *GetPlatform() noexcept;
}
