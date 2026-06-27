#include <PipCore/Features.hpp>

#if PIPCORE_TARGET_ESP32

#include <PipCore/Platforms/ESP32/Services/Touch.hpp>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_attr.h>
#include <esp_log.h>

#include <algorithm>
#include <cstring>
#include <cmath>

namespace pipcore::esp32::services
{
    namespace
    {
        constexpr const char *kTag = "Touch";

        constexpr uint8_t RegDevMode = 0x00;
        constexpr uint8_t RegTdStatus = 0x02;
        constexpr uint8_t RegP1 = 0x03;
        constexpr uint8_t RegP2 = 0x09;
        constexpr uint8_t RegGMode = 0xA4;

        Touch *g_instance = nullptr;

        void IRAM_ATTR touchIsrHandler(void *)
        {
            Touch::markTouched(g_instance);
        }
    }

    void IRAM_ATTR Touch::markTouched(Touch *self) noexcept
    {
        if (self)
            self->_touchedFlag = true;
    }

    Touch::~Touch()
    {
        end();
    }

    bool Touch::configure(const pipcore::TouchConfig &cfg) noexcept
    {
        end();
        if (cfg.intr < 0 || cfg.intr >= GPIO_NUM_MAX)
        {
            ESP_LOGE(kTag, "configure: INT pin is invalid or not configured");
            return false;
        }
        _sda = cfg.sda;
        _scl = cfg.scl;
        _intr = cfg.intr;
        _i2cAddr = cfg.i2cAddr ? cfg.i2cAddr : 0x38;
        _freqHz = cfg.freqHz ? cfg.freqHz : 400000U;
        _width = cfg.width;
        _height = cfg.height;
        _rotation = cfg.rotation & 3u;
        _i2cPort = 0;
        return true;
    }

    bool Touch::begin() noexcept
    {
        if (_ready)
            return true;

        if (_intr < 0 || _intr >= GPIO_NUM_MAX ||
            _sda < 0 || _sda >= GPIO_NUM_MAX ||
            _scl < 0 || _scl >= GPIO_NUM_MAX)
        {
            ESP_LOGW(kTag, "begin: pins not fully configured or out of physical range. Touch is disabled.");
            return false;
        }

        const i2c_port_t port = static_cast<i2c_port_t>(_i2cPort);

        i2c_config_t conf{};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = static_cast<gpio_num_t>(_sda);
        conf.scl_io_num = static_cast<gpio_num_t>(_scl);
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = _freqHz;
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
        conf.clk_flags = 0;
#endif

        esp_err_t err = i2c_param_config(port, &conf);
        if (err != ESP_OK)
        {
            ESP_LOGE(kTag, "i2c_param_config failed: %s", esp_err_to_name(err));
            return false;
        }

        err = i2c_driver_install(port, conf.mode, 0, 0, 0);
        if (err == ESP_ERR_INVALID_STATE)
        {
            _i2cBusOwned = false;
        }
        else if (err != ESP_OK)
        {
            ESP_LOGE(kTag, "i2c_driver_install failed: %s", esp_err_to_name(err));
            return false;
        }
        else
        {
            _i2cBusOwned = true;
        }

        uint8_t devMode = 0xFF;
        if (!i2cReadRegs(RegDevMode, &devMode, 1) || (devMode & 0x70) != 0)
        {
            if (!i2cReadRegs(RegDevMode, &devMode, 1))
            {
                ESP_LOGE(kTag, "no ACK at touch addr 0x%02X", _i2cAddr);
                if (_i2cBusOwned)
                {
                    i2c_driver_delete(port);
                    _i2cBusOwned = false;
                }
                return false;
            }
        }

        (void)i2cWriteReg(RegGMode, 0x00);

        for (uint8_t i = 0; i < MaxPoints; ++i)
            _slots[i] = Slot{};
        _reported = 0;
        _touchedFlag = false;

        const gpio_num_t intPin = static_cast<gpio_num_t>(_intr);
        gpio_config_t io{};
        io.pin_bit_mask = 1ULL << static_cast<uint8_t>(_intr);
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.intr_type = GPIO_INTR_NEGEDGE;
        if (gpio_config(&io) != ESP_OK)
        {
            ESP_LOGE(kTag, "gpio_config failed for INT pin %d", _intr);
            return false;
        }

        {
            const esp_err_t isrErr = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
            if (isrErr != ESP_OK && isrErr != ESP_ERR_INVALID_STATE)
            {
                ESP_LOGE(kTag, "gpio_install_isr_service failed: %s", esp_err_to_name(isrErr));
                return false;
            }
        }

        g_instance = this;
        if (gpio_isr_handler_add(intPin, touchIsrHandler, nullptr) != ESP_OK)
        {
            ESP_LOGE(kTag, "gpio_isr_handler_add failed for pin %d", _intr);
            g_instance = nullptr;
            return false;
        }
        _isrAttached = true;

        _ready = true;
        ESP_LOGI(kTag, "ready (INT pin %d, addr 0x%02X, %s)",
                 _intr, _i2cAddr, _i2cBusOwned ? "owned bus" : "shared bus");
        return true;
    }

    void Touch::end() noexcept
    {
        if (_isrAttached)
        {
            gpio_isr_handler_remove(static_cast<gpio_num_t>(_intr));
            _isrAttached = false;
        }
        if (g_instance == this)
            g_instance = nullptr;

        if (_ready)
        {
            if (_i2cBusOwned)
            {
                i2c_driver_delete(static_cast<i2c_port_t>(_i2cPort));
                _i2cBusOwned = false;
            }
            _ready = false;
        }
        _reported = 0;
        _touchedFlag = false;
        for (uint8_t i = 0; i < MaxPoints; ++i)
            _slots[i] = Slot{};
    }

    bool Touch::i2cWriteReg(uint8_t reg, uint8_t value) noexcept
    {
        const uint8_t data[] = {reg, value};
        esp_err_t err = i2c_master_write_to_device(
            static_cast<i2c_port_t>(_i2cPort),
            _i2cAddr,
            data,
            sizeof(data),
            pdMS_TO_TICKS(10));
        return err == ESP_OK;
    }

    bool Touch::i2cReadRegs(uint8_t reg, uint8_t *buf, size_t len) noexcept
    {
        esp_err_t err = i2c_master_write_read_device(
            static_cast<i2c_port_t>(_i2cPort),
            _i2cAddr,
            &reg,
            1,
            buf,
            len,
            pdMS_TO_TICKS(10));
        return err == ESP_OK;
    }

    Touch::Slot *Touch::findSlotById(uint8_t id) noexcept
    {
        for (uint8_t i = 0; i < MaxPoints; ++i)
            if (_slots[i].id == id)
                return &_slots[i];
        return nullptr;
    }

    Touch::Slot *Touch::findFreeSlot() noexcept
    {
        for (uint8_t i = 0; i < MaxPoints; ++i)
            if (_slots[i].id == 0xFF)
                return &_slots[i];
        return nullptr;
    }

    void Touch::remap(uint16_t inX, uint16_t inY, uint16_t &outX, uint16_t &outY) const noexcept
    {
        switch (_rotation)
        {
        default:
        case 0:
            outX = inX;
            outY = inY;
            break;
        case 1:
            outX = inY;
            outY = static_cast<uint16_t>(_height - 1 - inX);
            break;
        case 2:
            outX = static_cast<uint16_t>(_width - 1 - inX);
            outY = static_cast<uint16_t>(_height - 1 - inY);
            break;
        case 3:
            outX = static_cast<uint16_t>(_width - 1 - inY);
            outY = static_cast<uint16_t>(_height - 1 - inX);
            break;
        }
        if (outX >= _width)
            outX = static_cast<uint16_t>(_width - 1);
        if (outY >= _height)
            outY = static_cast<uint16_t>(_height - 1);
    }

    void Touch::update() noexcept
    {
        if (!_ready)
            return;

        const bool flag = _touchedFlag;
        _touchedFlag = false;

        bool physicallyPressed = false;
        if (_intr >= 0 && _intr < GPIO_NUM_MAX)
        {
            physicallyPressed = (gpio_get_level(static_cast<gpio_num_t>(_intr)) == 0);
        }

        if (!flag && !physicallyPressed && _reported == 0)
        {
            return;
        }

        for (uint8_t i = 0; i < MaxPoints; ++i)
        {
            _slots[i].wasPresent = _slots[i].present;
            _slots[i].present = false;
        }

        uint8_t td = 0;
        if (!i2cReadRegs(RegTdStatus, &td, 1))
            return;

        uint8_t n = td & 0x0F;
        if (n == 0)
            return;
        if (n > MaxPoints)
            n = MaxPoints;

        uint8_t buf[11] = {};
        const size_t want = (n >= 2) ? (RegP2 - RegP1 + 6u) : 6u;
        if (!i2cReadRegs(RegP1, buf, want))
            return;

        auto decodePoint = [](const uint8_t *r, uint16_t &outX, uint16_t &outY, uint8_t &outId) noexcept
        {
            outX = (static_cast<uint16_t>(r[0] & 0x0F) << 8) | r[1];
            outY = (static_cast<uint16_t>(r[2] & 0x0F) << 8) | r[3];
            outId = (r[2] >> 4) & 0x0F;
        };

        for (uint8_t i = 0; i < n; ++i)
        {
            const uint8_t *pointBuf = (i == 0) ? buf : (buf + (RegP2 - RegP1));
            uint16_t rx = 0, ry = 0;
            uint8_t rid = 0;
            decodePoint(pointBuf, rx, ry, rid);

            Slot *s = findSlotById(rid);
            if (!s)
                s = findFreeSlot();
            if (!s)
                continue;

            const bool fresh = (s->id == 0xFF);
            if (fresh)
                s->id = rid;

            uint16_t mx = 0, my = 0;
            remap(rx, ry, mx, my);

            const bool moved = !fresh && (mx != s->x || my != s->y);

            if (fresh)
            {
                s->x = mx;
                s->y = my;
                s->state = pipcore::TouchState::Pressed;
            }
            else
            {
                const uint16_t prevX = s->x;
                const uint16_t prevY = s->y;

                const int32_t dx = std::abs(static_cast<int32_t>(mx) - static_cast<int32_t>(prevX));
                const int32_t dy = std::abs(static_cast<int32_t>(my) - static_cast<int32_t>(prevY));
                const int32_t dist = dx + dy;

                if (dist <= 2)
                {
                    mx = prevX;
                    my = prevY;
                }
                else if (dist <= 12)
                {
                    mx = static_cast<uint16_t>((mx + prevX) >> 1);
                    my = static_cast<uint16_t>((my + prevY) >> 1);
                }

                s->x = mx;
                s->y = my;

                if (moved && (mx != prevX || my != prevY))
                {
                    s->state = pipcore::TouchState::Moved;
                }
                else
                {
                    s->state = pipcore::TouchState::Held;
                }
            }
            s->present = true;
        }

        uint8_t reported = 0;
        for (uint8_t i = 0; i < MaxPoints; ++i)
        {
            Slot &s = _slots[i];
            if (!s.present)
            {
                if (s.wasPresent)
                    s.state = pipcore::TouchState::Released;
                else if (s.state == pipcore::TouchState::Released)
                    s.id = 0xFF;
            }
            if (s.state != pipcore::TouchState::Released && s.id != 0xFF)
                ++reported;
        }
        _reported = reported;
    }

    pipcore::TouchPoint Touch::point(uint8_t index) const noexcept
    {
        if (index >= MaxPoints)
            return {};

        uint8_t out = 0;
        for (uint8_t i = 0; i < MaxPoints; ++i)
        {
            const Slot &s = _slots[i];
            if (s.id == 0xFF || s.state == pipcore::TouchState::Released)
                continue;
            if (out == index)
            {
                pipcore::TouchPoint p;
                p.x = s.x;
                p.y = s.y;
                p.id = s.id;
                p.state = s.state;
                return p;
            }
            ++out;
        }
        return {};
    }
}

#endif