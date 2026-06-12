#if defined(PIPGUI_SIM_USE_WX)

#include <PipCore/Platforms/Desktop/Runtime.hpp>

#undef INPUT
#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <wx/evtloop.h>
#include <wx/image.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/stdpaths.h>
#include <wx/sysopt.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#if defined(_WIN32)
#include <dwmapi.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

namespace pipcore::desktop
{
    namespace
    {
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

        [[nodiscard]] std::string makeBaseWindowTitle() noexcept
        {
#ifdef PIPGUI_SIM_PROJECT_NAME
            return std::string(PIPGUI_SIM_PROJECT_NAME) + " Simulator";
#else
            return "Simulator";
#endif
        }

        [[nodiscard]] std::string shellQuote(const std::string &text)
        {
            std::string out = "'";
            for (char ch : text)
                out += (ch == '\'') ? "'\\''" : std::string(1, ch);
            out += "'";
            return out;
        }

#if defined(_WIN32)
        [[nodiscard]] bool windowsAppsUseDarkTheme() noexcept
        {
            DWORD value = 1;
            DWORD bytes = sizeof(value);
            const LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER,
                                            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                                            L"AppsUseLightTheme",
                                            RRF_RT_REG_DWORD,
                                            nullptr,
                                            &value,
                                            &bytes);
            return rc == ERROR_SUCCESS && value == 0;
        }

        void applyNativeTitleBarTheme(wxWindow *window) noexcept
        {
            if (!window)
                return;
            HWND hwnd = reinterpret_cast<HWND>(window->GetHandle());
            if (!hwnd)
                return;

            const BOOL dark = windowsAppsUseDarkTheme() ? TRUE : FALSE;
            constexpr DWORD kDwmUseImmersiveDarkMode = 20;
            constexpr DWORD kDwmUseImmersiveDarkModeOld = 19;
            (void)DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
            (void)DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkModeOld, &dark, sizeof(dark));
        }
#else
        void applyNativeTitleBarTheme(wxWindow *) noexcept {}
#endif

        [[nodiscard]] bool shouldUseDarkUi() noexcept
        {
            if (const char *theme = std::getenv("PIPGUI_SIM_THEME"))
            {
                if (std::strcmp(theme, "dark") == 0 || std::strcmp(theme, "DARK") == 0)
                    return true;
                if (std::strcmp(theme, "light") == 0 || std::strcmp(theme, "LIGHT") == 0)
                    return false;
            }
#if defined(_WIN32)
            return windowsAppsUseDarkTheme();
#else
            if (const char *gtkTheme = std::getenv("GTK_THEME"))
            {
                const std::string value(gtkTheme);
                if (value.find("dark") != std::string::npos || value.find("Dark") != std::string::npos)
                    return true;
            }
            return wxSystemSettings::GetAppearance().IsDark();
#endif
        }

        void applyThemeRecursive(wxWindow *window, bool dark) noexcept
        {
            if (!window)
                return;

            const wxColour bg = dark ? wxColour(32, 32, 32) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
            const wxColour fg = dark ? wxColour(241, 241, 241) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
            window->SetBackgroundColour(bg);
            window->SetForegroundColour(fg);
            for (wxWindowList::compatibility_iterator node = window->GetChildren().GetFirst(); node; node = node->GetNext())
                applyThemeRecursive(node->GetData(), dark);
        }

        [[nodiscard]] std::filesystem::path simWorkDir() noexcept
        {
            if (const char *workDir = std::getenv("PIPGUI_SIM_WORKDIR"))
            {
                if (*workDir)
                    return std::filesystem::path(workDir);
            }
            return std::filesystem::current_path();
        }

        [[nodiscard]] std::filesystem::path timestampedPath(const char *dirName, const char *ext)
        {
            namespace fs = std::filesystem;
            fs::path dir = simWorkDir() / dirName;
            fs::create_directories(dir);

            const auto now = std::chrono::system_clock::now();
            const std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm = {};
#if defined(_WIN32)
            localtime_s(&tm, &tt);
#else
            localtime_r(&tt, &tm);
#endif
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

            char name[128];
            std::snprintf(name,
                          sizeof(name),
                          "sim_%04d%02d%02d_%02d%02d%02d_%03lld.%s",
                          tm.tm_year + 1900,
                          tm.tm_mon + 1,
                          tm.tm_mday,
                          tm.tm_hour,
                          tm.tm_min,
                          tm.tm_sec,
                          static_cast<long long>(ms),
                          ext);
            return dir / name;
        }

        [[nodiscard]] std::filesystem::path controlPath(const char *name)
        {
            return simWorkDir() / name;
        }

        void writeControlFile(const char *name) noexcept
        {
            try
            {
                std::ofstream out(controlPath(name), std::ios::binary | std::ios::trunc);
                out << "1\n";
            }
            catch (...)
            {
            }
        }

        [[nodiscard]] std::string formatThousands(uint64_t value)
        {
            std::string text = std::to_string(value);
            for (int pos = static_cast<int>(text.size()) - 3; pos > 0; pos -= 3)
                text.insert(static_cast<size_t>(pos), " ");
            return text;
        }

        [[nodiscard]] std::string padLeft(std::string text, size_t width)
        {
            if (text.size() >= width)
                return text;
            return std::string(width - text.size(), ' ') + text;
        }

        [[nodiscard]] std::string formatMilliseconds(uint64_t micros)
        {
            char text[32];
            std::snprintf(text, sizeof(text), "%05.2f ms", static_cast<double>(micros) / 1000.0);
            return text;
        }

        [[nodiscard]] wxString formatRecordElapsed(uint64_t micros)
        {
            const uint64_t totalSeconds = micros / 1'000'000ULL;
            const uint64_t hours = totalSeconds / 3600ULL;
            const uint64_t minutes = (totalSeconds / 60ULL) % 60ULL;
            const uint64_t seconds = totalSeconds % 60ULL;
            return wxString::Format("REC %02llu:%02llu:%02llu",
                                    static_cast<unsigned long long>(hours),
                                    static_cast<unsigned long long>(minutes),
                                    static_cast<unsigned long long>(seconds));
        }

        void drawMetricText(wxDC &dc, const wxString &text, int x, int y, int maxWidth)
        {
            wxString out = text;
            wxCoord w = 0;
            wxCoord h = 0;
            dc.GetTextExtent(out, &w, &h);
            while (w > maxWidth && out.size() > 4)
            {
                out.RemoveLast();
                dc.GetTextExtent(out + "...", &w, &h);
            }
            if (out != text)
                out += "...";
            dc.DrawText(out, x, y);
        }

        class SimWxApp final : public wxApp
        {
        public:
            bool OnInit() override
            {
#if defined(_WIN32)
                wxSystemOptions::SetOption("msw.dark-mode", windowsAppsUseDarkTheme() ? 2 : 0);
#endif
                wxInitAllImageHandlers();
                return true;
            }
        };
    }
}

wxIMPLEMENT_APP_NO_MAIN(pipcore::desktop::SimWxApp);

namespace pipcore::desktop
{
    class WxSimCanvas final : public wxPanel
    {
    public:
        explicit WxSimCanvas(wxWindow *parent, Runtime &runtime)
            : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE),
              _runtime(runtime)
        {
            SetBackgroundStyle(wxBG_STYLE_PAINT);
            Bind(wxEVT_PAINT, &WxSimCanvas::onPaint, this);
        }

        void syncNativeSize()
        {
            const int w = std::max(1, static_cast<int>(_runtime._width) * static_cast<int>(_runtime._scale));
            const int h = std::max(1, static_cast<int>(_runtime._height) * static_cast<int>(_runtime._scale));
            SetMinSize(wxSize(w, h));
            SetInitialSize(wxSize(w, h));
        }

    private:
        void onPaint(wxPaintEvent &)
        {
            wxAutoBufferedPaintDC dc(this);
            dc.SetBackground(wxBrush(shouldUseDarkUi() ? wxColour(18, 18, 18) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
            dc.Clear();

            if (_runtime._framebuffer.empty() || _runtime._width == 0 || _runtime._height == 0)
                return;

            wxImage image(_runtime._width, _runtime._height, false);
            unsigned char *rgb = image.GetData();
            for (size_t i = 0; i < _runtime._framebuffer.size(); ++i)
            {
                const uint32_t c = _runtime.presentColor(_runtime._framebuffer[i]);
                rgb[i * 3U + 0U] = static_cast<unsigned char>((c >> 16) & 0xFFU);
                rgb[i * 3U + 1U] = static_cast<unsigned char>((c >> 8) & 0xFFU);
                rgb[i * 3U + 2U] = static_cast<unsigned char>(c & 0xFFU);
            }

            const wxSize client = GetClientSize();
            const double sx = static_cast<double>(client.GetWidth()) / static_cast<double>(_runtime._width);
            const double sy = static_cast<double>(client.GetHeight()) / static_cast<double>(_runtime._height);
            const double nativeScale = static_cast<double>(_runtime._scale);
            const double scale = std::max(1.0, std::min(nativeScale, std::min(sx, sy)));
            const int outW = std::max(1, static_cast<int>(_runtime._width * scale));
            const int outH = std::max(1, static_cast<int>(_runtime._height * scale));
            const int outX = (client.GetWidth() - outW) / 2;
            const int outY = (client.GetHeight() - outH) / 2;

            wxBitmap bmp(image.Scale(outW, outH, wxIMAGE_QUALITY_NEAREST));
            dc.DrawBitmap(bmp, outX, outY, false);
        }

    private:
        Runtime &_runtime;
    };

    class WxMetricsPanel final : public wxPanel
    {
    public:
        explicit WxMetricsPanel(wxWindow *parent, Runtime &runtime)
            : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE),
              _runtime(runtime)
        {
            auto *root = new wxBoxSizer(wxVERTICAL);
            auto *grid = new wxFlexGridSizer(2, 2, 4, 8);
            grid->AddGrowableCol(1, 1);

            _redrawnLabel = new wxStaticText(this, wxID_ANY, "Redrawn");
            _redrawnValue = new wxStaticText(this, wxID_ANY, "");
            _heapLabel = new wxStaticText(this, wxID_ANY, "Heap");
            _heapValue = new wxStaticText(this, wxID_ANY, "");
            _timingValue = new wxStaticText(this, wxID_ANY, "");
            _heapBar = new wxGauge(this, wxID_ANY, 240, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL);

            grid->Add(_redrawnLabel, 0, wxALIGN_CENTER_VERTICAL);
            grid->Add(_redrawnValue, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
            grid->Add(_heapLabel, 0, wxALIGN_CENTER_VERTICAL);
            grid->Add(_heapValue, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

            root->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
            root->Add(_heapBar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
            root->Add(_timingValue, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);
            SetSizer(root);
            syncNativeSize();
            syncTheme();
        }

        void syncNativeSize()
        {
            const int w = std::max(1, static_cast<int>(_runtime._width) * static_cast<int>(_runtime._scale));
            SetMinSize(wxSize(w, 112));
            SetInitialSize(wxSize(w, 112));
        }

        void syncTheme()
        {
            const bool dark = shouldUseDarkUi();
            const wxColour bg = dark ? wxColour(24, 24, 24) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
            const wxColour fg = dark ? wxColour(241, 241, 241) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
            const wxColour muted = dark ? wxColour(176, 176, 176) : wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
            const wxColour track = dark ? wxColour(52, 52, 52) : wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
            const wxColour fill = wxColour(0, 120, 215);

            SetBackgroundColour(bg);
            SetForegroundColour(fg);
            for (wxWindowList::compatibility_iterator node = GetChildren().GetFirst(); node; node = node->GetNext())
            {
                wxWindow *child = node->GetData();
                if (child)
                {
                    child->SetBackgroundColour(bg);
                    child->SetForegroundColour(fg);
                }
            }
            if (_redrawnLabel)
                _redrawnLabel->SetForegroundColour(muted);
            if (_heapLabel)
                _heapLabel->SetForegroundColour(muted);
            if (_timingValue)
                _timingValue->SetForegroundColour(muted);
            if (_heapBar)
            {
                _heapBar->SetBackgroundColour(track);
                _heapBar->SetForegroundColour(fill);
            }

            wxFont mono = GetFont();
            mono.SetFamily(wxFONTFAMILY_TELETYPE);
#if defined(__linux__)
            mono.SetPointSize(std::max(8, mono.GetPointSize() - 1));
#endif
            if (_redrawnLabel)
                _redrawnLabel->SetFont(mono);
            if (_redrawnValue)
                _redrawnValue->SetFont(mono);
            if (_heapLabel)
                _heapLabel->SetFont(mono);
            if (_heapValue)
                _heapValue->SetFont(mono);
            if (_timingValue)
                _timingValue->SetFont(mono);
        }

        void syncMetrics()
        {
            const int screenPixels = std::max<int>(1, static_cast<int>(_runtime._width) * static_cast<int>(_runtime._height));
            const uint64_t redrawn = std::min<uint64_t>(_runtime._lastRedrawnPixels, static_cast<uint64_t>(screenPixels));
            const unsigned percent = static_cast<unsigned>((redrawn * 100ULL + static_cast<uint64_t>(screenPixels / 2)) /
                                                           static_cast<uint64_t>(screenPixels));
            const std::string redrawnCount = padLeft(formatThousands(redrawn), 7U);
            const std::string screenCount = padLeft(formatThousands(static_cast<uint64_t>(screenPixels)), 7U);
            if (_redrawnValue)
            {
                _redrawnValue->SetLabel(wxString::Format("%3u%% (%s / %s px)",
                                                         percent,
                                                         wxString::FromUTF8(redrawnCount.c_str()),
                                                         wxString::FromUTF8(screenCount.c_str())));
            }

            const int heapMaxKb = 240;
            const int heapKb = static_cast<int>((_runtime._simHeapBytes + 1023U) / 1024U);
            const int peakKb = static_cast<int>((_runtime._simHeapPeakBytes + 1023U) / 1024U);
            if (_heapValue)
                _heapValue->SetLabel(wxString::Format("%3d KB / %3d KB   Peak: %3d KB", heapKb, heapMaxKb, peakKb));
            if (_heapBar)
                _heapBar->SetValue(std::clamp(heapKb, 0, heapMaxKb));

            const std::string cpuText = formatMilliseconds(_runtime._lastRenderCpuUs);
            const std::string spiText = formatMilliseconds(_runtime._lastEstimatedSpiUs);
            if (_timingValue)
                _timingValue->SetLabel("Render CPU: " + wxString::FromUTF8(cpuText.c_str()) +
                                       "\nSPI estimate: " + wxString::FromUTF8(spiText.c_str()));
        }

    private:
        void onPaint(wxPaintEvent &)
        {
            wxAutoBufferedPaintDC dc(this);
            const bool dark = shouldUseDarkUi();
            const wxColour bg = dark ? wxColour(24, 24, 24) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
            const wxColour fg = dark ? wxColour(241, 241, 241) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
            const wxColour muted = dark ? wxColour(176, 176, 176) : wxColour(92, 92, 92);
            const wxColour track = dark ? wxColour(52, 52, 52) : wxColour(220, 220, 220);
            const wxColour fill = wxColour(0, 120, 215);

            dc.SetBackground(wxBrush(bg));
            dc.Clear();
            dc.SetTextForeground(fg);
            wxFont font = GetFont();
            font.SetFamily(wxFONTFAMILY_TELETYPE);
            dc.SetFont(font);

            const int width = GetClientSize().GetWidth();
            const int screenPixels = std::max<int>(1, static_cast<int>(_runtime._width) * static_cast<int>(_runtime._height));
            const uint64_t redrawn = std::min<uint64_t>(_runtime._lastRedrawnPixels, static_cast<uint64_t>(screenPixels));
            const unsigned percent = static_cast<unsigned>((redrawn * 100ULL + static_cast<uint64_t>(screenPixels / 2)) /
                                                           static_cast<uint64_t>(screenPixels));

            const std::string redrawnCount = padLeft(formatThousands(redrawn), 7U);
            const std::string screenCount = padLeft(formatThousands(static_cast<uint64_t>(screenPixels)), 7U);
            const wxString redrawnText = wxString::Format("Redrawn: %3u%% (", percent) +
                                         wxString::FromUTF8(redrawnCount.c_str()) + " / " +
                                         wxString::FromUTF8(screenCount.c_str()) + " px)";
            drawMetricText(dc, redrawnText, 8, 7, width - 16);

            const int heapMaxKb = 240;
            const int heapKb = static_cast<int>((_runtime._simHeapBytes + 1023U) / 1024U);
            const int peakKb = static_cast<int>((_runtime._simHeapPeakBytes + 1023U) / 1024U);
            const wxString heapText = wxString::Format("Heap: %3d KB / %3d KB   Peak: %3d KB", heapKb, heapMaxKb, peakKb);
            drawMetricText(dc, heapText, 8, 31, width - 16);

            const int barX = 8;
            const int barY = 56;
            const int barW = std::max(1, width - 16);
            const int barH = 8;
            const int fillW = std::clamp((barW * heapKb) / heapMaxKb, 0, barW);
            dc.SetPen(wxPen(track));
            dc.SetBrush(wxBrush(track));
            dc.DrawRectangle(barX, barY, barW, barH);
            dc.SetPen(wxPen(fill));
            dc.SetBrush(wxBrush(fill));
            dc.DrawRectangle(barX, barY, fillW, barH);

            dc.SetTextForeground(muted);
            const std::string cpuText = formatMilliseconds(_runtime._lastRenderCpuUs);
            const std::string spiText = formatMilliseconds(_runtime._lastEstimatedSpiUs);
            const wxString timing = "Render CPU: " + wxString::FromUTF8(cpuText.c_str()) +
                                    "   SPI estimate: " + wxString::FromUTF8(spiText.c_str());
            drawMetricText(dc, timing, 8, 69, width - 16);
        }

    private:
        Runtime &_runtime;
        wxStaticText *_redrawnLabel = nullptr;
        wxStaticText *_redrawnValue = nullptr;
        wxStaticText *_heapLabel = nullptr;
        wxStaticText *_heapValue = nullptr;
        wxGauge *_heapBar = nullptr;
        wxStaticText *_timingValue = nullptr;
    };

    class WxSimFrame final : public wxFrame
    {
    public:
        explicit WxSimFrame(Runtime &runtime)
            : wxFrame(nullptr,
                      wxID_ANY,
                      wxString::FromUTF8(runtime._windowTitle.c_str()),
                      wxDefaultPosition,
                      wxDefaultSize,
                      wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN),
              _runtime(runtime)
        {
            auto *root = new wxBoxSizer(wxVERTICAL);
            auto *top = new wxBoxSizer(wxHORIZONTAL);

            auto *left = new wxBoxSizer(wxVERTICAL);
            _canvas = new WxSimCanvas(this, runtime);
            _canvas->syncNativeSize();
            left->Add(_canvas, 0, wxALL, 8);
            _metrics = new WxMetricsPanel(this, runtime);
            left->Add(_metrics, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
            top->Add(left, 0, wxEXPAND);

            auto *side = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(360, -1));
            auto *panel = new wxBoxSizer(wxVERTICAL);

            auto addLabel = [&](const char *text)
            {
                auto *label = new wxStaticText(side, wxID_ANY, wxString::FromUTF8(text));
                panel->Add(label, 0, wxLEFT | wxRIGHT | wxTOP, 8);
            };

            addLabel("Runtime");
            _pause = new wxCheckBox(side, wxID_ANY, "Pause");
            panel->Add(_pause, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
            auto *stepCountRow = new wxBoxSizer(wxHORIZONTAL);
            auto *stepLabel = new wxStaticText(side, wxID_ANY, "Step frames");
            _stepCount = new wxSpinCtrl(side, wxID_ANY, "1", wxDefaultPosition, wxSize(136, -1), wxSP_ARROW_KEYS, 1, 120, 1);
            stepCountRow->Add(stepLabel, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
            stepCountRow->Add(_stepCount, 0, 0, 0);
            panel->Add(stepCountRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

            auto *stepButtonRow = new wxBoxSizer(wxHORIZONTAL);
            auto *stepBack = new wxButton(side, wxID_ANY, "Back");
            auto *stepForward = new wxButton(side, wxID_ANY, "Forward");
            stepButtonRow->Add(stepBack, 1, wxRIGHT, 6);
            stepButtonRow->Add(stepForward, 1, 0, 0);
            panel->Add(stepButtonRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);

            addLabel("Time scale");
            _timeScaleValue = new wxStaticText(side, wxID_ANY, "100%");
            _timeScale = new wxSlider(side, wxID_ANY, 100, 5, 200, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
            panel->Add(_timeScaleValue, 0, wxLEFT | wxRIGHT | wxTOP, 8);
            panel->Add(_timeScale, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

            addLabel("SPI bottleneck");
            _spiLimit = new wxCheckBox(side, wxID_ANY, "Limit SPI bandwidth");
            panel->Add(_spiLimit, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
            _spiValue = new wxStaticText(side, wxID_ANY, "80 MHz");
            _spi = new wxSlider(side, wxID_ANY, 80, 27, 80, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
            panel->Add(_spiValue, 0, wxLEFT | wxRIGHT | wxTOP, 8);
            panel->Add(_spi, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

            _rgb565 = new wxCheckBox(side, wxID_ANY, "RGB565 device preview");
            panel->Add(_rgb565, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

            auto *shots = new wxButton(side, wxID_ANY, "Screenshot");
            auto *record = new wxButton(side, wxID_ANY, "Record / stop");
            auto *restart = new wxButton(side, wxID_ANY, "Restart firmware");
            panel->Add(shots, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
            panel->Add(record, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
            panel->Add(restart, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

            panel->AddStretchSpacer(1);
            side->SetSizer(panel);
            top->Add(side, 0, wxEXPAND | wxRIGHT | wxTOP | wxBOTTOM, 8);
            root->Add(top, 0, wxEXPAND);

            _log = new wxTextCtrl(this,
                                  wxID_ANY,
                                  "",
                                  wxDefaultPosition,
                                  wxSize(-1, 150),
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
            root->Add(_log, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
            SetSizer(root);

            _runtime._wxCanvas = _canvas;
            _runtime._wxMetrics = _metrics;
            _runtime._wxLog = _log;

            auto bindInput = [this](wxWindow *window)
            {
                window->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &event)
                             {
                    const int key = event.GetKeyCode();
                    _runtime.handleKey(key, true);
                    if (key != WXK_LEFT && key != WXK_RIGHT && key != WXK_RETURN && key != WXK_SPACE &&
                        key != WXK_F1 && key != WXK_F2 && key != WXK_F3 &&
                        key != 'A' && key != 'D')
                    {
                        event.Skip();
                    } });
                window->Bind(wxEVT_KEY_UP, [this](wxKeyEvent &event)
                             {
                    _runtime.handleKey(event.GetKeyCode(), false);
                    event.Skip(); });
                window->Bind(wxEVT_CHAR, [this](wxKeyEvent &event)
                             {
                    const int key = event.GetUnicodeKey();
                    if (key >= 32 && key <= 126)
                    {
                        _runtime.pushSerialChar(static_cast<char>(key));
                    }
                    else if (key == WXK_RETURN)
                    {
                        _runtime.pushSerialChar('\n');
                    }
                    else
                    {
                        event.Skip();
                    } });
            };
            bindInput(this);
            bindInput(_canvas);

            Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &event)
                 {
                if (!_runtime._restartRequested)
                    _runtime.markUserExit();
                _runtime._shouldQuit = true;
                event.Skip(); });
            _pause->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &)
                         {
                _runtime._paused = _pause->GetValue();
                _runtime.setTransientStatus(_runtime._paused ? "Paused" : "Running");
                syncControls(); });
            stepBack->Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
                           { _runtime.stepBack(); });
            stepForward->Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
                              { _runtime.stepFrame(); });
            _stepCount->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent &)
                             {
                _runtime._frameStepCount = static_cast<uint32_t>(_stepCount->GetValue());
                syncControls(); });
            _timeScale->Bind(wxEVT_SLIDER, [this](wxCommandEvent &)
                             {
                _runtime._timeScalePercent = static_cast<uint32_t>(_timeScale->GetValue());
                syncControls(); });
            _spiLimit->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &)
                            {
                _runtime._spiLimitEnabled = _spiLimit->GetValue();
                syncControls(); });
            _spi->Bind(wxEVT_SLIDER, [this](wxCommandEvent &)
                       {
                const uint32_t mhz = static_cast<uint32_t>(_spi->GetValue());
                _runtime._spiLimitBytesPerSec = (mhz * 1'000'000U) / 8U;
                syncControls(); });
            _rgb565->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &)
                          {
                _runtime._rgb565Preview = _rgb565->GetValue();
                _runtime.requestPresent();
                _runtime.presentNow();
                syncControls(); });
            shots->Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
                        { (void)_runtime.saveScreenshot(); });
            record->Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
                         { (void)_runtime.toggleRecording(); });
            restart->Bind(wxEVT_BUTTON, [this](wxCommandEvent &)
                          { (void)_runtime.restartProcess(); });

            CreateStatusBar();
            applyNativeTitleBarTheme(this);
            applyThemeRecursive(this, shouldUseDarkUi());
            applyStatusTheme();
            if (_metrics)
                _metrics->syncTheme();
            if (shouldUseDarkUi() && _log)
            {
                _log->SetBackgroundColour(wxColour(18, 18, 18));
                _log->SetForegroundColour(wxColour(235, 235, 235));
            }
            syncControls();
            Fit();
            SetMinSize(GetBestSize());
            Centre();
            _canvas->SetFocus();
        }

        void syncControls()
        {
            if (_pause)
                _pause->SetValue(_runtime._paused);
            if (_rgb565)
                _rgb565->SetValue(_runtime._rgb565Preview);
            if (_spiLimit)
                _spiLimit->SetValue(_runtime._spiLimitEnabled);
            if (_timeScale)
            {
                _timeScale->SetValue(static_cast<int>(_runtime._timeScalePercent));
                if (_timeScaleValue)
                    _timeScaleValue->SetLabel(wxString::Format("%u%%", _runtime._timeScalePercent));
            }
            if (_spi)
            {
                const uint32_t mhz = (_runtime._spiLimitBytesPerSec * 8U) / 1'000'000U;
                _spi->SetValue(static_cast<int>(std::clamp<uint32_t>(mhz, 27U, 80U)));
                _spi->Enable(_runtime._spiLimitEnabled);
                if (_spiValue)
                    _spiValue->SetLabel(_runtime._spiLimitEnabled ? wxString::Format("%u MHz", std::clamp<uint32_t>(mhz, 27U, 80U)) : "Unlimited");
            }
            if (_stepCount)
                _stepCount->SetValue(static_cast<int>(_runtime._frameStepCount));

            const std::string spiText = _runtime._spiLimitEnabled
                                            ? std::to_string((_runtime._spiLimitBytesPerSec * 8U) / 1'000'000U) + " MHz"
                                            : "off";
            const std::string recText = _runtime._recording.active
                                            ? ("  " + formatRecordElapsed(monotonicMicros() - _runtime._recording.startedUs).ToStdString())
                                            : std::string();
            wxString status = wxString::Format("time=%u%%  spi=", _runtime._timeScalePercent) +
                              wxString::FromUTF8(spiText.c_str()) + "  " +
                              (_runtime._rgb565Preview ? "RGB565" : "RGB888") +
                              wxString::FromUTF8(recText.c_str());
            SetStatusText(status);
            if (_metrics)
                _metrics->syncMetrics();
        }

        void applyStatusTheme()
        {
            wxStatusBar *status = GetStatusBar();
            if (!status)
                return;

            const bool dark = shouldUseDarkUi();
            status->SetBackgroundColour(dark ? wxColour(32, 32, 32) : wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
            status->SetForegroundColour(dark ? wxColour(241, 241, 241) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
        }

        void appendLog(const char *text)
        {
            if (!_log || !text)
                return;
            _log->AppendText(wxString::FromUTF8(text));
        }

        WxSimCanvas *canvas() noexcept { return _canvas; }
        void syncNativeSize()
        {
            if (_canvas)
            {
                _canvas->syncNativeSize();
                if (_metrics)
                    _metrics->syncNativeSize();
                Fit();
                SetMinSize(GetBestSize());
            }
        }

    private:
        Runtime &_runtime;
        WxSimCanvas *_canvas = nullptr;
        WxMetricsPanel *_metrics = nullptr;
        wxCheckBox *_pause = nullptr;
        wxCheckBox *_rgb565 = nullptr;
        wxCheckBox *_spiLimit = nullptr;
        wxSpinCtrl *_stepCount = nullptr;
        wxStaticText *_timeScaleValue = nullptr;
        wxSlider *_timeScale = nullptr;
        wxStaticText *_spiValue = nullptr;
        wxSlider *_spi = nullptr;
        wxTextCtrl *_log = nullptr;
    };

    Runtime &Runtime::instance() noexcept
    {
        static Runtime runtime;
        return runtime;
    }

    Runtime::Runtime() noexcept
        : _windowTitle(makeBaseWindowTitle())
    {
        _scale =
#ifdef PIPGUI_SIM_SCALE
            static_cast<uint8_t>(PIPGUI_SIM_SCALE);
#else
            2U;
#endif
        _spiLimitBytesPerSec = 80'000'000U / 8U;
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
        _frameHistory.clear();
        _frameHistoryCursor = 0;
        _historyBrowsing = false;
        _framePixelWriteCount = 0;
        _lastRedrawnPixels = 0;
        _lastRenderCpuUs = 0;
        _lastEstimatedSpiUs = 0;
        updateBitmapInfo();
        resizeWindow();
        requestPresent();
        return true;
    }

    void Runtime::fillScreen565(uint16_t color565) noexcept
    {
        if (_framebuffer.empty())
            return;
        if (_frameCpuStartUs == 0)
            _frameCpuStartUs = monotonicMicros();
        std::fill(_framebuffer.begin(), _framebuffer.end(), color565ToArgb(color565));
        _framePixelWriteCount += _framebuffer.size();
        ++_drawCallCount;
        _pixelWriteCount += _framebuffer.size();
        throttleSpiTransfer(_framebuffer.size());
        requestPresent();
    }

    void Runtime::writeRect565(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pixels, int32_t stridePixels) noexcept
    {
        if (!pixels || w <= 0 || h <= 0 || stridePixels <= 0 || _framebuffer.empty())
            return;
        const int32_t dstX1 = std::max<int32_t>(0, x);
        const int32_t dstY1 = std::max<int32_t>(0, y);
        const int32_t dstX2 = std::min<int32_t>(_width, static_cast<int32_t>(x) + w);
        const int32_t dstY2 = std::min<int32_t>(_height, static_cast<int32_t>(y) + h);
        if (dstX1 >= dstX2 || dstY1 >= dstY2)
            return;

        const size_t pixelCount = static_cast<size_t>(dstX2 - dstX1) * static_cast<size_t>(dstY2 - dstY1);
        if (_frameCpuStartUs == 0)
            _frameCpuStartUs = monotonicMicros();
        ++_drawCallCount;
        _pixelWriteCount += pixelCount;
        _framePixelWriteCount += pixelCount;
        throttleSpiTransfer(pixelCount);

        const int32_t srcOffsetX = dstX1 - x;
        const int32_t srcOffsetY = dstY1 - y;
        for (int32_t row = dstY1; row < dstY2; ++row)
        {
            uint32_t *dst = _framebuffer.data() + static_cast<size_t>(row) * _width + dstX1;
            const uint16_t *src = pixels + static_cast<size_t>(srcOffsetY + (row - dstY1)) * static_cast<size_t>(stridePixels) + srcOffsetX;
            for (int32_t col = dstX1; col < dstX2; ++col)
                *dst++ = color565ToArgb(__builtin_bswap16(*src++));
        }
        requestPresent();
    }

    void Runtime::pumpEvents() noexcept
    {
        if (!_wxFrame && _baseWidth > 0 && _baseHeight > 0)
            (void)ensureWindow();

        if (wxTheApp)
        {
            auto *eventLoop = static_cast<wxEventLoop *>(_wxEventLoop);
            while (eventLoop && eventLoop->Pending())
                eventLoop->Dispatch();
            wxTheApp->ProcessPendingEvents();
            wxTheApp->ProcessIdle();
        }

        const uint64_t nowUs = monotonicMicros();
        serviceSimClock(nowUs);
        serviceRecording(nowUs);
        if (_dirty && (_lastPresentUs == 0 || (nowUs - _lastPresentUs) >= 16'000U))
            presentNow();
    }

    void Runtime::pinModeInput(uint8_t pin, pipcore::InputMode mode) noexcept { _pinModes[pin] = mode; }

    bool Runtime::digitalRead(uint8_t pin) const noexcept
    {
        const bool pressed = pinPressed(pin);
        if (_pinModes[pin] == pipcore::InputMode::Pullup)
            return !pressed;
        return pressed;
    }

    int16_t Runtime::analogRead(uint8_t pin) const noexcept
    {
        if (pin == 34)
        {
            if (_prevDown)
                return 0;
            if (_nextDown)
                return 4095;
            return 2048;
        }
        if (pin == 35)
        {
            if (_upDown)
                return 0;
            if (_downDown)
                return 4095;
            return 2048;
        }
        return 0;
    }

    uint32_t Runtime::nowMs() noexcept
    {
        pumpEvents();
        return static_cast<uint32_t>(_simClockUs / 1000U);
    }

    uint64_t Runtime::nowMicros() noexcept
    {
        pumpEvents();
        return _simClockUs;
    }

    void Runtime::delayMs(uint32_t ms) noexcept
    {
        const uint64_t endUs = _simClockUs + static_cast<uint64_t>(ms) * 1000U;
        while (!shouldQuit() && _simClockUs < endUs)
        {
            pumpEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
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
        if (_wxFrame)
        {
            std::string text(reinterpret_cast<const char *>(data), len);
            static_cast<WxSimFrame *>(_wxFrame)->appendLog(text.c_str());
        }
        return len;
    }

    size_t Runtime::serialWrite(uint8_t value) noexcept { return serialWrite(&value, 1U); }

    void Runtime::serviceSimClock(uint64_t realNowUs) noexcept
    {
        if (_lastRealClockUs == 0)
        {
            _lastRealClockUs = realNowUs;
            return;
        }
        const uint64_t realDelta = realNowUs - _lastRealClockUs;
        _lastRealClockUs = realNowUs;
        if (_pendingFrameSteps != 0)
        {
            --_pendingFrameSteps;
            _simClockUs += 16'667U;
            return;
        }
        if (!_paused)
            _simClockUs += (realDelta * _timeScalePercent) / 100U;
    }

    void Runtime::markUserExit() noexcept
    {
        writeControlFile("_sim-user-exit");
    }

    void Runtime::markRestartRequest() noexcept
    {
        _restartRequested = true;
        writeControlFile("_sim-restart");
    }

    void Runtime::advanceFrameStep() noexcept { _pendingFrameSteps += std::max<uint32_t>(1U, _frameStepCount); }

    void Runtime::throttleSpiTransfer(size_t pixelCount) noexcept
    {
        if (!_spiLimitEnabled || _spiLimitBytesPerSec == 0 || pixelCount == 0)
            return;
        const uint64_t nowUs = monotonicMicros();
        const uint64_t bytes = pixelCount * 2U;
        const uint64_t transferUs = (bytes * 1'000'000ULL + (_spiLimitBytesPerSec - 1U)) / _spiLimitBytesPerSec;
        if (_spiReadyUs < nowUs)
            _spiReadyUs = nowUs;
        _spiReadyUs += transferUs;
        while (!shouldQuit() && monotonicMicros() < _spiReadyUs)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    uint32_t Runtime::presentColor(uint32_t argb) const noexcept
    {
        if (!_rgb565Preview)
            return argb;
        const uint8_t r = static_cast<uint8_t>((argb >> 16) & 0xFFU);
        const uint8_t g = static_cast<uint8_t>((argb >> 8) & 0xFFU);
        const uint8_t b = static_cast<uint8_t>(argb & 0xFFU);
        return 0xFF000000u |
               (static_cast<uint32_t>(r & 0xF8U) << 16) |
               (static_cast<uint32_t>(g & 0xFCU) << 8) |
               static_cast<uint32_t>(b & 0xF8U);
    }

    void Runtime::togglePause() noexcept
    {
        _paused = !_paused;
        if (_wxFrame)
            static_cast<WxSimFrame *>(_wxFrame)->syncControls();
    }

    void Runtime::stepFrame() noexcept
    {
        _paused = true;
        _historyBrowsing = false;
        advanceFrameStep();
        if (_wxFrame)
            static_cast<WxSimFrame *>(_wxFrame)->syncControls();
    }

    void Runtime::cycleTimeScale() noexcept
    {
        _timeScalePercent = (_timeScalePercent == 100) ? 50 : (_timeScalePercent == 50) ? 25
                                                          : (_timeScalePercent == 25)   ? 10
                                                                                        : 100;
        if (_wxFrame)
            static_cast<WxSimFrame *>(_wxFrame)->syncControls();
    }

    void Runtime::cycleSpiLimit() noexcept
    {
        if (!_spiLimitEnabled)
        {
            _spiLimitEnabled = true;
            _spiLimitBytesPerSec = 80'000'000U / 8U;
            if (_wxFrame)
                static_cast<WxSimFrame *>(_wxFrame)->syncControls();
            return;
        }
        const uint32_t mhz = (_spiLimitBytesPerSec * 8U) / 1'000'000U;
        if (mhz <= 27U)
        {
            _spiLimitEnabled = false;
        }
        else
        {
            _spiLimitBytesPerSec = ((mhz <= 40U) ? 27U : 40U) * 1'000'000U / 8U;
        }
        if (_wxFrame)
            static_cast<WxSimFrame *>(_wxFrame)->syncControls();
    }

    void Runtime::toggleRgb565Preview() noexcept
    {
        _rgb565Preview = !_rgb565Preview;
        requestPresent();
        if (_wxFrame)
            static_cast<WxSimFrame *>(_wxFrame)->syncControls();
    }

    void Runtime::toggleConsole() noexcept {}
    void Runtime::stepBack() noexcept
    {
        if (_frameHistory.empty())
            return;
        _paused = true;
        if (!_historyBrowsing)
        {
            _frameHistoryCursor = _frameHistory.size() - 1U;
            _historyBrowsing = true;
        }

        const size_t step = std::min<size_t>(std::max<uint32_t>(1U, _frameStepCount), _frameHistoryCursor);
        _frameHistoryCursor -= step;
        if (_frameHistory[_frameHistoryCursor].size() == _framebuffer.size())
            _framebuffer = _frameHistory[_frameHistoryCursor];
        requestPresent();
        presentNow();
        if (_wxFrame)
            static_cast<WxSimFrame *>(_wxFrame)->syncControls();
    }
    void Runtime::logLine(const char *message) noexcept { uiLogLine(message); }

    void Runtime::uiTogglePause() noexcept { togglePause(); }
    void Runtime::uiStepFrame() noexcept { stepFrame(); }
    void Runtime::uiCycleTimeScale() noexcept { cycleTimeScale(); }
    void Runtime::uiCycleSpiLimit() noexcept { cycleSpiLimit(); }
    void Runtime::uiToggleRgb565Preview() noexcept { toggleRgb565Preview(); }
    void Runtime::uiToggleRecording() noexcept { (void)toggleRecording(); }
    void Runtime::uiSaveScreenshot() noexcept { (void)saveScreenshot(); }
    void Runtime::uiRestartProcess() noexcept { (void)restartProcess(); }
    void Runtime::uiLogLine(const char *message) noexcept
    {
        if (_wxFrame && message)
        {
            static_cast<WxSimFrame *>(_wxFrame)->appendLog("[sim] ");
            static_cast<WxSimFrame *>(_wxFrame)->appendLog(message);
            static_cast<WxSimFrame *>(_wxFrame)->appendLog("\n");
        }
    }

    bool Runtime::ensureWindow() noexcept
    {
        if (_wxFrame)
            return true;
        static bool wxStarted = false;
        if (!wxStarted)
        {
#if defined(_WIN32)
            wxSystemOptions::SetOption("msw.dark-mode", windowsAppsUseDarkTheme() ? 2 : 0);
#endif
            int argc = 0;
            char **argv = nullptr;
            if (!wxEntryStart(argc, argv))
                return false;
            if (!wxTheApp || !wxTheApp->CallOnInit())
                return false;
            if (!_wxEventLoop)
            {
                auto *loop = new wxEventLoop();
                wxEventLoopBase::SetActive(loop);
                _wxEventLoop = loop;
            }
            wxStarted = true;
        }
        auto *frame = new WxSimFrame(*this);
        _wxFrame = frame;
        frame->Show(true);
        if (const char *lastExit = std::getenv("PIPGUI_SIM_LAST_EXIT"))
        {
            if (*lastExit)
                uiLogLine(lastExit);
        }
        return true;
    }

    void Runtime::resizeWindow() noexcept
    {
        if (_wxFrame)
            static_cast<WxSimFrame *>(_wxFrame)->syncNativeSize();
    }

    void Runtime::updateBitmapInfo() noexcept {}
    void Runtime::requestPresent() noexcept { _dirty = true; }

    void Runtime::presentNow() noexcept
    {
        _dirty = false;
        _lastPresentUs = monotonicMicros();
        ++_presentFrameCount;
        const uint64_t screenPixels = static_cast<uint64_t>(_width) * static_cast<uint64_t>(_height);
        _lastRedrawnPixels = std::min<uint64_t>(_framePixelWriteCount, screenPixels);
        _lastRenderCpuUs = (_frameCpuStartUs != 0 && _lastPresentUs >= _frameCpuStartUs) ? (_lastPresentUs - _frameCpuStartUs) : 0;
        _lastEstimatedSpiUs = (_spiLimitBytesPerSec != 0)
                                  ? ((_lastRedrawnPixels * 2ULL * 1'000'000ULL + (_spiLimitBytesPerSec - 1U)) / _spiLimitBytesPerSec)
                                  : 0;
        constexpr uint32_t kSimHeapMax = 240U * 1024U;
        const uint64_t dynamicEstimate = (screenPixels * 2ULL) + (_lastRedrawnPixels / 2ULL) + (_recording.active ? 16ULL * 1024ULL : 0ULL);
        _simHeapBytes = static_cast<uint32_t>(std::min<uint64_t>(kSimHeapMax, 48ULL * 1024ULL + dynamicEstimate));
        _simHeapPeakBytes = std::max(_simHeapPeakBytes, _simHeapBytes);
        _framePixelWriteCount = 0;
        _frameCpuStartUs = 0;
        if (!_historyBrowsing && !_framebuffer.empty() &&
            (_lastHistoryCaptureUs == 0 || (_lastPresentUs - _lastHistoryCaptureUs) >= 100'000U))
        {
            _frameHistory.push_back(_framebuffer);
            _lastHistoryCaptureUs = _lastPresentUs;
            constexpr size_t maxHistoryFrames = 60U;
            if (_frameHistory.size() > maxHistoryFrames)
                _frameHistory.erase(_frameHistory.begin());
            _frameHistoryCursor = _frameHistory.size() - 1U;
        }
        if (_wxCanvas)
            static_cast<WxSimCanvas *>(_wxCanvas)->Refresh(false);
        if (_wxMetrics)
            static_cast<WxMetricsPanel *>(_wxMetrics)->syncMetrics();
    }

    void Runtime::serviceRecording(uint64_t nowUs) noexcept
    {
        if (!_recording.active)
            return;
        if (nowUs >= _recording.nextFrameUs)
        {
            if (!encodeRecordingFrame(static_cast<int64_t>(_recording.frameIndex)))
            {
                stopRecording();
                uiLogLine("video recording failed");
                return;
            }
            ++_recording.frameIndex;
            _recording.nextFrameUs += 1'000'000ULL / _recording.fps;
            if (nowUs > _recording.nextFrameUs + 1'000'000ULL / _recording.fps)
                _recording.nextFrameUs = nowUs + 1'000'000ULL / _recording.fps;
            if (_wxFrame && (_recording.frameIndex % _recording.fps) == 0)
                static_cast<WxSimFrame *>(_wxFrame)->syncControls();
        }
    }

    bool Runtime::restartProcess() noexcept
    {
        markRestartRequest();
        _shouldQuit = true;
        if (_wxFrame)
            static_cast<wxFrame *>(_wxFrame)->Close(true);
        return true;
    }

    bool Runtime::saveScreenshot() noexcept
    {
        if (_framebuffer.empty())
            return false;
        try
        {
            const auto pngOut = timestampedPath("shots", "png");
            const auto bmpOut = timestampedPath("shots", "bmp");
            if (wxImage::FindHandler(wxBITMAP_TYPE_PNG) != nullptr)
            {
                wxImage image(_width, _height, false);
                unsigned char *rgb = image.GetData();
                for (size_t i = 0; i < _framebuffer.size(); ++i)
                {
                    const uint32_t c = presentColor(_framebuffer[i]);
                    rgb[i * 3U + 0U] = static_cast<unsigned char>((c >> 16) & 0xFFU);
                    rgb[i * 3U + 1U] = static_cast<unsigned char>((c >> 8) & 0xFFU);
                    rgb[i * 3U + 2U] = static_cast<unsigned char>(c & 0xFFU);
                }
                const bool ok = image.SaveFile(wxString::FromUTF8(pngOut.string().c_str()), wxBITMAP_TYPE_PNG);
                if (ok)
                {
                    const std::string message = "screenshot: " + pngOut.string();
                    uiLogLine(message.c_str());
                    return true;
                }
            }

            std::ofstream file(bmpOut, std::ios::binary | std::ios::trunc);
            if (!file)
                return false;

            const uint32_t width = _width;
            const uint32_t height = _height;
            const uint32_t rowBytes = width * 3U;
            const uint32_t rowStride = (rowBytes + 3U) & ~3U;
            const uint32_t pixelBytes = rowStride * height;
            const uint32_t fileSize = 54U + pixelBytes;
            const uint32_t dibSize = 40U;
            const int32_t signedWidth = static_cast<int32_t>(width);
            const int32_t signedHeight = static_cast<int32_t>(height);
            const uint16_t planes = 1U;
            const uint16_t bpp = 24U;
            const uint32_t compression = 0U;
            const uint32_t imageSize = pixelBytes;
            const int32_t ppm = 2835;
            const uint32_t colorsUsed = 0U;
            const uint32_t colorsImportant = 0U;

            auto write16 = [&file](uint16_t value)
            {
                file.put(static_cast<char>(value & 0xFFU));
                file.put(static_cast<char>((value >> 8) & 0xFFU));
            };
            auto write32 = [&file](uint32_t value)
            {
                file.put(static_cast<char>(value & 0xFFU));
                file.put(static_cast<char>((value >> 8) & 0xFFU));
                file.put(static_cast<char>((value >> 16) & 0xFFU));
                file.put(static_cast<char>((value >> 24) & 0xFFU));
            };
            auto writeS32 = [&write32](int32_t value)
            {
                write32(static_cast<uint32_t>(value));
            };

            file.put('B');
            file.put('M');
            write32(fileSize);
            write16(0U);
            write16(0U);
            write32(54U);
            write32(dibSize);
            writeS32(signedWidth);
            writeS32(signedHeight);
            write16(planes);
            write16(bpp);
            write32(compression);
            write32(imageSize);
            writeS32(ppm);
            writeS32(ppm);
            write32(colorsUsed);
            write32(colorsImportant);

            std::vector<uint8_t> row(rowStride, 0U);
            for (int32_t y = static_cast<int32_t>(height) - 1; y >= 0; --y)
            {
                uint8_t *dst = row.data();
                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint32_t c = presentColor(_framebuffer[static_cast<size_t>(y) * width + x]);
                    *dst++ = static_cast<uint8_t>(c & 0xFFU);
                    *dst++ = static_cast<uint8_t>((c >> 8) & 0xFFU);
                    *dst++ = static_cast<uint8_t>((c >> 16) & 0xFFU);
                }
                file.write(reinterpret_cast<const char *>(row.data()), static_cast<std::streamsize>(rowStride));
            }

            const bool ok = static_cast<bool>(file);
            const std::string message = "screenshot: " + bmpOut.string();
            uiLogLine(ok ? message.c_str() : "screenshot failed");
            return ok;
        }
        catch (...)
        {
            uiLogLine("screenshot failed");
            return false;
        }
    }

    bool Runtime::toggleRecording() noexcept
    {
        if (_recording.active)
        {
            stopRecording();
            uiLogLine("recording stopped");
            return true;
        }
        const bool ok = startRecording();
        uiLogLine(ok ? "recording started" : "recording failed: ffmpeg not found or encoder rejected input");
        return ok;
    }

    bool Runtime::startRecording() noexcept
    {
        if (_recording.active || _framebuffer.empty())
            return false;
        try
        {
            const auto out = timestampedPath("videos", "mp4");
            std::string command = "ffmpeg -hide_banner -loglevel error -y -f rawvideo -pix_fmt bgra -s ";
            command += std::to_string(_width) + "x" + std::to_string(_height);
            command += " -r 30 -i - -an -c:v libx264 -preset ultrafast -tune zerolatency -pix_fmt yuv420p -movflags +faststart ";
#if defined(_WIN32)
            command += "\"" + out.string() + "\"";
            SECURITY_ATTRIBUTES sa = {};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;

            HANDLE readPipe = nullptr;
            HANDLE writePipe = nullptr;
            if (!CreatePipe(&readPipe, &writePipe, &sa, 1U << 20))
                return false;
            SetHandleInformation(writePipe, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si = {};
            PROCESS_INFORMATION pi = {};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = readPipe;
            si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
            si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

            std::vector<char> mutableCommand(command.begin(), command.end());
            mutableCommand.push_back('\0');
            const BOOL created = CreateProcessA(nullptr,
                                                mutableCommand.data(),
                                                nullptr,
                                                nullptr,
                                                TRUE,
                                                CREATE_NO_WINDOW,
                                                nullptr,
                                                nullptr,
                                                &si,
                                                &pi);
            CloseHandle(readPipe);
            if (!created)
            {
                CloseHandle(writePipe);
                return false;
            }
            CloseHandle(pi.hThread);

            const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(writePipe), _O_BINARY | _O_WRONLY);
            if (fd < 0)
            {
                CloseHandle(pi.hProcess);
                CloseHandle(writePipe);
                return false;
            }
            FILE *pipe = _fdopen(fd, "wb");
            if (!pipe)
            {
                _close(fd);
                CloseHandle(pi.hProcess);
                return false;
            }
            _recordProcess = pi.hProcess;
#else
            std::signal(SIGPIPE, SIG_IGN);
            command += shellQuote(out.string());
            FILE *pipe = popen(command.c_str(), "w");
#endif
            if (!pipe)
                return false;
            _recordPipe = pipe;
            _recording.fps = 30;
            _recording.frameDuration = 1;
            _recording.frameIndex = 0;
            _recording.startedUs = monotonicMicros();
            _recording.nextFrameUs = _recording.startedUs;
            _recording.active = true;
            if (_wxFrame)
                static_cast<WxSimFrame *>(_wxFrame)->syncControls();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void Runtime::stopRecording() noexcept
    {
        if (!_recording.active)
            return;
        if (_recordPipe)
        {
            FILE *pipe = static_cast<FILE *>(_recordPipe);
            std::fflush(pipe);
#if defined(_WIN32)
            std::fclose(pipe);
            if (_recordProcess)
            {
                HANDLE process = static_cast<HANDLE>(_recordProcess);
                WaitForSingleObject(process, 2000);
                CloseHandle(process);
                _recordProcess = nullptr;
            }
#else
            pclose(pipe);
#endif
            _recordPipe = nullptr;
        }
        _recording = {};
        if (_wxFrame)
            static_cast<WxSimFrame *>(_wxFrame)->syncControls();
    }

    bool Runtime::encodeRecordingFrame(int64_t) noexcept
    {
        if (!_recording.active || !_recordPipe || _framebuffer.empty())
            return false;
        FILE *pipe = static_cast<FILE *>(_recordPipe);
        return std::fwrite(_framebuffer.data(), sizeof(uint32_t), _framebuffer.size(), pipe) == _framebuffer.size();
    }

    void Runtime::setTransientStatus(const char *message, uint32_t) noexcept
    {
        if (_wxFrame && message)
            static_cast<WxSimFrame *>(_wxFrame)->SetStatusText(wxString::FromUTF8(message));
    }

    void Runtime::refreshWindowTitle() noexcept {}

    void Runtime::handleKey(int key, bool down) noexcept
    {
        switch (key)
        {
        case WXK_UP:
        case 'W':
            _upDown = down;
            break;
        case WXK_DOWN:
        case 'S':
            _downDown = down;
            break;
        case WXK_LEFT:
        case 'A':
            _prevDown = down;
            break;
        case WXK_RIGHT:
        case 'D':
            _nextDown = down;
            break;
        case WXK_RETURN:
        case WXK_SPACE:
            _selectDown = down;
            break;
        case WXK_F1:
            if (down)
                togglePause();
            break;
        case WXK_F2:
            if (down)
                stepBack();
            break;
        case WXK_F3:
            if (down)
                stepFrame();
            break;
        default:
            break;
        }
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

#else
#error "Desktop simulator requires PIPGUI_SIM_USE_WX. Use Tools/Simulator to build the wxWidgets simulator."
#endif
