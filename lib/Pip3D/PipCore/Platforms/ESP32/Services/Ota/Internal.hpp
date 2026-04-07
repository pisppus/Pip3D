#pragma once

#include <PipCore/Platforms/ESP32/Services/Ota.hpp>

#include <Preferences.h>
#include <sodium.h>

#include <cstdio>
#include <cstring>
namespace pipcore::esp32::services::detail
{
    inline constexpr const char *kSigPrefix = "pipgui-ota-manifest";
    inline constexpr size_t kManifestLimitBytes = 2048;

    inline constexpr const char *kPrefsNs = "pipgui";
    inline constexpr const char *kPrefsSeenBuildStable = "ota_sbuild";
    inline constexpr const char *kPrefsSeenBuildBeta = "ota_bbuild";
    inline constexpr const char *kPrefsGoodPart = "ota_good";
    inline constexpr const char *kPrefsPrevPart = "ota_prev";

    [[nodiscard]] inline bool isHex(char c) noexcept
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    [[nodiscard]] inline uint8_t hexNibble(char c) noexcept
    {
        if (c >= '0' && c <= '9')
            return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f')
            return static_cast<uint8_t>(10 + (c - 'a'));
        if (c >= 'A' && c <= 'F')
            return static_cast<uint8_t>(10 + (c - 'A'));
        return 0;
    }

    [[nodiscard]] inline bool parseHexBytes(const char *s, size_t hexLen, uint8_t *out, size_t outLen) noexcept
    {
        if (!s || !out || hexLen != outLen * 2u)
            return false;
        for (size_t i = 0; i < hexLen; i += 2)
        {
            const char a = s[i];
            const char b = s[i + 1];
            if (!isHex(a) || !isHex(b))
                return false;
            out[i / 2] = static_cast<uint8_t>((hexNibble(a) << 4) | hexNibble(b));
        }
        return true;
    }

    [[nodiscard]] inline const char *skipWs(const char *p) noexcept
    {
        while (p && *p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
            ++p;
        return p;
    }

    [[nodiscard]] inline bool jsonFindKey(const char *json, const char *key, const char *&outAfterColon) noexcept
    {
        outAfterColon = nullptr;
        if (!json || !key || !key[0])
            return false;

        char needle[64];
        const int n = std::snprintf(needle, sizeof(needle), "\"%s\"", key);
        if (n <= 0 || static_cast<size_t>(n) >= sizeof(needle))
            return false;

        const char *p = std::strstr(json, needle);
        if (!p)
            return false;
        p += n;
        p = skipWs(p);
        if (!p || *p != ':')
            return false;
        ++p;
        outAfterColon = skipWs(p);
        return outAfterColon != nullptr;
    }

    [[nodiscard]] inline bool jsonFindKeySpan(const char *begin,
                                              const char *end,
                                              const char *key,
                                              const char *&outAfterColon) noexcept
    {
        outAfterColon = nullptr;
        if (!begin || !end || begin >= end || !key || !key[0])
            return false;

        char needle[64];
        const int n = std::snprintf(needle, sizeof(needle), "\"%s\"", key);
        if (n <= 0 || static_cast<size_t>(n) >= sizeof(needle))
            return false;

        const char *p = begin;
        while (p && p < end)
        {
            const char *hit = std::strstr(p, needle);
            if (!hit || hit >= end)
                return false;
            const char *q = hit + n;
            q = skipWs(q);
            if (!q || q >= end || *q != ':')
            {
                p = hit + 1;
                continue;
            }
            ++q;
            outAfterColon = skipWs(q);
            return outAfterColon != nullptr && outAfterColon < end;
        }
        return false;
    }

    [[nodiscard]] inline const char *findMatchingBrace(const char *p) noexcept
    {
        if (!p || *p != '{')
            return nullptr;
        int depth = 0;
        bool inStr = false;
        bool esc = false;
        for (; *p; ++p)
        {
            const char c = *p;
            if (inStr)
            {
                if (esc)
                {
                    esc = false;
                    continue;
                }
                if (c == '\\')
                {
                    esc = true;
                    continue;
                }
                if (c == '"')
                    inStr = false;
                continue;
            }
            if (c == '"')
            {
                inStr = true;
                esc = false;
                continue;
            }
            if (c == '{')
            {
                ++depth;
                continue;
            }
            if (c == '}')
            {
                --depth;
                if (depth == 0)
                    return p;
            }
        }
        return nullptr;
    }

    [[nodiscard]] inline bool jsonReadU32(const char *p, uint32_t &out, const char *&outAfter) noexcept
    {
        outAfter = nullptr;
        p = skipWs(p);
        if (!p || !*p)
            return false;

        uint64_t value = 0;
        bool any = false;
        while (*p >= '0' && *p <= '9')
        {
            any = true;
            value = (value * 10u) + static_cast<uint64_t>(*p - '0');
            if (value > 0xFFFFFFFFu)
                return false;
            ++p;
        }
        if (!any)
            return false;
        out = static_cast<uint32_t>(value);
        outAfter = p;
        return true;
    }

    [[nodiscard]] inline bool jsonReadU64(const char *p, uint64_t &out, const char *&outAfter) noexcept
    {
        outAfter = nullptr;
        p = skipWs(p);
        if (!p || !*p)
            return false;

        uint64_t value = 0;
        bool any = false;
        while (*p >= '0' && *p <= '9')
        {
            any = true;
            const uint8_t digit = static_cast<uint8_t>(*p - '0');
            if (value > (UINT64_MAX / 10u))
                return false;
            value = (value * 10u) + static_cast<uint64_t>(digit);
            ++p;
        }
        if (!any)
            return false;
        out = value;
        outAfter = p;
        return true;
    }

    [[nodiscard]] inline bool jsonReadString(const char *p, char *out, size_t outCap, const char *&outAfter) noexcept
    {
        outAfter = nullptr;
        if (!out || outCap == 0)
            return false;
        p = skipWs(p);
        if (!p || *p != '"')
            return false;
        ++p;

        size_t size = 0;
        while (*p)
        {
            const char c = *p++;
            if (c == '"')
            {
                if (size >= outCap)
                    return false;
                out[size] = '\0';
                outAfter = p;
                return true;
            }
            if (c == '\\' && *p)
            {
                const char esc = *p++;
                if (size + 1 >= outCap)
                    return false;
                out[size++] = esc;
                continue;
            }
            if (size + 1 >= outCap)
                return false;
            out[size++] = c;
        }
        return false;
    }

    [[nodiscard]] inline bool appendQueryTs(const char *base, uint32_t ts, char *out, size_t outCap) noexcept
    {
        if (!base || !out || outCap == 0)
            return false;
        const size_t baseLen = std::strlen(base);
        const char separator = std::strchr(base, '?') ? '&' : '?';
        const int n = std::snprintf(out, outCap, "%s%c%s=%lu", base, separator, "ts", static_cast<unsigned long>(ts));
        return n > 0 && static_cast<size_t>(n) < outCap && static_cast<size_t>(n) >= baseLen + 3;
    }

    [[nodiscard]] inline const char *seenBuildKey(pipcore::ota::Channel ch) noexcept
    {
        switch (ch)
        {
        case pipcore::ota::Channel::Stable:
            return kPrefsSeenBuildStable;
        case pipcore::ota::Channel::Beta:
            return kPrefsSeenBuildBeta;
        default:
            return kPrefsSeenBuildStable;
        }
    }

    [[nodiscard]] inline bool prefsGetU64(const char *key, uint64_t &out) noexcept
    {
        out = 0;
        Preferences prefs;
        if (!prefs.begin(kPrefsNs, true))
            return false;
        out = prefs.getULong64(key, 0);
        prefs.end();
        return true;
    }

    inline void prefsPutU64(const char *key, uint64_t value) noexcept
    {
        Preferences prefs;
        if (!prefs.begin(kPrefsNs, false))
            return;
        (void)prefs.putULong64(key, value);
        prefs.end();
    }

    [[nodiscard]] inline bool prefsGetGoodPrev(uint32_t &good, uint32_t &prev) noexcept
    {
        good = 0;
        prev = 0;
        Preferences prefs;
        if (!prefs.begin(kPrefsNs, true))
            return false;
        good = prefs.getUInt(kPrefsGoodPart, 0);
        prev = prefs.getUInt(kPrefsPrevPart, 0);
        prefs.end();
        return true;
    }

    inline void prefsPutGoodPrev(uint32_t good, uint32_t prev) noexcept
    {
        Preferences prefs;
        if (!prefs.begin(kPrefsNs, false))
            return;
        (void)prefs.putUInt(kPrefsGoodPart, good);
        (void)prefs.putUInt(kPrefsPrevPart, prev);
        prefs.end();
    }

    [[nodiscard]] inline bool copyCString(const char *src, char *dst, size_t cap) noexcept
    {
        if (!dst || cap == 0)
            return false;
        if (!src || !src[0])
        {
            dst[0] = '\0';
            return true;
        }
        const size_t len = std::strlen(src);
        if (len >= cap)
            return false;
        std::memcpy(dst, src, len + 1);
        return true;
    }

    [[nodiscard]] inline bool joinUrl(const char *base,
                                      const char *rel,
                                      char *out,
                                      size_t outCap) noexcept
    {
        if (!base || !base[0] || !rel || !rel[0] || !out || outCap == 0)
            return false;
        size_t baseLen = std::strlen(base);
        while (baseLen > 0 && base[baseLen - 1] == '/')
            --baseLen;
        const bool relHasSlash = rel[0] == '/';
        const size_t relLen = std::strlen(rel);
        const size_t need = baseLen + (relHasSlash ? 0 : 1) + relLen + 1;
        if (need > outCap)
            return false;
        std::memcpy(out, base, baseLen);
        size_t off = baseLen;
        if (!relHasSlash)
            out[off++] = '/';
        std::memcpy(out + off, rel, relLen);
        out[off + relLen] = '\0';
        return true;
    }
}
