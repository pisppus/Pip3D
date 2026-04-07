#pragma once

#include <PipCore/Network/Wifi.hpp>

namespace pipcore::esp32::services
{
    class Wifi : public pipcore::net::Backend
    {
    public:
        Wifi() = default;

        void configure(const pipcore::net::WifiConfig &cfg) noexcept override;
        void request(bool enabled) noexcept override;
        void service() noexcept override;
        void service(uint32_t nowMs) noexcept;

        [[nodiscard]] pipcore::net::WifiState state() const noexcept override { return _state; }
        [[nodiscard]] bool connected() const noexcept override { return _state == pipcore::net::WifiState::Connected; }
        [[nodiscard]] uint32_t localIpV4() const noexcept override { return _ipV4; }

    private:
        void setState(pipcore::net::WifiState st) noexcept;
        void wifiOff() noexcept;
        void startConnect(uint32_t nowMs) noexcept;

        pipcore::net::WifiConfig _cfg = {};
        bool _configured = false;
        bool _hwOffApplied = false;

        pipcore::net::WifiState _state = pipcore::net::WifiState::Off;
        bool _requested = false;
        uint32_t _attemptStartMs = 0;
        uint32_t _nextRetryMs = 0;
        uint32_t _ipV4 = 0;
    };
}
