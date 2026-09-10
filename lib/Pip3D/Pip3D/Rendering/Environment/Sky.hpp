#pragma once

#include "Core/Color.hpp"
#include "Core/Platform.hpp"

namespace pip3D
{
  enum SkyType : uint8_t
  {
    DAY,
    SUNSET,
    NIGHT,
    DAWN,
    OVERCAST,
    MIDDAY,
    STORM,
    SANDSTORM,
    SPACE,
    ALIEN,
    CUSTOM
  };

  using SkyboxType = SkyType;

  struct SkyPreset
  {
    Color top;
    Color horizon;
    Color ground;
    uint16_t tempK;
  };

  struct alignas(8) Sky
  {
  private:
    static constexpr SkyPreset kPresets[] = {
        {Color::rgb(60, 140, 255), Color::rgb(210, 230, 255), Color::rgb(110, 120, 140), 5500},
        {Color::rgb(250, 130, 90), Color::rgb(255, 210, 140), Color::rgb(80, 55, 100), 2500},
        {Color::rgb(15, 40, 100), Color::rgb(40, 90, 160), Color::rgb(10, 25, 60), 8000},
        {Color::rgb(120, 155, 230), Color::rgb(255, 195, 170), Color::rgb(90, 95, 120), 4000},
        {Color::rgb(140, 160, 175), Color::rgb(195, 205, 215), Color::rgb(95, 106, 106), 6500},
        {Color::rgb(180, 220, 255), Color::rgb(255, 255, 240), Color::rgb(130, 140, 120), 6800},
        {Color::rgb(50, 55, 65), Color::rgb(80, 85, 95), Color::rgb(30, 30, 35), 7200},
        {Color::rgb(200, 130, 60), Color::rgb(220, 170, 90), Color::rgb(140, 90, 40), 3800},
        {Color::rgb(5, 0, 20), Color::rgb(20, 10, 50), Color::rgb(0, 0, 10), 9500},
        {Color::rgb(10, 40, 15), Color::rgb(40, 90, 30), Color::rgb(5, 20, 8), 5000}};

    static constexpr size_t kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);
    static constexpr uint16_t kCustomTempK = 5500;

    static constexpr uint16_t kRefH = 240;

    PIP3D_FORCE_INLINE static uint32_t calcT(int16_t y) noexcept
    {
      const uint32_t yScaled = (uint32_t)y * kRefH / SCREEN_HEIGHT;
      return (yScaled * 205u) >> 8;
    }

    PIP3D_FORCE_INLINE static uint32_t unpackR8(Color c) noexcept
    {
      uint32_t v = c.r5();
      return (v << 3) | (v >> 2);
    }
    PIP3D_FORCE_INLINE static uint32_t unpackG8(Color c) noexcept
    {
      uint32_t v = c.g6();
      return (v << 2) | (v >> 4);
    }
    PIP3D_FORCE_INLINE static uint32_t unpackB8(Color c) noexcept
    {
      uint32_t v = c.b5();
      return (v << 3) | (v >> 2);
    }

    static __attribute__((always_inline)) inline uint32_t smoothstep8(uint32_t t) noexcept
    {
      return (t * t * (768u - 2u * t)) >> 16;
    }

  public:
    Color top = Color::BLACK;
    Color horizon = Color::BLACK;
    Color ground = Color::BLACK;
    SkyType type = DAY;
    bool enabled = true;

    Sky() { setPreset(DAY); }
    Sky(SkyType t) { setPreset(t); }
    Sky(Color t, Color h, Color g) : top(t), horizon(h), ground(g), type(CUSTOM) {}

    void setPreset(SkyType t) noexcept
    {
      type = t;
      const size_t i = static_cast<size_t>(t);
      if (i < kPresetCount)
      {
        top = kPresets[i].top;
        horizon = kPresets[i].horizon;
        ground = kPresets[i].ground;
      }
    }

    void setCustom(Color t, Color h, Color g) noexcept
    {
      type = CUSTOM;
      top = t;
      horizon = h;
      ground = g;
    }

    float getLightTemp() const noexcept
    {
      const size_t i = static_cast<size_t>(type);
      return static_cast<float>((i < kPresetCount) ? kPresets[i].tempK : kCustomTempK);
    }

    __attribute__((always_inline)) inline void getColorAtY888(int16_t y, uint8_t &outR, uint8_t &outG, uint8_t &outB) const noexcept
    {
      if (unlikely(!enabled))
      {
        outR = outG = outB = 0;
        return;
      }

      if (unlikely(y <= 0))
      {
        outR = static_cast<uint8_t>(unpackR8(top));
        outG = static_cast<uint8_t>(unpackG8(top));
        outB = static_cast<uint8_t>(unpackB8(top));
        return;
      }
      if (unlikely(y >= static_cast<int16_t>(SCREEN_HEIGHT)))
      {
        outR = static_cast<uint8_t>(unpackR8(ground));
        outG = static_cast<uint8_t>(unpackG8(ground));
        outB = static_cast<uint8_t>(unpackB8(ground));
        return;
      }

      const uint32_t T = calcT(y);

      uint32_t rA, gA, bA;
      uint32_t rB, gB, bB;
      uint32_t s;

      if (likely(T < 166u))
      {
        s = smoothstep8((T * 395u) >> 8);
        rA = unpackR8(top);
        gA = unpackG8(top);
        bA = unpackB8(top);
        rB = unpackR8(horizon);
        gB = unpackG8(horizon);
        bB = unpackB8(horizon);
      }
      else
      {
        s = smoothstep8(((T - 166u) * 395u) >> 8);
        rA = unpackR8(horizon);
        gA = unpackG8(horizon);
        bA = unpackB8(horizon);
        rB = unpackR8(ground);
        gB = unpackG8(ground);
        bB = unpackB8(ground);
      }

      if (s > 256u)
        s = 256u;
      const uint32_t invS = 256u - s;

      uint32_t r = (rA * invS + rB * s) >> 8;
      uint32_t g = (gA * invS + gB * s) >> 8;
      uint32_t b = (bA * invS + bB * s) >> 8;

      outR = static_cast<uint8_t>(r > 248u ? 248u : r);
      outG = static_cast<uint8_t>(g > 252u ? 252u : g);
      outB = static_cast<uint8_t>(b > 248u ? 248u : b);
    }

    __attribute__((always_inline)) inline Color getColorAtY(int16_t y) const noexcept
    {
      if (unlikely(!enabled))
        return Color::BLACK;
      if (unlikely(y <= 0))
        return top;
      if (unlikely(y >= static_cast<int16_t>(SCREEN_HEIGHT)))
        return ground;

      const uint32_t T = calcT(y);

      const uint32_t topV = top.rgb565;
      const uint32_t horV = horizon.rgb565;
      const uint32_t grdV = ground.rgb565;

      if (likely(T < 166u))
      {
        const uint32_t s = smoothstep8((T * 395u) >> 8);
        const uint32_t a = s >> 2;
        const uint32_t ia = 64u - a;
        const uint32_t rb = ((topV & 0xF81Fu) * ia + (horV & 0xF81Fu) * a) >> 6 & 0xF81Fu;
        const uint32_t g = ((topV & 0x07E0u) * ia + (horV & 0x07E0u) * a) >> 6 & 0x07E0u;
        return Color(static_cast<uint16_t>(rb | g));
      }
      else
      {
        const uint32_t s = smoothstep8(((T - 166u) * 395u) >> 8);
        const uint32_t a = s >> 2;
        const uint32_t ia = 64u - a;
        const uint32_t rb = ((horV & 0xF81Fu) * ia + (grdV & 0xF81Fu) * a) >> 6 & 0xF81Fu;
        const uint32_t g = ((horV & 0x07E0u) * ia + (grdV & 0x07E0u) * a) >> 6 & 0x07E0u;
        return Color(static_cast<uint16_t>(rb | g));
      }
    }
  };

  using Skybox = Sky;
}