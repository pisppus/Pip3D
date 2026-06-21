#pragma once
#include "Core/Color.hpp"

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

  inline constexpr SkyType SKYBOX_DAY = DAY;
  inline constexpr SkyType SKYBOX_SUNSET = SUNSET;
  inline constexpr SkyType SKYBOX_NIGHT = NIGHT;
  inline constexpr SkyType SKYBOX_DAWN = DAWN;
  inline constexpr SkyType SKYBOX_OVERCAST = OVERCAST;
  inline constexpr SkyType SKYBOX_MIDDAY = MIDDAY;
  inline constexpr SkyType SKYBOX_STORM = STORM;
  inline constexpr SkyType SKYBOX_SANDSTORM = SANDSTORM;
  inline constexpr SkyType SKYBOX_SPACE = SPACE;
  inline constexpr SkyType SKYBOX_ALIEN = ALIEN;
  inline constexpr SkyType SKYBOX_CUSTOM = CUSTOM;

  using SkyboxType = SkyType;

  struct alignas(8) Sky
  {
  private:
    static constexpr Color presets[][3] = {
        {Color::rgb(60, 140, 255), Color::rgb(210, 230, 255), Color::rgb(110, 120, 140)},
        {Color::rgb(250, 130, 90), Color::rgb(255, 210, 140), Color::rgb(80, 55, 100)},
        {Color::rgb(15, 40, 100), Color::rgb(40, 90, 160), Color::rgb(10, 25, 60)},
        {Color::rgb(120, 155, 230), Color::rgb(255, 195, 170), Color::rgb(90, 95, 120)},
        {Color::rgb(140, 160, 175), Color::rgb(195, 205, 215), Color::rgb(95, 106, 106)},
        {Color::rgb(180, 220, 255), Color::rgb(255, 255, 240), Color::rgb(130, 140, 120)},
        {Color::rgb(50, 55, 65), Color::rgb(80, 85, 95), Color::rgb(30, 30, 35)},
        {Color::rgb(200, 130, 60), Color::rgb(220, 170, 90), Color::rgb(140, 90, 40)},
        {Color::rgb(5, 0, 20), Color::rgb(20, 10, 50), Color::rgb(0, 0, 10)},
        {Color::rgb(10, 40, 15), Color::rgb(40, 90, 30), Color::rgb(5, 20, 8)}};

    static constexpr float temps[] = {5500.0f, 2500.0f, 8000.0f, 4000.0f, 6500.0f,
                                      6800.0f, 7200.0f, 3800.0f, 9500.0f, 5000.0f};

    static __attribute__((always_inline)) inline uint32_t smoothstep8(uint32_t t) noexcept
    {
      return (t * t * (768u - 2u * t)) >> 16;
    }

    __attribute__((always_inline)) inline Color lerpColor(Color c1, Color c2, uint32_t s) const
    {
      if (unlikely(s == 0))
        return c1;
      if (unlikely(s >= 256))
        return c2;

      const uint32_t alpha = s >> 3;
      const uint32_t ia = 32u - alpha;
      const uint32_t c1_raw = c1.rgb565;
      const uint32_t c2_raw = c2.rgb565;

      const uint32_t rb1 = c1_raw & 0xF81Fu, rb2 = c2_raw & 0xF81Fu;
      const uint32_t g1 = c1_raw & 0x07E0u, g2 = c2_raw & 0x07E0u;

      const uint32_t rb = ((rb1 * ia + rb2 * alpha) >> 5) & 0xF81Fu;
      const uint32_t g = ((g1 * ia + g2 * alpha) >> 5) & 0x07E0u;

      return Color(static_cast<uint16_t>(rb | g));
    }

  public:
    Color top = Color::BLACK;
    Color horizon = Color::BLACK;
    Color ground = Color::BLACK;
    SkyType type = DAY;
    bool enabled = true;

    Sky() { setPreset(DAY); }
    Sky(SkyType t) : type(t) { setPreset(t); }
    Sky(Color t, Color h, Color g) : top(t), horizon(h), ground(g), type(CUSTOM) {}

    void setPreset(SkyType t)
    {
      type = t;
      if (static_cast<uint8_t>(t) < 10)
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
      return (static_cast<uint8_t>(type) < 10) ? temps[type] : 5500.0f;
    }

    __attribute__((always_inline)) inline Color getColorAtY(int16_t y, int16_t h) const
    {
      if (unlikely(!enabled))
        return Color::BLACK;
      if (unlikely(y <= 0))
        return top;
      if (unlikely(y >= h))
        return ground;

      const uint32_t T = (static_cast<uint32_t>(y) << 8) / static_cast<uint32_t>(h);

      if (likely(T < 166))
      {
        const uint32_t s = smoothstep8((T * 395u) >> 8);
        return lerpColor(top, horizon, s);
      }
      else
      {
        uint32_t gt = ((T - 166u) * 728u) >> 8;
        if (gt > 256u)
          gt = 256u;
        const uint32_t s = smoothstep8(gt);
        return lerpColor(horizon, ground, s);
      }
    }
  };

  using Skybox = Sky;
}
