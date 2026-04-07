#pragma once

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <PipCore/Platform.hpp>
#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include <windows.h>

namespace pipcore::desktop
{
    class Runtime final
    {
    public:
        static Runtime &instance() noexcept;

        [[nodiscard]] bool configureDisplay(uint16_t width, uint16_t height) noexcept;
        [[nodiscard]] bool beginDisplay(uint8_t rotation) noexcept;
        [[nodiscard]] bool setDisplayRotation(uint8_t rotation) noexcept;

        [[nodiscard]] uint16_t width() const noexcept { return _width; }
        [[nodiscard]] uint16_t height() const noexcept { return _height; }

        void fillScreen565(uint16_t color565) noexcept;
        void writeRect565(int16_t x,
                          int16_t y,
                          int16_t w,
                          int16_t h,
                          const uint16_t *pixels,
                          int32_t stridePixels) noexcept;

        void pumpEvents() noexcept;
        [[nodiscard]] bool shouldQuit() const noexcept { return _shouldQuit; }

        void pinModeInput(uint8_t pin, pipcore::InputMode mode) noexcept;
        [[nodiscard]] bool digitalRead(uint8_t pin) const noexcept;

        [[nodiscard]] uint32_t nowMs() noexcept;
        [[nodiscard]] uint64_t nowMicros() noexcept;
        void delayMs(uint32_t ms) noexcept;

        [[nodiscard]] bool keyDown(uint16_t vk) const noexcept;
        [[nodiscard]] bool mouseButtonDown(uint8_t button) const noexcept;
        [[nodiscard]] bool consumeMouseDelta(int32_t &dx, int32_t &dy) noexcept;
        void setMouseCapture(bool enabled) noexcept;
        [[nodiscard]] bool isMouseCaptured() const noexcept { return _mouseCaptured; }

        void setBacklightPercent(uint8_t percent) noexcept { _brightness = percent; }
        [[nodiscard]] uint8_t backlightPercent() const noexcept { return _brightness; }

        [[nodiscard]] int serialAvailable() noexcept;
        [[nodiscard]] int serialRead() noexcept;
        [[nodiscard]] size_t serialAvailableForWrite() const noexcept { return 65536U; }
        [[nodiscard]] size_t serialWrite(const uint8_t *data, size_t len) noexcept;
        [[nodiscard]] size_t serialWrite(uint8_t value) noexcept;

    private:
        Runtime() noexcept;
        Runtime(const Runtime &) = delete;
        Runtime &operator=(const Runtime &) = delete;

        [[nodiscard]] bool ensureWindow() noexcept;
        void resizeWindow() noexcept;
        void updateBitmapInfo() noexcept;
        void requestPresent() noexcept;
        void presentNow() noexcept;
        void serviceRecording(uint64_t nowUs) noexcept;
        [[nodiscard]] bool restartProcess() noexcept;
        [[nodiscard]] bool saveScreenshot() noexcept;
        [[nodiscard]] bool toggleRecording() noexcept;
        [[nodiscard]] bool startRecording() noexcept;
        void stopRecording() noexcept;
        [[nodiscard]] bool encodeRecordingFrame(LONGLONG sampleTimeHns) noexcept;
        void setTransientStatus(const std::wstring &message, uint32_t durationMs = 1800) noexcept;
        void refreshWindowTitle() noexcept;
        void handleKey(UINT vk, bool down) noexcept;
        void handleMouseMove(LPARAM lParam) noexcept;
        void updateMouseCaptureState() noexcept;
        void centerCursorInClient() noexcept;
        void resetInputState() noexcept;
        void pushSerialChar(char ch) noexcept;
        [[nodiscard]] bool pinPressed(uint8_t pin) const noexcept;
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
        [[nodiscard]] static uint32_t color565ToArgb(uint16_t color565) noexcept;

    private:
        HWND _hwnd = nullptr;
        HINSTANCE _instance = nullptr;
        BITMAPINFO _bitmapInfo = {};
        std::wstring _windowTitle;

        uint16_t _baseWidth = 0;
        uint16_t _baseHeight = 0;
        uint16_t _width = 0;
        uint16_t _height = 0;
        uint8_t _rotation = 0;
        uint8_t _scale = 2;
        uint8_t _brightness = 100;

        bool _shouldQuit = false;
        bool _dirty = false;
        uint64_t _lastPresentUs = 0;
        uint64_t _statusUntilUs = 0;
        std::wstring _statusText;

        struct RecordingState
        {
            void *writer = nullptr;
            uint32_t streamIndex = 0;
            uint64_t nextFrameUs = 0;
            uint64_t frameIndex = 0;
            uint32_t fps = 30;
            LONGLONG frameDurationHns = 0;
            bool active = false;
            std::vector<uint32_t> scratch;
        } _recording = {};

        std::vector<uint32_t> _framebuffer;
        std::vector<char> _serialInput;
        size_t _serialReadOffset = 0;

        pipcore::InputMode _pinModes[256] = {};
        bool _prevDown = false;
        bool _nextDown = false;
        bool _selectDown = false;
        std::array<bool, 256> _keys = {};
        std::array<bool, 5> _mouseButtons = {};
        int32_t _mouseDeltaX = 0;
        int32_t _mouseDeltaY = 0;
        bool _mouseCaptured = false;
        bool _cursorHidden = false;
        bool _ignoreNextMouseMove = false;
    };
}

#endif
