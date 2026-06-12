#pragma once

#include <PipCore/Config/Features.hpp>

#if PIPCORE_TARGET_DESKTOP

#include <PipCore/Platform.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pipcore::desktop
{
    class WxSimCanvas;
    class WxSimFrame;
    class WxMetricsPanel;

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
        [[nodiscard]] int16_t analogRead(uint8_t pin) const noexcept;

        [[nodiscard]] uint32_t nowMs() noexcept;
        [[nodiscard]] uint64_t nowMicros() noexcept;
        void delayMs(uint32_t ms) noexcept;

        void setBacklightPercent(uint8_t percent) noexcept { _brightness = percent; }
        [[nodiscard]] uint8_t backlightPercent() const noexcept { return _brightness; }

        [[nodiscard]] int serialAvailable() noexcept;
        [[nodiscard]] int serialRead() noexcept;
        [[nodiscard]] size_t serialAvailableForWrite() const noexcept { return 65536U; }
        [[nodiscard]] size_t serialWrite(const uint8_t *data, size_t len) noexcept;
        [[nodiscard]] size_t serialWrite(uint8_t value) noexcept;

        void uiTogglePause() noexcept;
        void uiStepFrame() noexcept;
        void uiCycleTimeScale() noexcept;
        void uiCycleSpiLimit() noexcept;
        void uiToggleRgb565Preview() noexcept;
        void uiToggleRecording() noexcept;
        void uiSaveScreenshot() noexcept;
        void uiRestartProcess() noexcept;
        void uiLogLine(const char *message) noexcept;

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
        [[nodiscard]] bool encodeRecordingFrame(int64_t sampleTime) noexcept;
        void setTransientStatus(const char *message, uint32_t durationMs = 1800) noexcept;
        void refreshWindowTitle() noexcept;
        void serviceSimClock(uint64_t realNowUs) noexcept;
        void markUserExit() noexcept;
        void markRestartRequest() noexcept;
        void advanceFrameStep() noexcept;
        void throttleSpiTransfer(size_t pixelCount) noexcept;
        [[nodiscard]] uint32_t presentColor(uint32_t argb) const noexcept;
        void togglePause() noexcept;
        void stepFrame() noexcept;
        void cycleTimeScale() noexcept;
        void cycleSpiLimit() noexcept;
        void toggleRgb565Preview() noexcept;
        void toggleConsole() noexcept;
        void stepBack() noexcept;
        void handleKey(int key, bool down) noexcept;
        void pushSerialChar(char ch) noexcept;
        [[nodiscard]] bool pinPressed(uint8_t pin) const noexcept;
        [[nodiscard]] static uint32_t color565ToArgb(uint16_t color565) noexcept;

    private:
        friend class WxSimCanvas;
        friend class WxSimFrame;
        friend class WxMetricsPanel;
#if defined(PIPGUI_SIM_USE_WX)
        void *_wxApp = nullptr;
        void *_wxFrame = nullptr;
        void *_wxCanvas = nullptr;
        void *_wxMetrics = nullptr;
        void *_wxLog = nullptr;
        void *_wxEventLoop = nullptr;
        void *_recordPipe = nullptr;
#if defined(_WIN32)
        void *_recordProcess = nullptr;
#endif
#else
#error "Desktop simulator requires PIPGUI_SIM_USE_WX. Use Tools/Simulator to build the wxWidgets simulator."
#endif
        std::string _windowTitle;

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
        uint64_t _lastRealClockUs = 0;
        uint64_t _simClockUs = 0;
        uint64_t _spiReadyUs = 0;
        uint64_t _statusUntilUs = 0;
        uint64_t _lastHistoryCaptureUs = 0;
        std::string _statusText;
        uint32_t _timeScalePercent = 100;
        uint32_t _spiLimitBytesPerSec = 0;
        uint32_t _pendingFrameSteps = 0;
        uint32_t _frameStepCount = 1;
        uint64_t _drawCallCount = 0;
        uint64_t _pixelWriteCount = 0;
        uint64_t _presentFrameCount = 0;
        uint64_t _framePixelWriteCount = 0;
        uint64_t _frameCpuStartUs = 0;
        uint64_t _lastRedrawnPixels = 0;
        uint64_t _lastRenderCpuUs = 0;
        uint64_t _lastEstimatedSpiUs = 0;
        uint32_t _simHeapBytes = 0;
        uint32_t _simHeapPeakBytes = 0;
        bool _spiLimitEnabled = false;
        bool _restartRequested = false;
        bool _paused = false;
        bool _rgb565Preview = false;
        bool _consoleOpen = false;
        bool _historyBrowsing = false;

        struct RecordingState
        {
            void *writer = nullptr;
            uint64_t startedUs = 0;
            uint32_t streamIndex = 0;
            uint64_t nextFrameUs = 0;
            uint64_t frameIndex = 0;
            uint32_t fps = 30;
            int64_t frameDuration = 0;
            bool active = false;
            std::vector<uint32_t> scratch;
        } _recording = {};

        std::vector<uint32_t> _framebuffer;
        std::vector<std::vector<uint32_t>> _frameHistory;
        size_t _frameHistoryCursor = 0;
        std::vector<char> _serialInput;
        size_t _serialReadOffset = 0;

        pipcore::InputMode _pinModes[256] = {};
        bool _prevDown = false;
        bool _nextDown = false;
        bool _selectDown = false;
        bool _upDown = false;
        bool _downDown = false;
    };
}

#endif