#pragma once

#include <PipCore/Config/Features.hpp>
#include <cstddef>
#include <cstdint>

namespace pipcore::ota
{
    static constexpr size_t kTitleCap = 64;
    static constexpr size_t kVersionCap = 32;
    static constexpr size_t kUrlCap = 512;
    static constexpr size_t kDescCap = 256;

    struct Options
    {
        uint16_t currentVerMajor = 0;
        uint16_t currentVerMinor = 0;
        uint16_t currentVerPatch = 0;
        uint64_t currentBuild = 0;
        const char *ed25519PubkeyHex = nullptr;
    };

    enum class Channel : uint8_t
    {
        Stable = 0,
        Beta = 1,
    };

    enum class CheckMode : uint8_t
    {
        NewerOnly = 0,
        AllowDowngrade = 1,
    };

    enum class State : uint8_t
    {
        Idle = 0,
        WifiStarting = 1,
        FetchingManifest = 2,
        UpdateAvailable = 3,
        Downloading = 4,
        Installing = 5,
        Success = 6,
        Error = 7,
        UpToDate = 8,
    };

    enum class Error : uint8_t
    {
        None = 0,
        WifiNotEnabled = 1,
        WifiNotConnected = 2,
        HttpBeginFailed = 3,
        HttpStatusNotOk = 4,
        ManifestTooLarge = 5,
        ManifestParseFailed = 6,
        ManifestReplay = 7,
        SignatureMissing = 8,
        SignatureInvalid = 9,
        FlashLayoutInvalid = 10,
        RollbackUnavailable = 11,
        UpdateBeginFailed = 12,
        UpdateWriteFailed = 13,
        HashPipelineFailed = 14,
        DownloadTruncated = 15,
        PayloadSizeMismatch = 16,
        HashMismatch = 17,
        UpdateEndFailed = 18,
        UrlTooLong = 19,
    };

    struct Manifest
    {
        char title[kTitleCap] = {};
        uint16_t verMajor = 0;
        uint16_t verMinor = 0;
        uint16_t verPatch = 0;
        char version[kVersionCap] = {};
        uint64_t build = 0;
        uint32_t size = 0;
        char url[kUrlCap] = {};
        char desc[kDescCap] = {};
        uint8_t sha256[32] = {};
        uint8_t sigEd25519[64] = {};
        bool hasSig = false;
    };

    struct Status
    {
        State state = State::Idle;
        Error error = Error::None;
        int httpCode = 0;
        int platformCode = 0;
        uint32_t downloaded = 0;
        uint32_t total = 0;
        uint32_t lastChangeMs = 0;
        int8_t versionCmp = 0;
        int8_t buildCmp = 0;
        bool pendingVerify = false;
        Channel channel = Channel::Stable;
        Manifest manifest = {};
    };

    using StatusCallback = void (*)(const Status &st, void *user);

    class Backend
    {
    public:
        virtual ~Backend() = default;

        virtual void markAppValid() noexcept = 0;
        virtual void configure(const Options &opt,
                               StatusCallback cb,
                               void *user) noexcept = 0;
        virtual void requestCheck(CheckMode mode) noexcept = 0;
        virtual void requestInstall() noexcept = 0;
        virtual void requestStableList() noexcept = 0;
        [[nodiscard]] virtual bool stableListReady() const noexcept = 0;
        [[nodiscard]] virtual uint8_t stableListCount() const noexcept = 0;
        [[nodiscard]] virtual const char *stableListVersion(uint8_t idx) const noexcept = 0;
        virtual void requestInstallStableVersion(const char *version) noexcept = 0;
        virtual void cancel() noexcept = 0;
        virtual void service() noexcept = 0;
        [[nodiscard]] virtual const Status &status() const noexcept = 0;
    };

#if PIPCORE_ENABLE_OTA
    void markAppValid() noexcept;

    void configure(const Options &opt,
                   StatusCallback cb = nullptr,
                   void *user = nullptr) noexcept;

    void requestCheck() noexcept;
    void requestCheck(CheckMode mode) noexcept;

    void requestInstall() noexcept;

    void requestStableList() noexcept;
    [[nodiscard]] bool stableListReady() noexcept;
    [[nodiscard]] uint8_t stableListCount() noexcept;
    [[nodiscard]] const char *stableListVersion(uint8_t idx) noexcept;
    void requestInstallStableVersion(const char *version) noexcept;

    void cancel() noexcept;
    void service() noexcept;

    [[nodiscard]] const Status &status() noexcept;
#else
    void markAppValid() noexcept = delete;

    void configure(const Options &opt,
                   StatusCallback cb = nullptr,
                   void *user = nullptr) noexcept = delete;

    void requestCheck() noexcept = delete;
    void requestCheck(CheckMode mode) noexcept = delete;

    void requestInstall() noexcept = delete;

    void requestStableList() noexcept = delete;
    [[nodiscard]] bool stableListReady() noexcept = delete;
    [[nodiscard]] uint8_t stableListCount() noexcept = delete;
    [[nodiscard]] const char *stableListVersion(uint8_t idx) noexcept = delete;
    void requestInstallStableVersion(const char *version) noexcept = delete;

    void cancel() noexcept = delete;
    void service() noexcept = delete;

    [[nodiscard]] const Status &status() noexcept = delete;
#endif
}
