#include <PipCore/Config/Features.hpp>

#if PIPCORE_ENABLE_WIFI

#include <PipCore/Platforms/ESP32/Services/Wifi.hpp>

#include <Arduino.h>
#include <WiFi.h>

namespace pipcore::esp32::services
{
    namespace
    {
        [[nodiscard]] uint32_t ipToV4(const IPAddress &ip) noexcept
        {
            return (static_cast<uint32_t>(ip[0]) << 24) |
                   (static_cast<uint32_t>(ip[1]) << 16) |
                   (static_cast<uint32_t>(ip[2]) << 8) |
                   static_cast<uint32_t>(ip[3]);
        }
    }

    void Wifi::configure(const pipcore::net::WifiConfig &cfg) noexcept
    {
        _cfg = cfg;
        _configured = true;
    }

    void Wifi::request(bool enabled) noexcept
    {
        _requested = enabled;
        if (!enabled)
            _nextRetryMs = 0;
    }

    void Wifi::service() noexcept
    {
        service(static_cast<uint32_t>(millis()));
    }

    void Wifi::setState(pipcore::net::WifiState st) noexcept
    {
        if (_state == st)
            return;
        _state = st;
    }

    void Wifi::wifiOff() noexcept
    {
        _ipV4 = 0;
        setState(pipcore::net::WifiState::Off);
        _hwOffApplied = true;

        WiFi.persistent(false);
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
    }

    void Wifi::startConnect(uint32_t nowMs) noexcept
    {
        _ipV4 = 0;
        _attemptStartMs = nowMs;
        _hwOffApplied = false;

        WiFi.persistent(false);
        WiFi.setAutoReconnect(_cfg.autoReconnect);
        WiFi.mode(WIFI_STA);

        WiFi.setSleep(!_cfg.disableSleep);

        if (_cfg.fullScan)
        {
            WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
            WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
        }

        WiFi.begin(_cfg.ssid ? _cfg.ssid : "", _cfg.password ? _cfg.password : "");
        setState(pipcore::net::WifiState::Connecting);
    }

    void Wifi::service(uint32_t nowMs) noexcept
    {
        if (!_requested)
        {
            if (_state != pipcore::net::WifiState::Off || !_hwOffApplied)
                wifiOff();
            return;
        }

        if (!_configured || !_cfg.ssid || !_cfg.ssid[0])
        {
            if (_state != pipcore::net::WifiState::Failed)
                setState(pipcore::net::WifiState::Failed);
            return;
        }

        if (_state == pipcore::net::WifiState::Off)
        {
            startConnect(nowMs);
            return;
        }

        if (_state == pipcore::net::WifiState::Failed)
        {
            if (_nextRetryMs == 0)
                _nextRetryMs = nowMs + _cfg.retryDelayMs;

            if ((int32_t)(nowMs - _nextRetryMs) >= 0)
            {
                _nextRetryMs = 0;
                startConnect(nowMs);
            }
            return;
        }

        if (_state == pipcore::net::WifiState::Connecting)
        {
            const wl_status_t st = WiFi.status();

            if (st == WL_CONNECTED)
            {
                _ipV4 = ipToV4(WiFi.localIP());
                setState(pipcore::net::WifiState::Connected);
                return;
            }

            const uint32_t el = nowMs - _attemptStartMs;
            const auto fail = [&]()
            {
                WiFi.disconnect(true, true);
                setState(pipcore::net::WifiState::Failed);
                _nextRetryMs = nowMs + _cfg.retryDelayMs;
            };

            if (st == WL_CONNECT_FAILED)
            {
                fail();
                return;
            }

            if (st == WL_NO_SSID_AVAIL && el >= _cfg.noSsidGraceMs)
            {
                fail();
                return;
            }

            if (el >= _cfg.connectTimeoutMs)
            {
                fail();
            }
            return;
        }

        if (_state == pipcore::net::WifiState::Connected)
        {
            if (WiFi.status() != WL_CONNECTED)
            {
                _ipV4 = 0;
                WiFi.disconnect(true, true);
                setState(pipcore::net::WifiState::Failed);
                _nextRetryMs = nowMs + _cfg.retryDelayMs;
            }
            return;
        }
    }
}

#endif
