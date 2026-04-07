#include <PipCore/Config/Features.hpp>

#if PIPCORE_ENABLE_OTA

#include <PipCore/Platforms/ESP32/Services/Ota/Internal.hpp>

namespace pipcore::esp32::services
{
    using namespace detail;

    bool Ota::parseManifest(uint32_t nowMs) noexcept
    {
        if (_manifestLen == 0)
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }
        if (_manifestLen > _manifestLimit)
        {
            setError(pipcore::ota::Error::ManifestTooLarge, nowMs);
            return false;
        }

        _manifestBuf[_manifestLen] = '\0';
        const char *json = reinterpret_cast<const char *>(_manifestBuf);

        pipcore::ota::Manifest manifest = {};
        const char *after = nullptr;

        if (!jsonFindKey(json, "title", after) ||
            !jsonReadString(after, manifest.title, sizeof(manifest.title), after))
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }

        if (!jsonFindKey(json, "version", after) ||
            !jsonReadString(after, manifest.version, sizeof(manifest.version), after) ||
            !parseVersion(manifest.version, manifest.verMajor, manifest.verMinor, manifest.verPatch))
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }

        if (!jsonFindKey(json, "build", after) ||
            !jsonReadU64(after, manifest.build, after) ||
            manifest.build == 0)
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }

        if (!jsonFindKey(json, "size", after) ||
            !jsonReadU32(after, manifest.size, after) ||
            manifest.size == 0)
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }

        char shaHex[80] = {};
        if (!jsonFindKey(json, "sha256", after) ||
            !jsonReadString(after, shaHex, sizeof(shaHex), after) ||
            !parseHexBytes(shaHex, std::strlen(shaHex), manifest.sha256, sizeof(manifest.sha256)))
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }

        if (!jsonFindKey(json, "url", after) ||
            !jsonReadString(after, manifest.url, sizeof(manifest.url), after))
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }

        if (!jsonFindKey(json, "desc", after) ||
            !jsonReadString(after, manifest.desc, sizeof(manifest.desc), after))
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }

        char sigHex[160] = {};
        if (jsonFindKey(json, "sig_ed25519", after) &&
            jsonReadString(after, sigHex, sizeof(sigHex), after) &&
            parseHexBytes(sigHex, std::strlen(sigHex), manifest.sigEd25519, sizeof(manifest.sigEd25519)))
        {
            manifest.hasSig = true;
        }

        _st.manifest = manifest;

        if (!manifest.hasSig)
        {
            setError(pipcore::ota::Error::SignatureMissing, nowMs);
            return false;
        }
        if (!verifyManifestSignature(nowMs))
            return false;

        const uint64_t derivedBuild =
            (static_cast<uint64_t>(manifest.verMajor) * 1'000'000ull) +
            (static_cast<uint64_t>(manifest.verMinor) * 1'000ull) +
            static_cast<uint64_t>(manifest.verPatch);
        if (manifest.build != derivedBuild)
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }

        uint64_t *seen = _st.channel == pipcore::ota::Channel::Stable ? &_seenBuildStable : &_seenBuildBeta;
        bool *seenLoaded = _st.channel == pipcore::ota::Channel::Stable ? &_seenBuildStableLoaded : &_seenBuildBetaLoaded;
        if (!*seenLoaded)
        {
            uint64_t stored = 0;
            (void)prefsGetU64(seenBuildKey(_st.channel), stored);
            *seen = stored;
            *seenLoaded = true;
        }

        if (_checkMode == pipcore::ota::CheckMode::NewerOnly && *seenLoaded && manifest.build < *seen)
        {
            setError(pipcore::ota::Error::ManifestReplay, nowMs);
            return false;
        }

        const int versionCmp = compareVersion(manifest.verMajor,
                                              manifest.verMinor,
                                              manifest.verPatch,
                                              _opt.currentVerMajor,
                                              _opt.currentVerMinor,
                                              _opt.currentVerPatch);
        _st.versionCmp = static_cast<int8_t>(versionCmp);

        if (_opt.currentBuild != 0)
        {
            if (manifest.build > _opt.currentBuild)
                _st.buildCmp = 1;
            else if (manifest.build < _opt.currentBuild)
                _st.buildCmp = -1;
            else
                _st.buildCmp = 0;
        }
        else
        {
            _st.buildCmp = 0;
        }

        if (!*seenLoaded || manifest.build > *seen)
        {
            *seen = manifest.build;
            *seenLoaded = true;
            prefsPutU64(seenBuildKey(_st.channel), *seen);
        }

        if (versionCmp == 0)
        {
            _st.error = pipcore::ota::Error::None;
            _st.httpCode = 0;
            _st.platformCode = 0;
            setState(pipcore::ota::State::UpToDate, nowMs);
            return true;
        }
        if ((versionCmp < 0 || _st.buildCmp < 0) && _checkMode == pipcore::ota::CheckMode::NewerOnly)
        {
            _st.error = pipcore::ota::Error::None;
            _st.httpCode = 0;
            _st.platformCode = 0;
            setState(pipcore::ota::State::UpToDate, nowMs);
            return true;
        }

        setState(pipcore::ota::State::UpdateAvailable, nowMs);
        return true;
    }

    bool Ota::parseProjectIndex(uint32_t nowMs,
                                pipcore::ota::Channel &outCh,
                                char *outManifestRel,
                                size_t outManifestRelCap) noexcept
    {
        outCh = pipcore::ota::Channel::Stable;
        if (outManifestRel && outManifestRelCap > 0)
            outManifestRel[0] = '\0';

        if (_manifestLen == 0 || !outManifestRel || outManifestRelCap == 0)
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }
        if (_manifestLen > _manifestLimit)
        {
            setError(pipcore::ota::Error::ManifestTooLarge, nowMs);
            return false;
        }

        _manifestBuf[_manifestLen] = '\0';
        const char *json = reinterpret_cast<const char *>(_manifestBuf);

        struct Latest
        {
            bool ok = false;
            char version[pipcore::ota::kVersionCap] = {};
            uint64_t build = 0;
            char manifest[pipcore::ota::kUrlCap] = {};
        };

        auto parseLatest = [&](const char *key, Latest &out) -> void
        {
            out = {};
            const char *after = nullptr;
            if (!jsonFindKey(json, key, after))
                return;

            const char *objectBegin = skipWs(after);
            if (!objectBegin || *objectBegin != '{')
                return;
            const char *objectEnd = findMatchingBrace(objectBegin);
            if (!objectEnd)
                return;

            const char *spanBegin = objectBegin;
            const char *spanEnd = objectEnd + 1;
            const char *cursor = nullptr;

            if (!jsonFindKeySpan(spanBegin, spanEnd, "version", cursor) ||
                !jsonReadString(cursor, out.version, sizeof(out.version), cursor) ||
                !jsonFindKeySpan(spanBegin, spanEnd, "build", cursor) ||
                !jsonReadU64(cursor, out.build, cursor) ||
                out.build == 0 ||
                !jsonFindKeySpan(spanBegin, spanEnd, "manifest", cursor) ||
                !jsonReadString(cursor, out.manifest, sizeof(out.manifest), cursor))
            {
                return;
            }

            out.ok = out.version[0] != '\0' && out.manifest[0] != '\0';
        };

        Latest stable;
        Latest beta;
        parseLatest("stable", stable);
        parseLatest("beta", beta);

        const uint64_t currentBuild = _opt.currentBuild;
        bool haveCandidate = false;
        pipcore::ota::Channel candidateChannel = pipcore::ota::Channel::Stable;
        const char *candidateManifest = nullptr;

        if (stable.ok && currentBuild != 0 && stable.build > currentBuild)
        {
            haveCandidate = true;
            candidateChannel = pipcore::ota::Channel::Stable;
            candidateManifest = stable.manifest;
        }
        else if (beta.ok && currentBuild != 0 && beta.build > currentBuild)
        {
            haveCandidate = true;
            candidateChannel = pipcore::ota::Channel::Beta;
            candidateManifest = beta.manifest;
        }

        if (!haveCandidate)
        {
            _st.versionCmp = 0;
            _st.buildCmp = 0;
            _st.error = pipcore::ota::Error::None;
            _st.httpCode = 0;
            _st.platformCode = 0;
            setState(pipcore::ota::State::UpToDate, nowMs);
            return true;
        }

        outCh = candidateChannel;
        if (!copyCString(candidateManifest, outManifestRel, outManifestRelCap))
        {
            setError(pipcore::ota::Error::UrlTooLong, nowMs);
            return false;
        }
        return true;
    }

    bool Ota::parseStableIndex(uint32_t nowMs) noexcept
    {
        _stableListCount = 0;
        _stableListReady = false;

        if (_manifestLen == 0)
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }
        if (_manifestLen > _manifestLimit)
        {
            setError(pipcore::ota::Error::ManifestTooLarge, nowMs);
            return false;
        }

        _manifestBuf[_manifestLen] = '\0';
        const char *json = reinterpret_cast<const char *>(_manifestBuf);

        const char *after = nullptr;
        if (!jsonFindKey(json, "releases", after))
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }
        const char *cursor = std::strchr(after, '[');
        if (!cursor)
        {
            setError(pipcore::ota::Error::ManifestParseFailed, nowMs);
            return false;
        }
        ++cursor;

        const uint64_t currentBuild = _opt.currentBuild;
        while (*cursor && _stableListCount < kStableListCap)
        {
            const char *versionCursor = nullptr;
            if (!jsonFindKey(cursor, "version", versionCursor))
                break;

            char version[pipcore::ota::kVersionCap] = {};
            if (!jsonReadString(versionCursor, version, sizeof(version), versionCursor))
                break;

            const char *buildCursor = nullptr;
            if (!jsonFindKey(versionCursor, "build", buildCursor))
                break;

            uint64_t build = 0;
            if (!jsonReadU64(buildCursor, build, cursor))
                break;

            if (currentBuild == 0 || build < currentBuild)
            {
                StableListEntry &entry = _stableList[_stableListCount++];
                (void)copyCString(version, entry.version, sizeof(entry.version));
                entry.build = build;
            }
        }

        for (uint8_t i = 0; i < _stableListCount; ++i)
        {
            for (uint8_t j = static_cast<uint8_t>(i + 1); j < _stableListCount; ++j)
            {
                if (_stableList[j].build > _stableList[i].build)
                {
                    StableListEntry tmp = _stableList[i];
                    _stableList[i] = _stableList[j];
                    _stableList[j] = tmp;
                }
            }
        }

        _stableListReady = true;
        return true;
    }

    bool Ota::verifyManifestSignature(uint32_t nowMs) noexcept
    {
        if (!_pubkeyOk || sodium_init() < 0)
        {
            setError(pipcore::ota::Error::SignatureInvalid, nowMs);
            return false;
        }

        const pipcore::ota::Manifest &manifest = _st.manifest;
        uint8_t payload[1024];
        size_t off = 0;

        const size_t prefixLen = std::strlen(kSigPrefix);
        if (off + prefixLen > sizeof(payload))
        {
            setError(pipcore::ota::Error::SignatureInvalid, nowMs);
            return false;
        }
        std::memcpy(payload + off, kSigPrefix, prefixLen);
        off += prefixLen;

        const size_t titleLen = std::strlen(manifest.title);
        if (titleLen == 0 || off + titleLen + 1 > sizeof(payload))
        {
            setError(pipcore::ota::Error::SignatureInvalid, nowMs);
            return false;
        }
        std::memcpy(payload + off, manifest.title, titleLen);
        off += titleLen;
        payload[off++] = 0;

        const size_t versionLen = std::strlen(manifest.version);
        if (versionLen == 0 || off + versionLen + 1 > sizeof(payload))
        {
            setError(pipcore::ota::Error::SignatureInvalid, nowMs);
            return false;
        }
        std::memcpy(payload + off, manifest.version, versionLen);
        off += versionLen;
        payload[off++] = 0;

        if (off + 8 + 4 + sizeof(manifest.sha256) > sizeof(payload))
        {
            setError(pipcore::ota::Error::SignatureInvalid, nowMs);
            return false;
        }

        const uint64_t build = manifest.build;
        payload[off++] = static_cast<uint8_t>((build >> 56) & 0xFF);
        payload[off++] = static_cast<uint8_t>((build >> 48) & 0xFF);
        payload[off++] = static_cast<uint8_t>((build >> 40) & 0xFF);
        payload[off++] = static_cast<uint8_t>((build >> 32) & 0xFF);
        payload[off++] = static_cast<uint8_t>((build >> 24) & 0xFF);
        payload[off++] = static_cast<uint8_t>((build >> 16) & 0xFF);
        payload[off++] = static_cast<uint8_t>((build >> 8) & 0xFF);
        payload[off++] = static_cast<uint8_t>(build & 0xFF);

        payload[off++] = static_cast<uint8_t>((manifest.size >> 24) & 0xFF);
        payload[off++] = static_cast<uint8_t>((manifest.size >> 16) & 0xFF);
        payload[off++] = static_cast<uint8_t>((manifest.size >> 8) & 0xFF);
        payload[off++] = static_cast<uint8_t>(manifest.size & 0xFF);

        std::memcpy(payload + off, manifest.sha256, sizeof(manifest.sha256));
        off += sizeof(manifest.sha256);

        const size_t urlLen = std::strlen(manifest.url);
        if (urlLen == 0 || off + urlLen > sizeof(payload))
        {
            setError(pipcore::ota::Error::SignatureInvalid, nowMs);
            return false;
        }
        std::memcpy(payload + off, manifest.url, urlLen);
        off += urlLen;

        const size_t descLen = std::strlen(manifest.desc);
        if (off + 1 + descLen > sizeof(payload))
        {
            setError(pipcore::ota::Error::SignatureInvalid, nowMs);
            return false;
        }
        payload[off++] = 0;
        if (descLen > 0)
        {
            std::memcpy(payload + off, manifest.desc, descLen);
            off += descLen;
        }

        if (crypto_sign_verify_detached(manifest.sigEd25519, payload, off, _pubkey) != 0)
        {
            setError(pipcore::ota::Error::SignatureInvalid, nowMs);
            return false;
        }
        return true;
    }
}

#endif
