#pragma once

#include "Core/Color.hpp"
#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"

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

  namespace SunDisc
  {
    inline constexpr float kRadiusScale = 0.6f;

    inline constexpr int kLutSize = 129;
    inline constexpr int kLutMax = kLutSize - 1;

    inline constexpr uint8_t kProfile[kLutSize] = {
        255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250,
        250, 249, 248, 246, 245, 244, 243, 241, 240, 238, 237, 235,
        234, 232, 230, 228, 226, 224, 222, 220, 218, 215, 213, 211,
        208, 206, 203, 201, 198, 196, 193, 190, 188, 185, 182, 179,
        176, 173, 170, 167, 165, 162, 158, 155, 152, 149, 146, 143,
        140, 137, 134, 131, 128, 124, 121, 118, 115, 112, 109, 106,
        103, 100, 97, 93, 90, 88, 85, 82, 79, 76, 73, 70,
        67, 65, 62, 59, 57, 54, 52, 49, 47, 44, 42, 40,
        37, 35, 33, 31, 29, 27, 25, 23, 21, 20, 18, 17,
        15, 14, 12, 11, 10, 9, 7, 6, 5, 5, 4, 3,
        2, 2, 1, 1, 1, 0, 0, 0, 0};

    PIP3D_HOT inline void drawRadialProfile(uint16_t *__restrict__ fb,
                                            float cxf, float cyf, float r,
                                            const uint8_t *lut, uint16_t color,
                                            uint8_t alpha,
                                            int bandTop, int bandBottom)
    {
      const int cx = static_cast<int>(cxf + 0.5f);
      const int cy = static_cast<int>(cyf + 0.5f);
      const int ri = static_cast<int>(r + 0.5f);
      const float invR2 = static_cast<float>(kLutMax) /
                          (static_cast<float>(ri) * static_cast<float>(ri));

      int y0 = cy - ri;
      int y1 = cy + ri;
      if (y0 < bandTop)
        y0 = bandTop;
      if (y1 > bandBottom - 1)
        y1 = bandBottom - 1;
      int x0 = cx - ri;
      int x1 = cx + ri;
      if (x0 < 0)
        x0 = 0;
      if (x1 > SCREEN_WIDTH - 1)
        x1 = SCREEN_WIDTH - 1;
      if (x0 > x1 || y0 > y1)
        return;

      const uint32_t sr = static_cast<uint32_t>((color >> 11) & 0x1Fu);
      const uint32_t sg = static_cast<uint32_t>((color >> 5) & 0x3Fu);
      const uint32_t sb = static_cast<uint32_t>(color & 0x1Fu);
      const uint32_t aSrc = alpha;

      for (int y = y0; y <= y1; ++y)
      {
        const float dy = static_cast<float>(y - cy);
        const float dy2 = dy * dy;
        size_t idx = static_cast<size_t>(y - bandTop) * SCREEN_WIDTH + x0;
        for (int x = x0; x <= x1; ++x, ++idx)
        {
          const float dx = static_cast<float>(x - cx);
          const float q = (dx * dx + dy2) * invR2;
          if (q >= static_cast<float>(kLutMax))
            continue;

          const int q8 = static_cast<int>(q * 256.0f);
          const uint32_t fr = static_cast<uint32_t>(q8) & 255u;
          const uint32_t tap = (static_cast<uint32_t>(lut[q8 >> 8]) * (256u - fr) +
                                static_cast<uint32_t>(lut[(q8 >> 8) + 1]) * fr) >>
                               8;
          const uint32_t pa = tap * aSrc;
          if (pa < 1024u)
            continue;

          const uint32_t dst = fb[idx];
          uint32_t r = ((dst >> 11) & 0x1Fu) + ((sr * pa + 32768u) >> 16);
          uint32_t g = ((dst >> 5) & 0x3Fu) + ((sg * pa + 32768u) >> 16);
          uint32_t b = (dst & 0x1Fu) + ((sb * pa + 32768u) >> 16);
          if (r > 31u)
            r = 31u;
          if (g > 63u)
            g = 63u;
          if (b > 31u)
            b = 31u;
          fb[idx] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
        }
      }
    }

    PIP3D_HOT inline void draw(uint16_t *__restrict__ fb,
                               const Vector3 &sunScreen, float sunDiskRadius,
                               int bandTop, int bandBottom,
                               const Color &tint, float brightness)
    {
      const float a = clamp(0.9f * brightness, 0.0f, 1.0f);
      const uint8_t aByte = static_cast<uint8_t>(a * 255.0f + 0.5f);
      if (aByte == 0)
        return;

      const float discR = sunDiskRadius * kRadiusScale;
      drawRadialProfile(fb, sunScreen.x, sunScreen.y, discR,
                        kProfile, tint.rgb565, aByte, bandTop, bandBottom);
    }
  }

  struct alignas(8) Sky
  {
  private:
    static constexpr SkyPreset kDay{
        .top = Color::rgb(60, 140, 255),
        .horizon = Color::rgb(210, 230, 255),
        .ground = Color::rgb(110, 120, 140),
        .tempK = 5500};

    static constexpr SkyPreset kSunset{
        .top = Color::rgb(250, 130, 90),
        .horizon = Color::rgb(255, 210, 140),
        .ground = Color::rgb(80, 55, 100),
        .tempK = 2500};

    static constexpr SkyPreset kNight{
        .top = Color::rgb(15, 40, 100),
        .horizon = Color::rgb(40, 90, 160),
        .ground = Color::rgb(10, 25, 60),
        .tempK = 8000};

    static constexpr SkyPreset kDawn{
        .top = Color::rgb(120, 155, 230),
        .horizon = Color::rgb(255, 195, 170),
        .ground = Color::rgb(90, 95, 120),
        .tempK = 4000};

    static constexpr SkyPreset kOvercast{
        .top = Color::rgb(140, 160, 175),
        .horizon = Color::rgb(195, 205, 215),
        .ground = Color::rgb(95, 106, 106),
        .tempK = 6500};

    static constexpr SkyPreset kMidday{
        .top = Color::rgb(180, 220, 255),
        .horizon = Color::rgb(255, 255, 240),
        .ground = Color::rgb(130, 140, 120),
        .tempK = 6800};

    static constexpr SkyPreset kStorm{
        .top = Color::rgb(50, 55, 65),
        .horizon = Color::rgb(80, 85, 95),
        .ground = Color::rgb(30, 30, 35),
        .tempK = 7200};

    static constexpr SkyPreset kSandstorm{
        .top = Color::rgb(200, 130, 60),
        .horizon = Color::rgb(220, 170, 90),
        .ground = Color::rgb(140, 90, 40),
        .tempK = 3800};

    static constexpr SkyPreset kSpace{
        .top = Color::rgb(5, 0, 20),
        .horizon = Color::rgb(20, 10, 50),
        .ground = Color::rgb(0, 0, 10),
        .tempK = 9500};

    static constexpr SkyPreset kAlien{
        .top = Color::rgb(10, 40, 15),
        .horizon = Color::rgb(40, 90, 30),
        .ground = Color::rgb(5, 20, 8),
        .tempK = 5000};

    static constexpr SkyPreset kPresets[] = {
        kDay, kSunset, kNight, kDawn, kOvercast,
        kMidday, kStorm, kSandstorm, kSpace, kAlien};
    static_assert(sizeof(kPresets) / sizeof(kPresets[0]) == static_cast<size_t>(CUSTOM),
                  "every SkyType except CUSTOM must have a preset row");

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

    static PIP3D_ALWAYS_INLINE inline uint32_t smoothstep8(uint32_t t) noexcept
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

    PIP3D_ALWAYS_INLINE inline void getColorAtY888(int16_t y, uint8_t &outR, uint8_t &outG, uint8_t &outB) const noexcept
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

    PIP3D_ALWAYS_INLINE inline Color getColorAtY(int16_t y) const noexcept
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
