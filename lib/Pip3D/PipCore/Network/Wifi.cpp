#include <PipCore/Network/Wifi.hpp>

#if PIPCORE_ENABLE_WIFI

#include <PipCore/Platforms/Select.hpp>

namespace pipcore::net
{
    void wifiConfigure(const WifiConfig &cfg) noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->network())
                backend->configure(cfg);
        }
    }

    void wifiRequest(bool enabled) noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->network())
                backend->request(enabled);
        }
    }

    void wifiService() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->network())
                backend->service();
        }
    }

    WifiState wifiState() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (const Backend *backend = p->network())
                return backend->state();
        }
        return WifiState::Unsupported;
    }

    bool wifiConnected() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (const Backend *backend = p->network())
                return backend->connected();
        }
        return false;
    }

    uint32_t wifiLocalIpV4() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (const Backend *backend = p->network())
                return backend->localIpV4();
        }
        return 0;
    }
}

#endif
