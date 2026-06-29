#include <PipCore/Features.hpp>

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

        [[nodiscard]] inline IPAddress u32ToIp(uint32_t addr) noexcept
        { 
            return IPAddress((addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF);
        }
    }

    Wifi::~Wifi()
    {
        if (_eventId != 0)
        {
            WiFi.removeEvent(_eventId);
            _eventId = 0;
        }
    }

    void Wifi::configure(const pipcore::net::WifiConfig &cfg) noexcept
    {
        _cfg = cfg;
        _configured = true;

        if (_eventId == 0)
        {
            _eventId = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
                this->handleWiFiEvent(event, info);
            });
        }
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
        WiFi.disconnect(false); 
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

        if (_cfg.staticIp != 0 && _cfg.subnet != 0 && _cfg.gateway != 0)
        {
            IPAddress ip = u32ToIp(_cfg.staticIp);
            IPAddress gateway = u32ToIp(_cfg.gateway);
            IPAddress subnet = u32ToIp(_cfg.subnet);
            IPAddress dns1 = u32ToIp(_cfg.dns1);
            IPAddress dns2 = u32ToIp(_cfg.dns2);

            WiFi.config(ip, gateway, subnet, dns1, dns2);
        }

        WiFi.begin(_cfg.ssid ? _cfg.ssid : "", _cfg.password ? _cfg.password : "");
        setState(pipcore::net::WifiState::Connecting);
    }

    void Wifi::handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) noexcept
    {
        switch (event)
        {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            _ipV4 = ipToV4(WiFi.localIP());
            setState(pipcore::net::WifiState::Connected);
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            _ipV4 = 0;
            if (_state == pipcore::net::WifiState::Connected || _state == pipcore::net::WifiState::Connecting)
            {
                setState(pipcore::net::WifiState::Failed);
                _nextRetryMs = static_cast<uint32_t>(millis()) + _cfg.retryDelayMs;
            }
            break;

        default:
            break;
        }
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
            const uint32_t el = nowMs - _attemptStartMs;
            if (el >= _cfg.connectTimeoutMs)
            {
                WiFi.disconnect(false);
                setState(pipcore::net::WifiState::Failed);
                _nextRetryMs = nowMs + _cfg.retryDelayMs;
            }
            return;
        }

        if (_state == pipcore::net::WifiState::Connected)
        {
            return;
        }
    }
}

#endif