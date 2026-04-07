#include <PipCore/Config/Features.hpp>

#if PIPCORE_ENABLE_OTA

#include <PipCore/Platforms/ESP32/Services/Ota/Internal.hpp>

#include <Arduino.h>
#include <esp_ota_ops.h>

namespace pipcore::esp32::services
{
    using namespace detail;

    void Ota::publish() noexcept
    {
        if (_cb)
            _cb(_st, _cbUser);
    }

    void Ota::setState(pipcore::ota::State s, uint32_t nowMs) noexcept
    {
        if (_st.state == s)
            return;
        _st.state = s;
        _st.lastChangeMs = nowMs;
        publish();
    }

    void Ota::stopHttp() noexcept
    {
        if (_http.active)
        {
            _http.http.end();
            _http.active = false;
        }
        _http.forManifest = false;
        _http.bodyRead = 0;
        _http.bodyTotal = 0;

        if (_http.updateStarted)
        {
            Update.abort();
            _http.updateStarted = false;
        }
        if (_http.shaInit)
        {
            mbedtls_sha256_free(&_http.sha);
            _http.shaInit = false;
        }
    }

    void Ota::setError(pipcore::ota::Error e, uint32_t nowMs, int httpCode, int platformCode) noexcept
    {
        stopHttp();
        wifiRelease();
        _st.error = e;
        _st.httpCode = httpCode;
        _st.platformCode = platformCode;
        setState(pipcore::ota::State::Error, nowMs);
    }

    void Ota::updatePendingVerify(uint32_t nowMs) noexcept
    {
        const esp_partition_t *running = esp_ota_get_running_partition();
        esp_ota_img_states_t state = ESP_OTA_IMG_INVALID;
        const bool pending = running &&
                             esp_ota_get_state_partition(running, &state) == ESP_OK &&
                             state == ESP_OTA_IMG_PENDING_VERIFY;
        if (_st.pendingVerify != pending)
        {
            _st.pendingVerify = pending;
            _st.lastChangeMs = nowMs;
            publish();
        }
    }

    void Ota::resetOperationState() noexcept
    {
        _st.error = pipcore::ota::Error::None;
        _st.httpCode = 0;
        _st.platformCode = 0;
        _st.downloaded = 0;
        _st.total = 0;
        _manifestLen = 0;
        _jsonKind = JsonKind::None;
        _fetchUrl[0] = '\0';
        _autoInstallAfterManifest = false;
    }

    void Ota::failHttpOpen(uint32_t nowMs) noexcept
    {
        if (_st.httpCode <= 0)
        {
            setError(pipcore::ota::Error::HttpBeginFailed, nowMs, _st.httpCode, _st.platformCode);
            return;
        }
        setError(pipcore::ota::Error::HttpStatusNotOk, nowMs, _st.httpCode, _st.platformCode);
    }

    void Ota::wifiAcquire() noexcept
    {
        if (_wifiOwned || !_wifi)
            return;
        if (_wifi->state() != pipcore::net::WifiState::Off)
            return;
        _wifi->request(true);
        _wifiOwned = true;
    }

    void Ota::wifiRelease() noexcept
    {
        if (!_wifiOwned || !_wifi)
            return;
        _wifi->request(false);
        _wifiOwned = false;
    }

    bool Ota::wifiReady() const noexcept
    {
        return _wifi && _wifi->state() == pipcore::net::WifiState::Connected;
    }

    void Ota::markAppValid() noexcept
    {
        esp_ota_mark_app_valid_cancel_rollback();
        updatePendingVerify(static_cast<uint32_t>(millis()));

        const esp_partition_t *running = esp_ota_get_running_partition();
        if (!running)
            return;

        uint32_t good = 0;
        uint32_t prev = 0;
        if (!prefsGetGoodPrev(good, prev))
            return;

        const uint32_t address = running->address;
        if (address == 0 || address == good)
            return;

        prefsPutGoodPrev(address, good);
    }

    void Ota::configure(const pipcore::ota::Options &opt,
                        pipcore::ota::StatusCallback cb,
                        void *user) noexcept
    {
        stopHttp();
        wifiRelease();

        _opt = opt;
        _cb = cb;
        _cbUser = user;
        _st = {};
        _st.channel = pipcore::ota::Channel::Stable;
        _seenBuildStable = 0;
        _seenBuildStableLoaded = false;
        _seenBuildBeta = 0;
        _seenBuildBetaLoaded = false;
        _wantCheck = false;
        _wantInstall = false;
        _wantCancel = false;
        _wifiNextDownload = false;
        _stableListCount = 0;
        _stableListReady = false;
        _wantStableList = false;
        _wantInstallStable = false;
        _installStableVer[0] = '\0';
        resetOperationState();

        if (!copyCString(PIPCORE_OTA_PROJECT_URL, _projectUrl, sizeof(_projectUrl)))
        {
            setError(pipcore::ota::Error::UrlTooLong, static_cast<uint32_t>(millis()));
            return;
        }

        _manifestLimit = kManifestLimitBytes;
        if (_manifestLimit == 0 || _manifestLimit > kMaxManifestBuf)
            _manifestLimit = kMaxManifestBuf;

        _pubkeyOk = false;
        std::memset(_pubkey, 0, sizeof(_pubkey));
        if (_opt.ed25519PubkeyHex && _opt.ed25519PubkeyHex[0])
        {
            const size_t hexLen = std::strlen(_opt.ed25519PubkeyHex);
            _pubkeyOk = parseHexBytes(_opt.ed25519PubkeyHex, hexLen, _pubkey, sizeof(_pubkey));
        }
        publish();
    }

    void Ota::requestCheck(pipcore::ota::CheckMode mode) noexcept
    {
        _wantCheck = true;
        _checkMode = mode;
        _wantInstall = false;
        _wantCancel = false;
        _wantStableList = false;
        _wantInstallStable = false;
        _wifiNextDownload = false;
    }

    void Ota::requestInstall() noexcept
    {
        _wantInstall = true;
        _wantCancel = false;
        _wifiNextDownload = true;
    }

    void Ota::requestStableList() noexcept
    {
        _wantCheck = false;
        _wantStableList = true;
        _wantInstall = false;
        _wantInstallStable = false;
        _wantCancel = false;
        _wifiNextDownload = false;
    }

    void Ota::requestInstallStableVersion(const char *version) noexcept
    {
        if (!version || !version[0])
            return;
        if (!copyCString(version, _installStableVer, sizeof(_installStableVer)))
            return;
        _wantCheck = false;
        _wantStableList = false;
        _wantInstall = false;
        _wantInstallStable = true;
        _wantCancel = false;
        _wifiNextDownload = false;
    }

    void Ota::cancel() noexcept
    {
        _wantCancel = true;
        _wantCheck = false;
        _wantInstall = false;
        _wifiNextDownload = false;
        _wantStableList = false;
        _wantInstallStable = false;
    }

    int Ota::compareVersion(uint16_t aMaj,
                            uint16_t aMin,
                            uint16_t aPat,
                            uint16_t bMaj,
                            uint16_t bMin,
                            uint16_t bPat) noexcept
    {
        if (aMaj != bMaj)
            return (aMaj > bMaj) ? 1 : -1;
        if (aMin != bMin)
            return (aMin > bMin) ? 1 : -1;
        if (aPat != bPat)
            return (aPat > bPat) ? 1 : -1;
        return 0;
    }

    bool Ota::parseVersion(const char *version, uint16_t &maj, uint16_t &min, uint16_t &pat) noexcept
    {
        maj = min = pat = 0;
        if (!version || !version[0])
            return false;

        const char *cursor = version;
        uint32_t parts[3] = {0, 0, 0};
        for (int i = 0; i < 3; ++i)
        {
            if (!*cursor)
                return false;

            uint64_t value = 0;
            bool any = false;
            while (*cursor >= '0' && *cursor <= '9')
            {
                any = true;
                value = (value * 10u) + static_cast<uint64_t>(*cursor - '0');
                if (value > 100000u)
                    return false;
                ++cursor;
            }
            if (!any)
                return false;

            parts[i] = static_cast<uint32_t>(value);
            if (i < 2)
            {
                if (*cursor != '.')
                    return false;
                ++cursor;
            }
        }

        if (*cursor != '\0')
            return false;
        if (parts[0] >= 10000u || parts[1] >= 1000u || parts[2] >= 1000u)
            return false;

        maj = static_cast<uint16_t>(parts[0]);
        min = static_cast<uint16_t>(parts[1]);
        pat = static_cast<uint16_t>(parts[2]);
        return true;
    }
}

#endif
