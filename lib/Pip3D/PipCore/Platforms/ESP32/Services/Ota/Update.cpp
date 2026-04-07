#include <PipCore/Config/Features.hpp>

#if PIPCORE_ENABLE_OTA

#include <PipCore/Platforms/ESP32/Services/Ota/Internal.hpp>

#include <Arduino.h>
#include <esp_ota_ops.h>

namespace pipcore::esp32::services
{
    bool Ota::beginFirmwareDownload(uint32_t nowMs) noexcept
    {
        const pipcore::ota::Manifest &manifest = _st.manifest;

        const esp_partition_t *running = esp_ota_get_running_partition();
        const esp_partition_t *part = esp_ota_get_next_update_partition(running);
        if (!part)
        {
            setError(pipcore::ota::Error::FlashLayoutInvalid, nowMs);
            return false;
        }

        const uint32_t flashSize = ESP.getFlashChipSize();
        const uint64_t partEnd = static_cast<uint64_t>(part->address) + static_cast<uint64_t>(part->size);
        if (partEnd > flashSize || part->size < manifest.size)
        {
            setError(pipcore::ota::Error::FlashLayoutInvalid, nowMs);
            return false;
        }

        if (!beginHttp(manifest.url, false))
        {
            failHttpOpen(nowMs);
            return false;
        }

        _st.downloaded = 0;
        _st.total = manifest.size;

        if (!_http.shaInit)
        {
            mbedtls_sha256_init(&_http.sha);
            if (mbedtls_sha256_starts_ret(&_http.sha, 0) != 0)
            {
                setError(pipcore::ota::Error::HashPipelineFailed, nowMs);
                return false;
            }
            _http.shaInit = true;
        }

        if (!Update.begin(manifest.size))
        {
            setError(pipcore::ota::Error::UpdateBeginFailed, nowMs);
            return false;
        }

        _http.updateStarted = true;
        setState(pipcore::ota::State::Downloading, nowMs);
        return true;
    }

    bool Ota::stepFirmwareDownload(uint32_t nowMs) noexcept
    {
        if (!_http.active || _http.forManifest)
        {
            setError(pipcore::ota::Error::UpdateWriteFailed, nowMs);
            return false;
        }

        WiFiClient *stream = _http.http.getStreamPtr();
        if (!stream)
        {
            setError(pipcore::ota::Error::UpdateWriteFailed, nowMs);
            return false;
        }

        uint8_t buffer[1024];
        size_t budget = 8 * 1024;
        while (budget > 0)
        {
            const int available = stream->available();
            if (available <= 0)
                break;

            size_t chunk = static_cast<size_t>(available);
            if (chunk > sizeof(buffer))
                chunk = sizeof(buffer);

            const uint32_t remaining = _st.total > _st.downloaded ? (_st.total - _st.downloaded) : 0;
            if (remaining == 0)
                break;
            if (chunk > remaining)
                chunk = remaining;
            if (chunk > budget)
                chunk = budget;

            const int read = stream->read(buffer, chunk);
            if (read <= 0)
                break;

            if (mbedtls_sha256_update_ret(&_http.sha, buffer, static_cast<size_t>(read)) != 0)
            {
                setError(pipcore::ota::Error::HashPipelineFailed, nowMs);
                return false;
            }

            const size_t written = Update.write(buffer, static_cast<size_t>(read));
            if (written != static_cast<size_t>(read))
            {
                setError(pipcore::ota::Error::UpdateWriteFailed, nowMs);
                return false;
            }

            _st.downloaded += static_cast<uint32_t>(read);
            budget -= static_cast<size_t>(read);
            if (_st.downloaded > _st.total)
            {
                setError(pipcore::ota::Error::PayloadSizeMismatch, nowMs);
                return false;
            }
        }

        if (_st.downloaded < _st.total)
        {
            if (!_http.http.connected() || !stream->connected())
            {
                setError(pipcore::ota::Error::DownloadTruncated, nowMs);
                return false;
            }
            return true;
        }

        uint8_t hash[32] = {};
        if (mbedtls_sha256_finish_ret(&_http.sha, hash) != 0)
        {
            setError(pipcore::ota::Error::HashPipelineFailed, nowMs);
            return false;
        }
        mbedtls_sha256_free(&_http.sha);
        _http.shaInit = false;

        if (std::memcmp(hash, _st.manifest.sha256, sizeof(hash)) != 0)
        {
            setError(pipcore::ota::Error::HashMismatch, nowMs);
            return false;
        }

        setState(pipcore::ota::State::Installing, nowMs);
        if (!Update.end(false))
        {
            const int updateError = static_cast<int>(Update.getError());
            if (updateError == UPDATE_ERROR_ACTIVATE)
            {
                const esp_partition_t *running = esp_ota_get_running_partition();
                const esp_partition_t *part = esp_ota_get_next_update_partition(running);
                const esp_err_t err = part ? esp_ota_set_boot_partition(part) : ESP_ERR_INVALID_STATE;
                if (err == ESP_OK)
                {
                    _http.updateStarted = false;
                    stopHttp();
                    wifiRelease();
                    setState(pipcore::ota::State::Success, nowMs);
                    return true;
                }
                setError(pipcore::ota::Error::UpdateEndFailed, nowMs, 0, static_cast<int>(err));
                return false;
            }

            setError(pipcore::ota::Error::UpdateEndFailed, nowMs, 0, updateError);
            return false;
        }

        _http.updateStarted = false;
        stopHttp();
        wifiRelease();
        setState(pipcore::ota::State::Success, nowMs);
        return true;
    }
}

#endif
