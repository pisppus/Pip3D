#include <PipCore/Update/Ota.hpp>

#if PIPCORE_ENABLE_OTA

#include <PipCore/Platforms/Select.hpp>

namespace pipcore::ota
{
    void markAppValid() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->update())
                backend->markAppValid();
        }
    }

    void configure(const Options &opt, StatusCallback cb, void *user) noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->update())
                backend->configure(opt, cb, user);
        }
    }

    void requestCheck() noexcept
    {
        requestCheck(CheckMode::NewerOnly);
    }

    void requestCheck(CheckMode mode) noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->update())
                backend->requestCheck(mode);
        }
    }

    void requestInstall() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->update())
                backend->requestInstall();
        }
    }

    void requestStableList() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->update())
                backend->requestStableList();
        }
    }

    bool stableListReady() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (const Backend *backend = p->update())
                return backend->stableListReady();
        }
        return false;
    }

    uint8_t stableListCount() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (const Backend *backend = p->update())
                return backend->stableListCount();
        }
        return 0;
    }

    const char *stableListVersion(uint8_t idx) noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (const Backend *backend = p->update())
                return backend->stableListVersion(idx);
        }
        return "";
    }

    void requestInstallStableVersion(const char *version) noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->update())
                backend->requestInstallStableVersion(version);
        }
    }

    void cancel() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->update())
                backend->cancel();
        }
    }

    void service() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (Backend *backend = p->update())
                backend->service();
        }
    }

    const Status &status() noexcept
    {
        if (pipcore::Platform *p = pipcore::GetPlatform())
        {
            if (const Backend *backend = p->update())
                return backend->status();
        }
        static Status st = {};
        return st;
    }
}

#endif
