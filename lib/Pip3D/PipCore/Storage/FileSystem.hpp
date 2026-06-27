#pragma once

#include <cstddef>
#include <cstdint>

#include <PipCore/Features.hpp>

#if PIPCORE_TARGET_DESKTOP

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

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
        File(const std::string &path, OpenMode mode, bool isDir = false)
            : _path(path), _isDir(isDir)
        {
            if (_isDir)
                return;

            std::ios_base::openmode iosMode = std::ios::binary;
            if (mode == OpenMode::Read)
            {
                iosMode |= std::ios::in;
                _stream = std::make_unique<std::fstream>(_path, iosMode);
            }
            else if (mode == OpenMode::Write)
            {
                iosMode |= std::ios::out | std::ios::trunc;
                _stream = std::make_unique<std::fstream>(_path, iosMode);
            }
            else if (mode == OpenMode::Append)
            {
                iosMode |= std::ios::out | std::ios::app;
                _stream = std::make_unique<std::fstream>(_path, iosMode);
            }
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            if (_isDir)
                return true;
            return _stream && _stream->is_open();
        }

        [[nodiscard]] bool isDirectory() const noexcept { return _isDir; }

        void close() noexcept
        {
            if (_stream && _stream->is_open())
                _stream->close();
            _stream.reset();
            _isDir = false;
        }

        [[nodiscard]] const char *name() const noexcept
        {
            _fileNameCache = std::filesystem::path(_path).filename().string();
            return _fileNameCache.c_str();
        }

        [[nodiscard]] File openNextFile() noexcept
        {
            if (!_isDir)
                return {};
            if (!_dirIterStarted)
            {
                try
                {
                    _dirIter = std::filesystem::directory_iterator(_path);
                }
                catch (...)
                {
                    return {};
                }
                _dirIterStarted = true;
            }
            if (_dirIter == std::filesystem::directory_iterator{})
            {
                return {};
            }
            auto entry = *_dirIter;
            ++_dirIter;
            return File(entry.path().string(), OpenMode::Read, entry.is_directory());
        }

        [[nodiscard]] int read(uint8_t *buf, size_t size) noexcept
        {
            if (!_stream || !_stream->is_open() || _isDir)
                return -1;
            _stream->read(reinterpret_cast<char *>(buf), size);
            return static_cast<int>(_stream->gcount());
        }

        [[nodiscard]] size_t write(const uint8_t *buf, size_t size) noexcept
        {
            if (!_stream || !_stream->is_open() || _isDir)
                return 0;
            _stream->write(reinterpret_cast<const char *>(buf), size);
            return size;
        }

        [[nodiscard]] size_t write(const char *buf, size_t size) noexcept
        {
            return write(reinterpret_cast<const uint8_t *>(buf), size);
        }

        [[nodiscard]] size_t size() const noexcept
        {
            if (_isDir)
                return 0;
            try
            {
                return std::filesystem::file_size(_path);
            }
            catch (...)
            {
                return 0;
            }
        }

        [[nodiscard]] bool seek(size_t pos) noexcept
        {
            if (!_stream || !_stream->is_open() || _isDir)
                return false;
            _stream->clear();
            _stream->seekg(pos);
            _stream->seekp(pos);
            return true;
        }

        void flush() noexcept
        {
            if (_stream && _stream->is_open())
                _stream->flush();
        }

    private:
        std::string _path;
        bool _isDir = false;
        std::unique_ptr<std::fstream> _stream;
        mutable std::string _fileNameCache;

        bool _dirIterStarted = false;
        std::filesystem::directory_iterator _dirIter;
    };

    [[nodiscard]] inline bool begin(bool = false) noexcept
    {
        try
        {
            std::filesystem::create_directories("sim_fs");
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] inline File open(const char *path) noexcept
    {
        std::string realPath = "sim_fs" + std::string(path);
        if (std::filesystem::is_directory(realPath))
        {
            return File(realPath, OpenMode::Read, true);
        }
        return File(realPath, OpenMode::Read, false);
    }

    [[nodiscard]] inline File open(const char *path, OpenMode mode) noexcept
    {
        std::string realPath = "sim_fs" + std::string(path);
        if (mode == OpenMode::Write || mode == OpenMode::Append)
        {
            try
            {
                std::filesystem::create_directories(std::filesystem::path(realPath).parent_path());
            }
            catch (...)
            {
            }
        }
        return File(realPath, mode, false);
    }

    [[nodiscard]] inline bool remove(const char *path) noexcept
    {
        try
        {
            return std::filesystem::remove("sim_fs" + std::string(path));
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] inline bool rename(const char *from, const char *to) noexcept
    {
        try
        {
            std::filesystem::rename("sim_fs" + std::string(from), "sim_fs" + std::string(to));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] inline bool mkdir(const char *path) noexcept
    {
        try
        {
            return std::filesystem::create_directories("sim_fs" + std::string(path));
        }
        catch (...)
        {
            return false;
        }
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
