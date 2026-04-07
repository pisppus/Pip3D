#pragma once

#include <PipCore/Config/Features.hpp>
#include <cstdint>

namespace pipcore::net
{
    enum class WifiState : uint8_t
    {
        Off = 0,
        Connecting = 1,
        Connected = 2,
        Failed = 3,
        Unsupported = 4,
    };

    struct WifiConfig
    {
        const char *ssid = nullptr;
        const char *password = nullptr;

        bool disableSleep = false;
        bool fullScan = false;
        bool autoReconnect = true;

        uint32_t connectTimeoutMs = 15'000;
        uint32_t retryDelayMs = 2'500;
        uint32_t noSsidGraceMs = 1'000;
    };

    class Backend
    {
    public:
        virtual ~Backend() = default;

        virtual void configure(const WifiConfig &cfg) noexcept = 0;
        virtual void request(bool enabled) noexcept = 0;
        virtual void service() noexcept = 0;

        [[nodiscard]] virtual WifiState state() const noexcept = 0;
        [[nodiscard]] virtual bool connected() const noexcept { return state() == WifiState::Connected; }
        [[nodiscard]] virtual uint32_t localIpV4() const noexcept { return 0; }
    };

#if PIPCORE_ENABLE_WIFI
    void wifiConfigure(const WifiConfig &cfg) noexcept;
    void wifiRequest(bool enabled) noexcept;
    void wifiService() noexcept;

    [[nodiscard]] WifiState wifiState() noexcept;
    [[nodiscard]] bool wifiConnected() noexcept;
    [[nodiscard]] uint32_t wifiLocalIpV4() noexcept;
#else
    void wifiConfigure(const WifiConfig &cfg) noexcept = delete;
    void wifiRequest(bool enabled) noexcept = delete;
    void wifiService() noexcept = delete;

    [[nodiscard]] WifiState wifiState() noexcept = delete;
    [[nodiscard]] bool wifiConnected() noexcept = delete;
    [[nodiscard]] uint32_t wifiLocalIpV4() noexcept = delete;
#endif
}
