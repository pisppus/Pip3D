#include <PipCore/Config/Features.hpp>

#if PIPCORE_ENABLE_OTA

#include <PipCore/Platforms/ESP32/Services/Ota/Internal.hpp>

namespace pipcore::esp32::services
{
    using namespace detail;

    bool Ota::beginHttp(const char *url, bool forManifest) noexcept
    {
        stopHttp();
        if (!url || !url[0])
            return false;

        _http.client.setInsecure();
        _http.client.setTimeout(5000);
        _http.http.setReuse(false);
        _http.http.setConnectTimeout(5000);
        _http.http.setTimeout(5000);
        _http.http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        _http.http.setRedirectLimit(4);
        if (!_http.http.begin(_http.client, url))
        {
            char tmp[128];
            const int netErr = _http.client.lastError(tmp, sizeof(tmp));
            _st.platformCode = netErr != 0 ? netErr : 0;
            return false;
        }

        const int code = _http.http.GET();
        _st.httpCode = code;
        if (code != 200)
        {
            _st.platformCode = 0;
            if (code <= 0)
            {
                char tmp[128];
                const int netErr = _http.client.lastError(tmp, sizeof(tmp));
                _st.platformCode = netErr != 0 ? netErr : code;
            }
            _http.http.end();
            return false;
        }

        _http.active = true;
        _http.forManifest = forManifest;
        _st.platformCode = 0;
        const int total = _http.http.getSize();
        _http.bodyTotal = total > 0 ? static_cast<uint32_t>(total) : 0;
        _http.bodyRead = 0;
        if (forManifest)
        {
            _st.downloaded = 0;
            _st.total = 0;
        }
        return true;
    }

    bool Ota::readHttpBody() noexcept
    {
        if (!_http.active || !_http.forManifest)
            return false;

        WiFiClient *stream = _http.http.getStreamPtr();
        if (!stream)
            return false;

        size_t budget = 1024;
        while (budget > 0)
        {
            const int available = stream->available();
            if (available <= 0)
                break;

            const size_t space = _manifestLimit > _manifestLen ? (_manifestLimit - _manifestLen) : 0;
            if (space == 0)
            {
                _manifestLen = _manifestLimit + 1;
                return true;
            }

            size_t chunk = static_cast<size_t>(available);
            if (chunk > 256)
                chunk = 256;
            if (chunk > space)
                chunk = space;
            if (chunk > budget)
                chunk = budget;

            const int read = stream->read(reinterpret_cast<uint8_t *>(_manifestBuf + _manifestLen), chunk);
            if (read <= 0)
                break;
            _manifestLen += static_cast<size_t>(read);
            _http.bodyRead += static_cast<uint32_t>(read);
            budget -= static_cast<size_t>(read);
        }

        if (_http.bodyTotal > 0 && _http.bodyRead >= _http.bodyTotal)
            return true;

        if (_http.bodyTotal == 0 && !_http.http.connected() && stream->available() <= 0)
            return true;

        return false;
    }
}

#endif
