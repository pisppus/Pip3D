#include <PipCore/Update/Ota.hpp>

#if PIPCORE_ENABLE_OTA

#include <PipCore/Platforms/Select.hpp>

namespace pipcore::ota
{
    namespace
    {
        [[nodiscard]] inline Backend *getBackend() noexcept
        {
            if (pipcore::Platform *p = pipcore::GetPlatform())
            {
                return p->update();
            }
            return nullptr;
        }
    }

    void markAppValid() noexcept
    {
        if (Backend *b = getBackend())
            b->markAppValid();
    }

    void configure(const Options &opt, StatusCallback cb, void *user) noexcept
    {
        if (Backend *b = getBackend())
            b->configure(opt, cb, user);
    }

    void requestCheck() noexcept
    {
        requestCheck(CheckMode::NewerOnly);
    }

    void requestCheck(CheckMode mode) noexcept
    {
        if (Backend *b = getBackend())
            b->requestCheck(mode);
    }

    void requestInstall() noexcept
    {
        if (Backend *b = getBackend())
            b->requestInstall();
    }

    void requestStableList() noexcept
    {
        if (Backend *b = getBackend())
            b->requestStableList();
    }

    bool stableListReady() noexcept
    {
        if (const Backend *b = getBackend())
            return b->stableListReady();
        return false;
    }

    uint8_t stableListCount() noexcept
    {
        if (const Backend *b = getBackend())
            return b->stableListCount();
        return 0;
    }

    const char *stableListVersion(uint8_t idx) noexcept
    {
        if (const Backend *b = getBackend())
            return b->stableListVersion(idx);
        return "";
    }

    void requestInstallStableVersion(const char *version) noexcept
    {
        if (Backend *b = getBackend())
            b->requestInstallStableVersion(version);
    }

    void cancel() noexcept
    {
        if (Backend *b = getBackend())
            b->cancel();
    }

    void service() noexcept
    {
        if (Backend *b = getBackend())
            b->service();
    }

    const Status &status() noexcept
    {
        if (const Backend *b = getBackend())
            return b->status();

        static Status st = {};
        return st;
    }
}

#endif