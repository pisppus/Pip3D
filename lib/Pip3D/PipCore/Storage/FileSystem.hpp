#pragma once

#include <cstddef>
#include <cstdint>

#include <PipCore/Config/Features.hpp>

#if PIPCORE_TARGET_DESKTOP

namespace pipcore::storage
{
    enum class OpenMode : uint8_t
    {
        Read,
        Write,
        Append,
    };

    class File
    {
    public:
        File() = default;

        [[nodiscard]] explicit operator bool() const noexcept { return false; }
        [[nodiscard]] bool isDirectory() const noexcept { return false; }
        void close() noexcept {}
        [[nodiscard]] const char *name() const noexcept { return ""; }
        [[nodiscard]] File openNextFile() noexcept { return {}; }
        [[nodiscard]] int read(uint8_t *, size_t) noexcept { return 0; }
        [[nodiscard]] size_t write(const uint8_t *, size_t) noexcept { return 0; }
        [[nodiscard]] size_t write(const char *, size_t) noexcept { return 0; }
        [[nodiscard]] size_t write(const uint8_t *, unsigned) noexcept { return 0; }
        [[nodiscard]] size_t size() const noexcept { return 0; }
        [[nodiscard]] bool seek(size_t) noexcept { return false; }
        void flush() noexcept {}
    };

    [[nodiscard]] inline bool begin(bool = false) noexcept
    {
        return true;
    }

    [[nodiscard]] inline File open(const char *) noexcept
    {
        return {};
    }

    [[nodiscard]] inline File open(const char *, OpenMode) noexcept
    {
        return {};
    }

    [[nodiscard]] inline bool remove(const char *) noexcept
    {
        return false;
    }

    [[nodiscard]] inline bool rename(const char *, const char *) noexcept
    {
        return false;
    }

    [[nodiscard]] inline bool mkdir(const char *) noexcept
    {
        return true;
    }
}

#else

#include <FS.h>
#include <LittleFS.h>

namespace pipcore::storage
{
    enum class OpenMode : uint8_t
    {
        Read,
        Write,
        Append,
    };

    using File = ::fs::File;

    [[nodiscard]] inline const char *nativeMode(OpenMode mode) noexcept
    {
        switch (mode)
        {
        case OpenMode::Write:
            return FILE_WRITE;
        case OpenMode::Append:
            return FILE_APPEND;
        case OpenMode::Read:
        default:
            return FILE_READ;
        }
    }

    [[nodiscard]] inline bool begin(bool formatOnFail = false) noexcept
    {
        return LittleFS.begin(formatOnFail);
    }

    [[nodiscard]] inline File open(const char *path) noexcept
    {
        return LittleFS.open(path);
    }

    [[nodiscard]] inline File open(const char *path, OpenMode mode) noexcept
    {
        return LittleFS.open(path, nativeMode(mode));
    }

    [[nodiscard]] inline bool remove(const char *path) noexcept
    {
        return LittleFS.remove(path);
    }

    [[nodiscard]] inline bool rename(const char *from, const char *to) noexcept
    {
        return LittleFS.rename(from, to);
    }

    [[nodiscard]] inline bool mkdir(const char *path) noexcept
    {
        return LittleFS.mkdir(path);
    }
}

#endif
