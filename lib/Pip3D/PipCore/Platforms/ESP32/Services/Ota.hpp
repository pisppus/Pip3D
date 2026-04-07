#pragma once

#include <PipCore/Platforms/ESP32/Services/Wifi.hpp>
#include <PipCore/Update/Ota.hpp>

#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>

#include <mbedtls/sha256.h>

namespace pipcore::esp32::services
{
    class Ota : public pipcore::ota::Backend
    {
    public:
        Ota() = default;

        void bindWifi(Wifi *wifi) noexcept { _wifi = wifi; }

        void markAppValid() noexcept override;

        void configure(const pipcore::ota::Options &opt,
                       pipcore::ota::StatusCallback cb,
                       void *user) noexcept override;

        void requestCheck(pipcore::ota::CheckMode mode) noexcept override;
        void requestInstall() noexcept override;

        void requestStableList() noexcept override;
        [[nodiscard]] bool stableListReady() const noexcept override { return _stableListReady; }
        [[nodiscard]] uint8_t stableListCount() const noexcept override { return _stableListCount; }
        [[nodiscard]] const char *stableListVersion(uint8_t idx) const noexcept override
        {
            return (idx < _stableListCount) ? _stableList[idx].version : "";
        }
        void requestInstallStableVersion(const char *version) noexcept override;

        void cancel() noexcept override;

        void service() noexcept override;
        void service(uint32_t nowMs) noexcept;

        [[nodiscard]] const pipcore::ota::Status &status() const noexcept override { return _st; }

    private:
        void publish() noexcept;
        void setState(pipcore::ota::State s, uint32_t nowMs) noexcept;
        void setError(pipcore::ota::Error e, uint32_t nowMs, int httpCode = 0, int platformCode = 0) noexcept;
        void updatePendingVerify(uint32_t nowMs) noexcept;
        void resetOperationState() noexcept;
        void failHttpOpen(uint32_t nowMs) noexcept;

        void wifiAcquire() noexcept;
        void wifiRelease() noexcept;
        [[nodiscard]] bool wifiReady() const noexcept;

        [[nodiscard]] bool beginHttp(const char *url, bool forManifest) noexcept;
        void stopHttp() noexcept;
        [[nodiscard]] bool readHttpBody() noexcept;
        [[nodiscard]] bool parseManifest(uint32_t nowMs) noexcept;
        [[nodiscard]] bool parseProjectIndex(uint32_t nowMs,
                                             pipcore::ota::Channel &outCh,
                                             char *outManifestRel,
                                             size_t outManifestRelCap) noexcept;
        [[nodiscard]] bool parseStableIndex(uint32_t nowMs) noexcept;
        [[nodiscard]] bool verifyManifestSignature(uint32_t nowMs) noexcept;
        [[nodiscard]] bool beginFirmwareDownload(uint32_t nowMs) noexcept;
        [[nodiscard]] bool stepFirmwareDownload(uint32_t nowMs) noexcept;

        [[nodiscard]] bool parseVersion(const char *version, uint16_t &maj, uint16_t &min, uint16_t &pat) noexcept;
        [[nodiscard]] int compareVersion(uint16_t aMaj, uint16_t aMin, uint16_t aPat,
                                         uint16_t bMaj, uint16_t bMin, uint16_t bPat) noexcept;

        Wifi *_wifi = nullptr;

        pipcore::ota::Status _st = {};
        pipcore::ota::Options _opt = {};
        pipcore::ota::StatusCallback _cb = nullptr;
        void *_cbUser = nullptr;

        static constexpr size_t kManifestUrlCap = pipcore::ota::kUrlCap;
        char _projectUrl[kManifestUrlCap] = {};
        char _fetchUrl[kManifestUrlCap] = {};

        enum class JsonKind : uint8_t
        {
            None = 0,
            ProjectIndex = 1,
            ReleaseManifest = 2,
            StableIndex = 3,
        };
        JsonKind _jsonKind = JsonKind::None;

        bool _wantCheck = false;
        pipcore::ota::CheckMode _checkMode = pipcore::ota::CheckMode::NewerOnly;
        bool _wantInstall = false;
        bool _wantCancel = false;
        bool _autoInstallAfterManifest = false;

        bool _wifiOwned = false;
        bool _wifiNextDownload = false;

        uint8_t _pubkey[32] = {};
        bool _pubkeyOk = false;

        static constexpr size_t kMaxManifestBuf = 4096;
        uint8_t _manifestBuf[kMaxManifestBuf + 1] = {};
        size_t _manifestLen = 0;
        size_t _manifestLimit = 0;

        struct HttpState
        {
            HTTPClient http;
            WiFiClientSecure client;
            bool active = false;
            bool forManifest = false;
            uint32_t bodyRead = 0;
            uint32_t bodyTotal = 0;

            mbedtls_sha256_context sha = {};
            bool shaInit = false;
            bool updateStarted = false;
        };

        HttpState _http = {};

        uint64_t _seenBuildStable = 0;
        bool _seenBuildStableLoaded = false;
        uint64_t _seenBuildBeta = 0;
        bool _seenBuildBetaLoaded = false;

        struct StableListEntry
        {
            char version[pipcore::ota::kVersionCap] = {};
            uint64_t build = 0;
        };
        static constexpr uint8_t kStableListCap = 16;
        StableListEntry _stableList[kStableListCap] = {};
        uint8_t _stableListCount = 0;
        bool _stableListReady = false;
        bool _wantStableList = false;
        bool _wantInstallStable = false;
        char _installStableVer[pipcore::ota::kVersionCap] = {};

        bool _bootInit = false;
    };
}
