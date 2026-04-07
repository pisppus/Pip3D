#include <PipCore/Config/Features.hpp>

#if PIPCORE_ENABLE_OTA

#include <PipCore/Platforms/ESP32/Services/Ota/Internal.hpp>

#include <Arduino.h>
#include <esp_ota_ops.h>

namespace pipcore::esp32::services
{
    using namespace detail;

    void Ota::service() noexcept
    {
        const uint32_t nowMs = static_cast<uint32_t>(millis());
        if (_wifi)
            _wifi->service(nowMs);
        service(nowMs);
    }

    void Ota::service(uint32_t nowMs) noexcept
    {
        updatePendingVerify(nowMs);

        if (!_bootInit)
        {
            _bootInit = true;
            const esp_partition_t *running = esp_ota_get_running_partition();
            esp_ota_img_states_t state = ESP_OTA_IMG_INVALID;
            if (running && esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_VALID)
            {
                uint32_t good = 0;
                uint32_t prev = 0;
                if (prefsGetGoodPrev(good, prev))
                {
                    const uint32_t address = running->address;
                    uint32_t newGood = good;
                    uint32_t newPrev = prev;
                    if (good == 0)
                    {
                        newGood = address;
                        newPrev = 0;
                    }
                    else if (address == prev)
                    {
                        newGood = address;
                        newPrev = good;
                    }
                    else if (address != good)
                    {
                        newGood = address;
                        newPrev = 0;
                    }

                    if (newGood != good || newPrev != prev)
                        prefsPutGoodPrev(newGood, newPrev);
                }
            }
        }

        if (_wantCancel)
        {
            _wantCancel = false;
            stopHttp();
            wifiRelease();
            resetOperationState();
            setState(pipcore::ota::State::Idle, nowMs);
            return;
        }

        const bool wantOperation = _wantCheck || _wantStableList || _wantInstallStable;
        if (wantOperation &&
            _st.state != pipcore::ota::State::Idle &&
            _st.state != pipcore::ota::State::Downloading &&
            _st.state != pipcore::ota::State::Installing)
        {
            stopHttp();
            wifiRelease();
            resetOperationState();
            setState(pipcore::ota::State::Idle, nowMs);
        }

        if (_st.state == pipcore::ota::State::Error)
            return;

        if (_st.state == pipcore::ota::State::Idle)
        {
            if (_wantCheck || _wantStableList || _wantInstallStable)
            {
                _wifiNextDownload = false;
                resetOperationState();

                if (_wantCheck)
                {
                    _wantCheck = false;
                    _jsonKind = JsonKind::ProjectIndex;
                    if (!joinUrl(_projectUrl, "index.json", _fetchUrl, sizeof(_fetchUrl)))
                    {
                        setError(pipcore::ota::Error::UrlTooLong, nowMs);
                        return;
                    }
                }
                else if (_wantStableList)
                {
                    _wantStableList = false;
                    _stableListCount = 0;
                    _stableListReady = false;
                    _jsonKind = JsonKind::StableIndex;
                    if (!joinUrl(_projectUrl, "stable/index.json", _fetchUrl, sizeof(_fetchUrl)))
                    {
                        setError(pipcore::ota::Error::UrlTooLong, nowMs);
                        return;
                    }
                }
                else if (_wantInstallStable)
                {
                    _wantInstallStable = false;
                    _checkMode = pipcore::ota::CheckMode::AllowDowngrade;
                    _autoInstallAfterManifest = true;
                    _st.channel = pipcore::ota::Channel::Stable;
                    _jsonKind = JsonKind::ReleaseManifest;

                    char rel[pipcore::ota::kUrlCap] = {};
                    const int n = std::snprintf(rel, sizeof(rel), "stable/%s/manifest.json", _installStableVer);
                    if (n <= 0 ||
                        static_cast<size_t>(n) >= sizeof(rel) ||
                        !joinUrl(_projectUrl, rel, _fetchUrl, sizeof(_fetchUrl)))
                    {
                        setError(pipcore::ota::Error::UrlTooLong, nowMs);
                        return;
                    }
                }

                wifiAcquire();
                setState(pipcore::ota::State::WifiStarting, nowMs);
            }
            return;
        }

        if (_st.state == pipcore::ota::State::WifiStarting)
        {
            wifiAcquire();
            if (!_wifi)
            {
                setError(pipcore::ota::Error::WifiNotEnabled, nowMs);
                return;
            }
            if (_wifi->state() == pipcore::net::WifiState::Failed)
            {
                setError(pipcore::ota::Error::WifiNotConnected, nowMs);
                return;
            }
            if (!wifiReady())
                return;

            if (_wifiNextDownload)
            {
                _wifiNextDownload = false;
                (void)beginFirmwareDownload(nowMs);
                return;
            }

            setState(pipcore::ota::State::FetchingManifest, nowMs);
            _manifestLen = 0;

            char url[pipcore::ota::kUrlCap + 32] = {};
            if (!_fetchUrl[0] || !appendQueryTs(_fetchUrl, nowMs, url, sizeof(url)))
            {
                setError(pipcore::ota::Error::UrlTooLong, nowMs);
                return;
            }
            if (!beginHttp(url, true))
            {
                failHttpOpen(nowMs);
                return;
            }
            return;
        }

        if (_st.state == pipcore::ota::State::FetchingManifest)
        {
            if (!_http.active)
            {
                setError(pipcore::ota::Error::HttpBeginFailed, nowMs);
                return;
            }

            if (!readHttpBody())
                return;

            stopHttp();
            switch (_jsonKind)
            {
            case JsonKind::ProjectIndex:
            {
                pipcore::ota::Channel channel = pipcore::ota::Channel::Stable;
                char rel[pipcore::ota::kUrlCap] = {};
                if (!parseProjectIndex(nowMs, channel, rel, sizeof(rel)))
                    return;
                if (!rel[0])
                {
                    wifiRelease();
                    return;
                }

                _st.channel = channel;
                _jsonKind = JsonKind::ReleaseManifest;
                if (!joinUrl(_projectUrl, rel, _fetchUrl, sizeof(_fetchUrl)))
                {
                    setError(pipcore::ota::Error::UrlTooLong, nowMs);
                    return;
                }

                _manifestLen = 0;
                char url[pipcore::ota::kUrlCap + 32] = {};
                if (!appendQueryTs(_fetchUrl, nowMs, url, sizeof(url)))
                {
                    setError(pipcore::ota::Error::UrlTooLong, nowMs);
                    return;
                }
                if (!beginHttp(url, true))
                {
                    failHttpOpen(nowMs);
                    return;
                }
                return;
            }
            case JsonKind::StableIndex:
                if (!parseStableIndex(nowMs))
                    return;
                _jsonKind = JsonKind::None;
                wifiRelease();
                setState(pipcore::ota::State::Idle, nowMs);
                return;

            case JsonKind::ReleaseManifest:
            {
                if (!parseManifest(nowMs))
                    return;

                const bool autoInstall = _autoInstallAfterManifest;
                _autoInstallAfterManifest = false;
                _jsonKind = JsonKind::None;
                if (autoInstall && _st.state == pipcore::ota::State::UpdateAvailable)
                    _wantInstall = true;

                wifiRelease();
                return;
            }

            default:
                setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
                return;
            }
        }

        if (_st.state == pipcore::ota::State::UpdateAvailable)
        {
            if (_wantInstall)
            {
                _wantInstall = false;
                _wifiNextDownload = true;
                wifiAcquire();
                if (wifiReady())
                {
                    _wifiNextDownload = false;
                    (void)beginFirmwareDownload(nowMs);
                    return;
                }
                setState(pipcore::ota::State::WifiStarting, nowMs);
            }
            return;
        }

        if (_st.state == pipcore::ota::State::Downloading || _st.state == pipcore::ota::State::Installing)
            (void)stepFirmwareDownload(nowMs);
    }
}

#endif
