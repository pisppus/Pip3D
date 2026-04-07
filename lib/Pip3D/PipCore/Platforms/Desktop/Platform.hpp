#pragma once

#if defined(_WIN32)

#include <PipCore/Displays/Simulator/Display.hpp>
#include <PipCore/Network/Wifi.hpp>
#include <PipCore/Platform.hpp>
#include <PipCore/Update/Ota.hpp>

namespace pipcore::desktop
{
    class Platform final : public pipcore::Platform
    {
    public:
        Platform() = default;

        [[nodiscard]] uint32_t nowMs() noexcept override;

        void pinModeInput(uint8_t pin, InputMode mode) noexcept override;
        [[nodiscard]] bool digitalRead(uint8_t pin) noexcept override;

        void configureBacklightPin(uint8_t, uint8_t = 0, uint32_t = 5000, uint8_t = 12) noexcept override {}
        [[nodiscard]] uint8_t loadMaxBrightnessPercent() noexcept override;
        void storeMaxBrightnessPercent(uint8_t percent) noexcept override;
        void setBacklightPercent(uint8_t percent) noexcept override;

        [[nodiscard]] void *alloc(size_t bytes, AllocCaps caps = AllocCaps::Default) noexcept override;
        void free(void *ptr) noexcept override;

        [[nodiscard]] bool configDisplay(const DisplayConfig &cfg) noexcept override;
        [[nodiscard]] bool beginDisplay(uint8_t rotation) noexcept override;
        [[nodiscard]] bool setDisplayRotation(uint8_t rotation) noexcept override;

        [[nodiscard]] pipcore::Display *display() noexcept override { return &_display; }

        [[nodiscard]] uint32_t freeHeapTotal() noexcept override { return 256U * 1024U * 1024U; }
        [[nodiscard]] uint32_t freeHeapInternal() noexcept override { return 256U * 1024U * 1024U; }
        [[nodiscard]] uint32_t largestFreeBlock() noexcept override { return 64U * 1024U * 1024U; }
        [[nodiscard]] uint32_t minFreeHeap() noexcept override { return 128U * 1024U * 1024U; }

        [[nodiscard]] net::Backend *network() noexcept override { return &_wifi; }
        [[nodiscard]] const net::Backend *network() const noexcept override { return &_wifi; }

        [[nodiscard]] ota::Backend *update() noexcept override { return &_ota; }
        [[nodiscard]] const ota::Backend *update() const noexcept override { return &_ota; }

    private:
        class WifiBackend final : public pipcore::net::Backend
        {
        public:
            void configure(const pipcore::net::WifiConfig &cfg) noexcept override { _cfg = cfg; }
            void request(bool enabled) noexcept override;
            void service() noexcept override {}

            [[nodiscard]] pipcore::net::WifiState state() const noexcept override { return _state; }

        private:
            pipcore::net::WifiConfig _cfg = {};
            pipcore::net::WifiState _state = pipcore::net::WifiState::Off;
        };

        class OtaBackend final : public pipcore::ota::Backend
        {
        public:
            void markAppValid() noexcept override;
            void configure(const pipcore::ota::Options &opt,
                           pipcore::ota::StatusCallback cb,
                           void *user) noexcept override;
            void requestCheck(pipcore::ota::CheckMode mode) noexcept override;
            void requestInstall() noexcept override;
            void requestStableList() noexcept override;
            [[nodiscard]] bool stableListReady() const noexcept override { return false; }
            [[nodiscard]] uint8_t stableListCount() const noexcept override { return 0; }
            [[nodiscard]] const char *stableListVersion(uint8_t idx) const noexcept override;
            void requestInstallStableVersion(const char *version) noexcept override;
            void cancel() noexcept override;
            void service() noexcept override {}
            [[nodiscard]] const pipcore::ota::Status &status() const noexcept override { return _status; }

        private:
            void fail(pipcore::ota::Error error) noexcept;
            void notify() noexcept;

        private:
            pipcore::ota::Options _options = {};
            pipcore::ota::Status _status = {};
            pipcore::ota::StatusCallback _callback = nullptr;
            void *_callbackUser = nullptr;
        };

    private:
        pipcore::simulator::Display _display;
        DisplayConfig _config = {};
        bool _configValid = false;
        uint8_t _brightness = 100;
        WifiBackend _wifi = {};
        OtaBackend _ota = {};
    };
}

#endif
