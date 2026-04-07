#pragma once

#include <PipCore/Platforms/Desktop/Runtime.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

using boolean = bool;
using byte = unsigned char;

#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

#ifndef LOW
#define LOW 0
#endif
#ifndef HIGH
#define HIGH 1
#endif
#ifndef INPUT
#define INPUT 0
#endif
#ifndef OUTPUT
#define OUTPUT 1
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 2
#endif

class String
{
public:
    String() = default;
    String(const char *value) : _value(value ? value : "") {}
    String(const std::string &value) : _value(value) {}
    String(char ch) : _value(1, ch) {}
    String(unsigned char value) : _value(std::to_string(static_cast<unsigned int>(value))) {}
    String(signed char value) : _value(std::to_string(static_cast<int>(value))) {}
    String(short value) : _value(std::to_string(static_cast<int>(value))) {}
    String(unsigned short value) : _value(std::to_string(static_cast<unsigned int>(value))) {}
    String(int value) : _value(std::to_string(value)) {}
    String(unsigned int value) : _value(std::to_string(value)) {}
    String(long value) : _value(std::to_string(value)) {}
    String(unsigned long value) : _value(std::to_string(value)) {}
    String(long long value) : _value(std::to_string(value)) {}
    String(unsigned long long value) : _value(std::to_string(value)) {}
    String(float value) : _value(formatFloat(value, 2)) {}
    String(double value) : _value(formatFloat(value, 2)) {}
    String(float value, unsigned char digits) : _value(formatFloat(value, digits)) {}
    String(double value, unsigned char digits) : _value(formatFloat(value, digits)) {}

    String(const String &) = default;
    String(String &&) noexcept = default;
    String &operator=(const String &) = default;
    String &operator=(String &&) noexcept = default;

    String &operator=(const char *value)
    {
        _value = value ? value : "";
        return *this;
    }

    [[nodiscard]] unsigned int length() const noexcept { return static_cast<unsigned int>(_value.size()); }
    [[nodiscard]] bool reserve(unsigned int size) noexcept
    {
        _value.reserve(size);
        return true;
    }
    [[nodiscard]] const char *c_str() const noexcept { return _value.c_str(); }

    char operator[](unsigned int index) const noexcept
    {
        return (index < _value.size()) ? _value[index] : '\0';
    }

    void remove(unsigned int index)
    {
        remove(index, static_cast<unsigned int>(std::string::npos));
    }

    void remove(unsigned int index, unsigned int count)
    {
        if (index >= _value.size())
            return;
        _value.erase(index, count);
    }

    [[nodiscard]] int indexOf(char ch, unsigned int fromIndex = 0) const noexcept
    {
        if (fromIndex >= _value.size())
            return -1;
        const size_t pos = _value.find(ch, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    [[nodiscard]] String substring(unsigned int start) const
    {
        if (start >= _value.size())
            return String();
        return String(_value.substr(start));
    }

    [[nodiscard]] String substring(unsigned int start, unsigned int end) const
    {
        if (start >= _value.size() || end <= start)
            return String();
        const size_t safeEnd = std::min<size_t>(end, _value.size());
        return String(_value.substr(start, safeEnd - start));
    }

    String &operator+=(const String &rhs)
    {
        _value += rhs._value;
        return *this;
    }

    String &operator+=(const char *rhs)
    {
        _value += rhs ? rhs : "";
        return *this;
    }

    String &operator+=(char ch)
    {
        _value.push_back(ch);
        return *this;
    }

    friend String operator+(const String &lhs, const String &rhs) { return String(lhs._value + rhs._value); }
    friend String operator+(const String &lhs, const char *rhs) { return String(lhs._value + std::string(rhs ? rhs : "")); }
    friend String operator+(const char *lhs, const String &rhs) { return String(std::string(lhs ? lhs : "") + rhs._value); }

private:
    static std::string formatFloat(double value, unsigned char digits)
    {
        std::ostringstream stream;
        stream.setf(std::ios::fixed, std::ios::floatfield);
        stream << std::setprecision(digits) << value;
        return stream.str();
    }

private:
    std::string _value;
};

inline bool operator==(const String &lhs, const String &rhs) noexcept
{
    return std::strcmp(lhs.c_str(), rhs.c_str()) == 0;
}

inline bool operator!=(const String &lhs, const String &rhs) noexcept
{
    return !(lhs == rhs);
}

inline bool operator==(const String &lhs, const char *rhs) noexcept
{
    return std::strcmp(lhs.c_str(), rhs ? rhs : "") == 0;
}

inline bool operator!=(const String &lhs, const char *rhs) noexcept
{
    return !(lhs == rhs);
}

inline bool operator==(const char *lhs, const String &rhs) noexcept
{
    return rhs == lhs;
}

inline bool operator!=(const char *lhs, const String &rhs) noexcept
{
    return !(rhs == lhs);
}

class HardwareSerial
{
public:
    void begin(unsigned long) {}

    [[nodiscard]] int available() { return pipcore::desktop::Runtime::instance().serialAvailable(); }
    [[nodiscard]] int read() { return pipcore::desktop::Runtime::instance().serialRead(); }
    [[nodiscard]] size_t availableForWrite() const { return pipcore::desktop::Runtime::instance().serialAvailableForWrite(); }

    size_t write(uint8_t value) { return pipcore::desktop::Runtime::instance().serialWrite(value); }
    size_t write(const uint8_t *data, size_t len) { return pipcore::desktop::Runtime::instance().serialWrite(data, len); }
    size_t write(const char *data, size_t len) { return write(reinterpret_cast<const uint8_t *>(data), len); }

    size_t print(const String &value) { return write(reinterpret_cast<const uint8_t *>(value.c_str()), value.length()); }
    size_t print(const char *value)
    {
        const char *safe = value ? value : "";
        return write(reinterpret_cast<const uint8_t *>(safe), std::strlen(safe));
    }
    size_t print(char value) { return write(static_cast<uint8_t>(value)); }

    template <typename T>
    size_t print(const T &value)
    {
        std::ostringstream stream;
        stream << value;
        const std::string text = stream.str();
        return write(reinterpret_cast<const uint8_t *>(text.data()), text.size());
    }

    size_t println()
    {
        static const char newline = '\n';
        return write(reinterpret_cast<const uint8_t *>(&newline), 1U);
    }

    template <typename T>
    size_t println(const T &value)
    {
        return print(value) + println();
    }
};

struct EspShim
{
    void restart() const noexcept {}
    [[nodiscard]] uint32_t getFreeHeap() const noexcept { return 256U * 1024U * 1024U; }
    [[nodiscard]] uint32_t getFreePsram() const noexcept { return 0; }
    [[nodiscard]] uint32_t getMaxAllocHeap() const noexcept { return 64U * 1024U * 1024U; }
};

extern HardwareSerial Serial;
extern EspShim ESP;

#if !defined(PIP3D_PC)
inline unsigned long millis()
{
    return pipcore::desktop::Runtime::instance().nowMs();
}

inline unsigned long micros()
{
    return static_cast<unsigned long>(pipcore::desktop::Runtime::instance().nowMicros());
}
#endif

inline void delay(unsigned long ms)
{
    pipcore::desktop::Runtime::instance().delayMs(ms);
}

inline void yield()
{
    pipcore::desktop::Runtime::instance().pumpEvents();
}

inline void pinMode(uint8_t pin, uint8_t mode)
{
    auto inputMode = pipcore::InputMode::Floating;
    if (mode == INPUT_PULLUP)
        inputMode = pipcore::InputMode::Pullup;
    pipcore::desktop::Runtime::instance().pinModeInput(pin, inputMode);
}

inline int digitalRead(uint8_t pin)
{
    return pipcore::desktop::Runtime::instance().digitalRead(pin) ? HIGH : LOW;
}

inline int analogRead(uint8_t)
{
    return 0;
}

template <typename T>
constexpr T min(const T &a, const T &b)
{
    return (b < a) ? b : a;
}

template <typename T>
constexpr T max(const T &a, const T &b)
{
    return (a < b) ? b : a;
}
