#pragma once
#include "Core/Color.hpp"

namespace pip3D
{
  enum SkyType
  {
    DAY,
    SUNSET,
    NIGHT,
    DAWN,
    OVERCAST,
    CUSTOM
  };

#define SKYBOX_DAY DAY
#define SKYBOX_SUNSET SUNSET
#define SKYBOX_NIGHT NIGHT
#define SKYBOX_DAWN DAWN
#define SKYBOX_OVERCAST OVERCAST
#define SKYBOX_CUSTOM CUSTOM

  struct alignas(8) Sky
  {
    SkyType type = DAY;
    Color top, horizon, ground;
    bool enabled = true;

    Sky() { setPreset(DAY); }
    Sky(SkyType t) : type(t) { setPreset(t); }
    Sky(Color t, Color h, Color g) : type(CUSTOM), top(t), horizon(h), ground(g) {}

    void setPreset(SkyType t)
    {
      static constexpr Color presets[][3] = {
          {Color::rgb(60, 140, 255), Color::rgb(210, 230, 255), Color::rgb(110, 120, 140)},
          {Color::rgb(250, 130, 90), Color::rgb(255, 210, 140), Color::rgb(80, 55, 100)},
          {Color::rgb(15, 40, 100), Color::rgb(40, 90, 160), Color::rgb(10, 25, 60)},
          {Color::rgb(120, 155, 230), Color::rgb(255, 195, 170), Color::rgb(90, 95, 120)},
          {Color::rgb(140, 160, 175), Color::rgb(195, 205, 215), Color::rgb(95, 106, 106)}};

      type = t;
      if (t != CUSTOM && t < 5)
      {
        top = presets[t][0];
        horizon = presets[t][1];
        ground = presets[t][2];
      }
    }

    void setCustom(Color t, Color h, Color g)
    {
      type = CUSTOM;
      top = t;
      horizon = h;
      ground = g;
    }

    float getLightTemp() const
    {
      static constexpr float temps[] = {5500, 2500, 8000, 4000, 6500};
      return (type < 5) ? temps[type] : 5500;
    }

    Color getLightColor() const { return Color::temp(getLightTemp()); }

    __attribute__((always_inline)) inline Color getColorAtY(int16_t y, int16_t h) const
    {
      if (unlikely(!enabled))
        return Color::BLACK;
      if (unlikely(y <= 0))
        return top;
      if (unlikely(y >= h))
        return ground;

      const float t = (float)y / h;

      if (likely(t < 0.65f))
      {
        const float st = t * 1.538f;
        const float s = st * st * (3 - 2 * st);
        return lerp(top, horizon, s);
      }
      else
      {
        const float gt = clamp((t - 0.65f) * 3.43f, 0.0f, 1.0f);
        const float s = gt * gt * (3 - 2 * gt);
        return lerp(horizon, ground, s);
      }
    }

  private:
    __attribute__((always_inline)) inline Color lerp(Color c1, Color c2, float t) const
    {
      if (unlikely(t <= 0))
        return c1;
      if (unlikely(t >= 1))
        return c2;

      const int r1 = (c1.rgb565 >> 11) & 0x1F, g1 = (c1.rgb565 >> 5) & 0x3F, b1 = c1.rgb565 & 0x1F;
      const int r2 = (c2.rgb565 >> 11) & 0x1F, g2 = (c2.rgb565 >> 5) & 0x3F, b2 = c2.rgb565 & 0x1F;

      const uint16_t r = r1 + (r2 - r1) * t;
      const uint16_t g = g1 + (g2 - g1) * t;
      const uint16_t b = b1 + (b2 - b1) * t;

      return Color((r << 11) | (g << 5) | b);
    }
  };

  using Skybox = Sky;
  using SkyboxType = SkyType;
}