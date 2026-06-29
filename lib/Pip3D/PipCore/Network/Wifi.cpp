#include <PipCore/Network/Wifi.hpp>

#if PIPCORE_ENABLE_WIFI

#include <PipCore/Platforms/Select.hpp>

namespace pipcore::net
{
    namespace
    {
        [[nodiscard]] inline Backend *getBackend() noexcept
        {
            if (pipcore::Platform *p = pipcore::GetPlatform())
            {
                return p->network();
            }
            return nullptr;
        }
    }

    void wifiConfigure(const WifiConfig &cfg) noexcept
    {
        if (Backend *b = getBackend())
            b->configure(cfg);
    }

    void wifiRequest(bool enabled) noexcept
    {
        if (Backend *b = getBackend())
            b->request(enabled);
    }

    void wifiService() noexcept
    {
        if (Backend *b = getBackend())
            b->service();
    }

    WifiState wifiState() noexcept
    {
        if (const Backend *b = getBackend())
            return b->state();
        return WifiState::Unsupported;
    }

    bool wifiConnected() noexcept
    {
        if (const Backend *b = getBackend())
            return b->connected();
        return false;
    }

    uint32_t wifiLocalIpV4() noexcept
    {
        if (const Backend *b = getBackend())
            return b->localIpV4();
        return 0;
    }
}

#endif