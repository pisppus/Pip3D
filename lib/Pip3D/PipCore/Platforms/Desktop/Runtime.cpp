#if defined(_WIN32)

#include <PipCore/Platforms/Desktop/Runtime.hpp>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <objbase.h>
#include <wincodec.h>
#include <iostream>
#include <thread>
#include <windowsx.h>

namespace pipcore::desktop
{
    namespace
    {
        constexpr wchar_t kWindowClassName[] = L"PipGuiDesktopSimulatorWindow";
        constexpr uint8_t kPrevPin =
#ifdef PIPGUI_SIM_BTN_PREV_PIN
            static_cast<uint8_t>(PIPGUI_SIM_BTN_PREV_PIN);
#else
            4U;
#endif
        constexpr uint8_t kNextPin =
#ifdef PIPGUI_SIM_BTN_NEXT_PIN
            static_cast<uint8_t>(PIPGUI_SIM_BTN_NEXT_PIN);
#else
            20U;
#endif
        constexpr uint8_t kSelectPin =
#ifdef PIPGUI_SIM_BTN_SELECT_PIN
            static_cast<uint8_t>(PIPGUI_SIM_BTN_SELECT_PIN);
#else
            21U;
#endif

        [[nodiscard]] uint64_t monotonicMicros() noexcept
        {
            using clock = std::chrono::steady_clock;
            static const clock::time_point start = clock::now();
            const auto delta = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start);
            return static_cast<uint64_t>(delta.count());
        }

        template <typename T>
        void safeRelease(T *&ptr) noexcept
        {
            if (ptr)
            {
                ptr->Release();
                ptr = nullptr;
            }
        }

        void enableDpiAwareness() noexcept
        {
            static bool applied = false;
            if (applied)
                return;

            applied = true;

            HMODULE user32 = GetModuleHandleW(L"user32.dll");
            if (!user32)
                return;

            using SetThreadDpiAwarenessContextFn = DPI_AWARENESS_CONTEXT(WINAPI *)(DPI_AWARENESS_CONTEXT);
            using SetProcessDpiAwarenessContextFn = BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT);
            using SetProcessDPIAwareFn = BOOL(WINAPI *)(void);

            if (auto setThreadDpiAwarenessContext =
                    reinterpret_cast<SetThreadDpiAwarenessContextFn>(GetProcAddress(user32, "SetThreadDpiAwarenessContext")))
            {
                setThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
                return;
            }

            if (auto setProcessDpiAwarenessContext =
                    reinterpret_cast<SetProcessDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")))
            {
                setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
                return;
            }

            if (auto setProcessDPIAware =
                    reinterpret_cast<SetProcessDPIAwareFn>(GetProcAddress(user32, "SetProcessDPIAware")))
            {
                setProcessDPIAware();
            }
        }

        [[nodiscard]] std::wstring makeBaseWindowTitle() noexcept
        {
            namespace fs = std::filesystem;
            try
            {
                fs::path cwd = fs::current_path();
                fs::path projectDir = (cwd.filename() == ".sim") ? cwd.parent_path() : cwd;
                const std::wstring name = projectDir.filename().wstring();
                if (!name.empty())
                    return name + L" Simulator";
            }
            catch (...)
            {
            }

            return L"Simulator";
        }
    }

    Runtime &Runtime::instance() noexcept
    {
        static Runtime runtime;
        return runtime;
    }

    Runtime::Runtime() noexcept
        : _instance(GetModuleHandleW(nullptr)),
          _windowTitle(makeBaseWindowTitle())
    {
        _scale =
#ifdef PIPGUI_SIM_SCALE
            static_cast<uint8_t>(PIPGUI_SIM_SCALE);
#else
            2U;
#endif
        std::memset(&_bitmapInfo, 0, sizeof(_bitmapInfo));
    }

    bool Runtime::configureDisplay(uint16_t width, uint16_t height) noexcept
    {
        if (width == 0 || height == 0)
            return false;

        _baseWidth = width;
        _baseHeight = height;
        return setDisplayRotation(_rotation);
    }

    bool Runtime::beginDisplay(uint8_t rotation) noexcept
    {
        return setDisplayRotation(rotation & 3U) && ensureWindow();
    }

    bool Runtime::setDisplayRotation(uint8_t rotation) noexcept
    {
        if (_baseWidth == 0 || _baseHeight == 0)
            return false;

        _rotation = rotation & 3U;
        const bool quarterTurn = ((_rotation & 1U) != 0U);
        _width = quarterTurn ? _baseHeight : _baseWidth;
        _height = quarterTurn ? _baseWidth : _baseHeight;

        _framebuffer.assign(static_cast<size_t>(_width) * static_cast<size_t>(_height), 0xFF000000u);
        updateBitmapInfo();
        resizeWindow();
        requestPresent();
        return true;
    }

    void Runtime::fillScreen565(uint16_t color565) noexcept
    {
        if (_framebuffer.empty())
            return;

        std::fill(_framebuffer.begin(), _framebuffer.end(), color565ToArgb(color565));
        requestPresent();
    }

    void Runtime::writeRect565(int16_t x,
                               int16_t y,
                               int16_t w,
                               int16_t h,
                               const uint16_t *pixels,
                               int32_t stridePixels) noexcept
    {
        if (!pixels || w <= 0 || h <= 0 || stridePixels <= 0 || _framebuffer.empty())
            return;

        const int32_t dstX1 = std::max<int32_t>(0, x);
        const int32_t dstY1 = std::max<int32_t>(0, y);
        const int32_t dstX2 = std::min<int32_t>(_width, static_cast<int32_t>(x) + w);
        const int32_t dstY2 = std::min<int32_t>(_height, static_cast<int32_t>(y) + h);
        if (dstX1 >= dstX2 || dstY1 >= dstY2)
            return;

        const int32_t srcOffsetX = dstX1 - x;
        const int32_t srcOffsetY = dstY1 - y;

        for (int32_t row = dstY1; row < dstY2; ++row)
        {
            uint32_t *dst = _framebuffer.data() + static_cast<size_t>(row) * _width + dstX1;
            const uint16_t *src = pixels + static_cast<size_t>(srcOffsetY + (row - dstY1)) * static_cast<size_t>(stridePixels) + srcOffsetX;
            for (int32_t col = dstX1; col < dstX2; ++col)
            {
                *dst++ = color565ToArgb(__builtin_bswap16(*src++));
            }
        }

        requestPresent();
    }

    void Runtime::pumpEvents() noexcept
    {
        if (_hwnd == nullptr && _baseWidth > 0 && _baseHeight > 0)
            (void)ensureWindow();

        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                _shouldQuit = true;
                return;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        const uint64_t nowUs = monotonicMicros();
        if (_statusUntilUs != 0 && nowUs >= _statusUntilUs)
        {
            _statusUntilUs = 0;
            _statusText.clear();
            refreshWindowTitle();
        }

        serviceRecording(nowUs);
        if (_dirty && (_lastPresentUs == 0 || (nowUs - _lastPresentUs) >= 16'000U))
            presentNow();
    }

    void Runtime::pinModeInput(uint8_t pin, pipcore::InputMode mode) noexcept
    {
        _pinModes[pin] = mode;
    }

    bool Runtime::digitalRead(uint8_t pin) const noexcept
    {
        const bool pressed = pinPressed(pin);
        switch (_pinModes[pin])
        {
        case pipcore::InputMode::Pullup:
            return !pressed;
        case pipcore::InputMode::Pulldown:
            return pressed;
        default:
            return pressed;
        }
    }

    uint32_t Runtime::nowMs() noexcept
    {
        pumpEvents();
        return static_cast<uint32_t>(monotonicMicros() / 1000U);
    }

    uint64_t Runtime::nowMicros() noexcept
    {
        pumpEvents();
        return monotonicMicros();
    }

    void Runtime::delayMs(uint32_t ms) noexcept
    {
        const uint64_t endUs = monotonicMicros() + static_cast<uint64_t>(ms) * 1000U;
        while (!shouldQuit() && monotonicMicros() < endUs)
        {
            pumpEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    bool Runtime::keyDown(uint16_t vk) const noexcept
    {
        return (vk < _keys.size()) ? _keys[vk] : false;
    }

    bool Runtime::mouseButtonDown(uint8_t button) const noexcept
    {
        return (button < _mouseButtons.size()) ? _mouseButtons[button] : false;
    }

    bool Runtime::consumeMouseDelta(int32_t &dx, int32_t &dy) noexcept
    {
        pumpEvents();
        dx = _mouseDeltaX;
        dy = _mouseDeltaY;
        _mouseDeltaX = 0;
        _mouseDeltaY = 0;
        return dx != 0 || dy != 0;
    }

    void Runtime::setMouseCapture(bool enabled) noexcept
    {
        if (_mouseCaptured == enabled)
            return;

        _mouseCaptured = enabled;
        updateMouseCaptureState();
    }

    int Runtime::serialAvailable() noexcept
    {
        pumpEvents();
        return static_cast<int>(_serialInput.size() - _serialReadOffset);
    }

    int Runtime::serialRead() noexcept
    {
        pumpEvents();
        if (_serialReadOffset >= _serialInput.size())
            return -1;

        const int value = static_cast<unsigned char>(_serialInput[_serialReadOffset++]);
        if (_serialReadOffset >= _serialInput.size())
        {
            _serialInput.clear();
            _serialReadOffset = 0;
        }
        return value;
    }

    size_t Runtime::serialWrite(const uint8_t *data, size_t len) noexcept
    {
        if (!data || len == 0)
            return 0;

        std::cout.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(len));
        std::cout.flush();
        return len;
    }

    size_t Runtime::serialWrite(uint8_t value) noexcept
    {
        return serialWrite(&value, 1U);
    }

    bool Runtime::ensureWindow() noexcept
    {
        if (_hwnd)
            return true;
        if (_width == 0 || _height == 0)
            return false;

        enableDpiAwareness();

        static bool classRegistered = false;
        if (!classRegistered)
        {
            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(wc);
            wc.hInstance = _instance;
            wc.lpfnWndProc = &Runtime::WindowProc;
            wc.lpszClassName = kWindowClassName;
            wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
            wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            if (!RegisterClassExW(&wc))
            {
                const DWORD err = GetLastError();
                if (err != ERROR_CLASS_ALREADY_EXISTS)
                    return false;
            }
            classRegistered = true;
        }

        RECT rect = {0, 0, static_cast<LONG>(_width * _scale), static_cast<LONG>(_height * _scale)};
        AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

        _hwnd = CreateWindowExW(0,
                                kWindowClassName,
                                _windowTitle.c_str(),
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                rect.right - rect.left,
                                rect.bottom - rect.top,
                                nullptr,
                                nullptr,
                                _instance,
                                this);
        if (!_hwnd)
            return false;

        refreshWindowTitle();
        ShowWindow(_hwnd, SW_SHOW);
        UpdateWindow(_hwnd);
        return true;
    }

    void Runtime::resizeWindow() noexcept
    {
        if (!_hwnd)
            return;

        RECT rect = {0, 0, static_cast<LONG>(_width * _scale), static_cast<LONG>(_height * _scale)};
        AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
        SetWindowPos(_hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void Runtime::updateBitmapInfo() noexcept
    {
        std::memset(&_bitmapInfo, 0, sizeof(_bitmapInfo));
        _bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        _bitmapInfo.bmiHeader.biWidth = static_cast<LONG>(_width);
        _bitmapInfo.bmiHeader.biHeight = -static_cast<LONG>(_height);
        _bitmapInfo.bmiHeader.biPlanes = 1;
        _bitmapInfo.bmiHeader.biBitCount = 32;
        _bitmapInfo.bmiHeader.biCompression = BI_RGB;
    }

    void Runtime::requestPresent() noexcept
    {
        _dirty = true;
        if (_hwnd)
            InvalidateRect(_hwnd, nullptr, FALSE);
    }

    void Runtime::presentNow() noexcept
    {
        if (!_hwnd)
            return;
        _dirty = false;
        _lastPresentUs = monotonicMicros();
        RedrawWindow(_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    }

    void Runtime::serviceRecording(uint64_t nowUs) noexcept
    {
        if (!_recording.active)
            return;

        while (_recording.active && nowUs >= _recording.nextFrameUs)
        {
            const LONGLONG sampleTime = static_cast<LONGLONG>(_recording.frameIndex) * _recording.frameDurationHns;
            if (!encodeRecordingFrame(sampleTime))
            {
                stopRecording();
                setTransientStatus(L"Video failed");
                break;
            }

            ++_recording.frameIndex;
            _recording.nextFrameUs += 1'000'000ULL / _recording.fps;
        }
    }

    bool Runtime::restartProcess() noexcept
    {
        wchar_t exePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0)
            return false;

        wchar_t workDir[MAX_PATH] = {};
        if (GetCurrentDirectoryW(MAX_PATH, workDir) == 0)
            return false;

        STARTUPINFOW si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);

        const BOOL ok = CreateProcessW(exePath,
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       FALSE,
                                       0,
                                       nullptr,
                                       workDir,
                                       &si,
                                       &pi);
        if (!ok)
            return false;

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        if (_hwnd)
            PostMessageW(_hwnd, WM_CLOSE, 0, 0);
        else
            _shouldQuit = true;

        return true;
    }

    bool Runtime::saveScreenshot() noexcept
    {
        if (_framebuffer.empty() || _width == 0 || _height == 0)
            return false;

        try
        {
            namespace fs = std::filesystem;
            fs::path dir = fs::current_path() / "shots";
            fs::create_directories(dir);

            SYSTEMTIME st = {};
            GetLocalTime(&st);

            wchar_t nameBuf[128];
            std::swprintf(nameBuf,
                          sizeof(nameBuf) / sizeof(nameBuf[0]),
                          L"pipgui_%04u%02u%02u_%02u%02u%02u_%03u.png",
                          static_cast<unsigned>(st.wYear),
                          static_cast<unsigned>(st.wMonth),
                          static_cast<unsigned>(st.wDay),
                          static_cast<unsigned>(st.wHour),
                          static_cast<unsigned>(st.wMinute),
                          static_cast<unsigned>(st.wSecond),
                          static_cast<unsigned>(st.wMilliseconds));

            const fs::path outPath = dir / nameBuf;

            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            const bool shouldUninit = SUCCEEDED(hr);
            if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
                return false;

            IWICImagingFactory *factory = nullptr;
            IWICBitmapEncoder *encoder = nullptr;
            IWICStream *stream = nullptr;
            IWICBitmapFrameEncode *frame = nullptr;
            IPropertyBag2 *props = nullptr;

            hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
            if (SUCCEEDED(hr))
                hr = factory->CreateStream(&stream);
            if (SUCCEEDED(hr))
                hr = stream->InitializeFromFilename(outPath.c_str(), GENERIC_WRITE);
            if (SUCCEEDED(hr))
                hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
            if (SUCCEEDED(hr))
                hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
            if (SUCCEEDED(hr))
                hr = encoder->CreateNewFrame(&frame, &props);
            if (SUCCEEDED(hr))
                hr = frame->Initialize(props);
            if (SUCCEEDED(hr))
                hr = frame->SetSize(_width, _height);
            WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
            if (SUCCEEDED(hr))
                hr = frame->SetPixelFormat(&format);
            if (SUCCEEDED(hr))
                hr = frame->WritePixels(_height,
                                        static_cast<UINT>(_width) * 4U,
                                        static_cast<UINT>(_framebuffer.size() * sizeof(uint32_t)),
                                        reinterpret_cast<BYTE *>(_framebuffer.data()));
            if (SUCCEEDED(hr))
                hr = frame->Commit();
            if (SUCCEEDED(hr))
                hr = encoder->Commit();

            safeRelease(props);
            safeRelease(frame);
            safeRelease(encoder);
            safeRelease(stream);
            safeRelease(factory);
            if (shouldUninit)
                CoUninitialize();

            if (FAILED(hr))
                return false;

            setTransientStatus(L"Screenshot saved");
            return true;
        }
        catch (...)
        {
            setTransientStatus(L"Screenshot failed");
            return false;
        }
    }

    bool Runtime::toggleRecording() noexcept
    {
        if (_recording.active)
        {
            stopRecording();
            setTransientStatus(L"Recording stopped");
            return true;
        }

        if (startRecording())
        {
            setTransientStatus(L"Recording started");
            return true;
        }

        setTransientStatus(L"Recording failed");
        return false;
    }

    bool Runtime::startRecording() noexcept
    {
        if (_recording.active || _framebuffer.empty() || _width == 0 || _height == 0)
            return false;

        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr))
            return false;

        namespace fs = std::filesystem;
        fs::create_directories(fs::current_path() / "videos");

        SYSTEMTIME st = {};
        GetLocalTime(&st);
        wchar_t nameBuf[128];
        std::swprintf(nameBuf,
                      sizeof(nameBuf) / sizeof(nameBuf[0]),
                      L"pipgui_%04u%02u%02u_%02u%02u%02u_%03u.mp4",
                      static_cast<unsigned>(st.wYear),
                      static_cast<unsigned>(st.wMonth),
                      static_cast<unsigned>(st.wDay),
                      static_cast<unsigned>(st.wHour),
                      static_cast<unsigned>(st.wMinute),
                      static_cast<unsigned>(st.wSecond),
                      static_cast<unsigned>(st.wMilliseconds));

        const std::wstring outPath = (fs::current_path() / "videos" / nameBuf).wstring();

        IMFAttributes *attrs = nullptr;
        IMFSinkWriter *writer = nullptr;
        IMFMediaType *outType = nullptr;
        IMFMediaType *inType = nullptr;
        DWORD streamIndex = 0;

        hr = MFCreateAttributes(&attrs, 1);
        if (SUCCEEDED(hr))
            hr = attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        if (SUCCEEDED(hr))
            hr = MFCreateSinkWriterFromURL(outPath.c_str(), nullptr, attrs, &writer);
        if (SUCCEEDED(hr))
            hr = MFCreateMediaType(&outType);
        if (SUCCEEDED(hr))
            hr = outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (SUCCEEDED(hr))
            hr = outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        if (SUCCEEDED(hr))
            hr = outType->SetUINT32(MF_MT_AVG_BITRATE, 6'000'000);
        if (SUCCEEDED(hr))
            hr = MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, _width, _height);
        if (SUCCEEDED(hr))
            hr = MFSetAttributeRatio(outType, MF_MT_FRAME_RATE, 30, 1);
        if (SUCCEEDED(hr))
            hr = MFSetAttributeRatio(outType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        if (SUCCEEDED(hr))
            hr = writer->AddStream(outType, &streamIndex);
        if (SUCCEEDED(hr))
            hr = MFCreateMediaType(&inType);
        if (SUCCEEDED(hr))
            hr = inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (SUCCEEDED(hr))
            hr = inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        if (SUCCEEDED(hr))
            hr = MFSetAttributeSize(inType, MF_MT_FRAME_SIZE, _width, _height);
        if (SUCCEEDED(hr))
            hr = MFSetAttributeRatio(inType, MF_MT_FRAME_RATE, 30, 1);
        if (SUCCEEDED(hr))
            hr = MFSetAttributeRatio(inType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        if (SUCCEEDED(hr))
            hr = writer->SetInputMediaType(streamIndex, inType, nullptr);
        if (SUCCEEDED(hr))
            hr = writer->BeginWriting();

        safeRelease(inType);
        safeRelease(outType);
        safeRelease(attrs);

        if (FAILED(hr))
        {
            safeRelease(writer);
            MFShutdown();
            return false;
        }

        _recording.writer = writer;
        _recording.streamIndex = streamIndex;
        _recording.fps = 30;
        _recording.frameDurationHns = 10'000'000LL / static_cast<LONGLONG>(_recording.fps);
        _recording.frameIndex = 0;
        _recording.nextFrameUs = monotonicMicros();
        _recording.scratch.assign(_framebuffer.size(), 0);
        _recording.active = true;
        refreshWindowTitle();
        return true;
    }

    void Runtime::stopRecording() noexcept
    {
        if (!_recording.active)
            return;

        IMFSinkWriter *writer = static_cast<IMFSinkWriter *>(_recording.writer);
        if (writer)
        {
            writer->Finalize();
            writer->Release();
        }

        _recording = {};
        MFShutdown();
        refreshWindowTitle();
    }

    bool Runtime::encodeRecordingFrame(LONGLONG sampleTimeHns) noexcept
    {
        if (!_recording.active || _framebuffer.empty())
            return false;

        IMFSinkWriter *writer = static_cast<IMFSinkWriter *>(_recording.writer);
        if (!writer)
            return false;

        IMFSample *sample = nullptr;
        IMFMediaBuffer *buffer = nullptr;
        const DWORD frameBytes = static_cast<DWORD>(_framebuffer.size() * sizeof(uint32_t));
        const size_t rowPixels = static_cast<size_t>(_width);

        if (_recording.scratch.size() != _framebuffer.size())
            _recording.scratch.resize(_framebuffer.size());

        for (uint32_t row = 0; row < _height; ++row)
        {
            const uint32_t *src = _framebuffer.data() + static_cast<size_t>(_height - 1U - row) * rowPixels;
            uint32_t *dstRow = _recording.scratch.data() + static_cast<size_t>(row) * rowPixels;
            std::memcpy(dstRow, src, rowPixels * sizeof(uint32_t));
        }

        HRESULT hr = MFCreateMemoryBuffer(frameBytes, &buffer);
        BYTE *dst = nullptr;
        if (SUCCEEDED(hr))
            hr = buffer->Lock(&dst, nullptr, nullptr);
        if (SUCCEEDED(hr))
            std::memcpy(dst, _recording.scratch.data(), frameBytes);
        if (SUCCEEDED(hr))
            hr = buffer->Unlock();
        if (SUCCEEDED(hr))
            hr = buffer->SetCurrentLength(frameBytes);
        if (SUCCEEDED(hr))
            hr = MFCreateSample(&sample);
        if (SUCCEEDED(hr))
            hr = sample->AddBuffer(buffer);
        if (SUCCEEDED(hr))
            hr = sample->SetSampleTime(sampleTimeHns);
        if (SUCCEEDED(hr))
            hr = sample->SetSampleDuration(_recording.frameDurationHns);
        if (SUCCEEDED(hr))
            hr = writer->WriteSample(_recording.streamIndex, sample);

        safeRelease(sample);
        safeRelease(buffer);
        return SUCCEEDED(hr);
    }

    void Runtime::setTransientStatus(const std::wstring &message, uint32_t durationMs) noexcept
    {
        _statusText = message;
        _statusUntilUs = monotonicMicros() + static_cast<uint64_t>(durationMs) * 1000ULL;
        refreshWindowTitle();
    }

    void Runtime::refreshWindowTitle() noexcept
    {
        if (!_hwnd)
            return;

        std::wstring title = _windowTitle;
        if (_recording.active)
            title += L"  |  REC";
        if (!_statusText.empty())
        {
            title += L"  |  ";
            title += _statusText;
        }
        SetWindowTextW(_hwnd, title.c_str());
    }

    void Runtime::handleKey(UINT vk, bool down) noexcept
    {
        if (vk < _keys.size())
            _keys[vk] = down;

        switch (vk)
        {
        case VK_LEFT:
        case 'A':
            _prevDown = down;
            break;
        case VK_RIGHT:
        case 'D':
            _nextDown = down;
            break;
        case VK_RETURN:
        case VK_SPACE:
            _selectDown = down;
            break;
        default:
            break;
        }
    }

    void Runtime::handleMouseMove(LPARAM lParam) noexcept
    {
        if (!_mouseCaptured || !_hwnd)
            return;

        if (_ignoreNextMouseMove)
        {
            _ignoreNextMouseMove = false;
            return;
        }

        RECT client = {};
        if (!GetClientRect(_hwnd, &client))
            return;

        const int32_t centerX = (client.right - client.left) / 2;
        const int32_t centerY = (client.bottom - client.top) / 2;
        const int32_t mouseX = GET_X_LPARAM(lParam);
        const int32_t mouseY = GET_Y_LPARAM(lParam);
        const int32_t dx = mouseX - centerX;
        const int32_t dy = mouseY - centerY;

        if (dx == 0 && dy == 0)
            return;

        _mouseDeltaX += dx;
        _mouseDeltaY += dy;
        centerCursorInClient();
    }

    void Runtime::updateMouseCaptureState() noexcept
    {
        if (!_hwnd)
            return;

        if (_mouseCaptured)
        {
            RECT client = {};
            GetClientRect(_hwnd, &client);
            POINT tl = {client.left, client.top};
            POINT br = {client.right, client.bottom};
            ClientToScreen(_hwnd, &tl);
            ClientToScreen(_hwnd, &br);
            RECT clip = {tl.x, tl.y, br.x, br.y};
            ClipCursor(&clip);
            SetCapture(_hwnd);
            if (!_cursorHidden)
            {
                while (ShowCursor(FALSE) >= 0)
                {
                }
                _cursorHidden = true;
            }
            centerCursorInClient();
        }
        else
        {
            ClipCursor(nullptr);
            if (GetCapture() == _hwnd)
                ReleaseCapture();
            if (_cursorHidden)
            {
                while (ShowCursor(TRUE) < 0)
                {
                }
                _cursorHidden = false;
            }
            _mouseDeltaX = 0;
            _mouseDeltaY = 0;
            _ignoreNextMouseMove = false;
        }
    }

    void Runtime::centerCursorInClient() noexcept
    {
        if (!_hwnd)
            return;

        RECT client = {};
        if (!GetClientRect(_hwnd, &client))
            return;

        POINT center = {(client.right - client.left) / 2, (client.bottom - client.top) / 2};
        ClientToScreen(_hwnd, &center);
        _ignoreNextMouseMove = true;
        SetCursorPos(center.x, center.y);
    }

    void Runtime::resetInputState() noexcept
    {
        _keys.fill(false);
        _mouseButtons.fill(false);
        _prevDown = false;
        _nextDown = false;
        _selectDown = false;
        _mouseDeltaX = 0;
        _mouseDeltaY = 0;
    }

    void Runtime::pushSerialChar(char ch) noexcept
    {
        if (_serialReadOffset != 0 && _serialReadOffset >= _serialInput.size())
        {
            _serialInput.clear();
            _serialReadOffset = 0;
        }
        _serialInput.push_back(ch);
    }

    bool Runtime::pinPressed(uint8_t pin) const noexcept
    {
        if (pin == kPrevPin)
            return _prevDown;
        if (pin == kNextPin)
            return _nextDown;
        if (pin == kSelectPin)
            return _selectDown;
        return false;
    }

    LRESULT CALLBACK Runtime::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
    {
        Runtime *self = reinterpret_cast<Runtime *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE)
        {
            const auto *cs = reinterpret_cast<const CREATESTRUCTW *>(lParam);
            self = static_cast<Runtime *>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }

        if (!self)
            return DefWindowProcW(hwnd, msg, wParam, lParam);

        switch (msg)
        {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            self->setMouseCapture(false);
            self->_shouldQuit = true;
            self->_hwnd = nullptr;
            PostQuitMessage(0);
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                self->setMouseCapture(false);
                self->resetInputState();
            }
            return 0;
        case WM_KILLFOCUS:
            self->setMouseCapture(false);
            self->resetInputState();
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if ((lParam & (1LL << 30)) == 0)
            {
                if (wParam == VK_F1)
                {
                    (void)self->restartProcess();
                    return 0;
                }
                if (wParam == VK_F2)
                {
                    (void)self->saveScreenshot();
                    return 0;
                }
                if (wParam == VK_F3)
                {
                    (void)self->toggleRecording();
                    return 0;
                }
                if (wParam == VK_ESCAPE && self->isMouseCaptured())
                {
                    self->setMouseCapture(false);
                    self->setTransientStatus(L"Mouse released");
                    return 0;
                }
            }
            self->handleKey(static_cast<UINT>(wParam), true);
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            self->handleKey(static_cast<UINT>(wParam), false);
            return 0;
        case WM_LBUTTONDOWN:
            self->_mouseButtons[0] = true;
            SetFocus(hwnd);
            return 0;
        case WM_LBUTTONUP:
            self->_mouseButtons[0] = false;
            return 0;
        case WM_RBUTTONDOWN:
            self->_mouseButtons[1] = true;
            SetFocus(hwnd);
            return 0;
        case WM_RBUTTONUP:
            self->_mouseButtons[1] = false;
            return 0;
        case WM_MBUTTONDOWN:
            self->_mouseButtons[2] = true;
            SetFocus(hwnd);
            return 0;
        case WM_MBUTTONUP:
            self->_mouseButtons[2] = false;
            return 0;
        case WM_MOUSEMOVE:
            self->handleMouseMove(lParam);
            return 0;
        case WM_CHAR:
            if (wParam >= L'0' && wParam <= L'3')
                self->pushSerialChar(static_cast<char>(wParam));
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC dc = BeginPaint(hwnd, &ps);
            RECT client = {};
            GetClientRect(hwnd, &client);
            if (!self->_framebuffer.empty())
            {
                SetStretchBltMode(dc, COLORONCOLOR);
                StretchDIBits(dc,
                              0,
                              0,
                              client.right - client.left,
                              client.bottom - client.top,
                              0,
                              0,
                              self->_width,
                              self->_height,
                              self->_framebuffer.data(),
                              &self->_bitmapInfo,
                              DIB_RGB_COLORS,
                              SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }

    uint32_t Runtime::color565ToArgb(uint16_t color565) noexcept
    {
        const uint8_t r5 = static_cast<uint8_t>((color565 >> 11) & 0x1Fu);
        const uint8_t g6 = static_cast<uint8_t>((color565 >> 5) & 0x3Fu);
        const uint8_t b5 = static_cast<uint8_t>(color565 & 0x1Fu);

        const uint8_t r8 = static_cast<uint8_t>((r5 * 255U + 15U) / 31U);
        const uint8_t g8 = static_cast<uint8_t>((g6 * 255U + 31U) / 63U);
        const uint8_t b8 = static_cast<uint8_t>((b5 * 255U + 15U) / 31U);
        return 0xFF000000u | (static_cast<uint32_t>(r8) << 16) | (static_cast<uint32_t>(g8) << 8) | b8;
    }
}

#endif
