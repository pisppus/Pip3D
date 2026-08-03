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

    static constexpr uint16_t temps[11] = {
        5500, 2500, 8000, 4000, 6500, 6800, 7200, 3800, 9500, 5000, 5500};

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
      if (static_cast<uint8_t>(t) < 10u)
      {
        top = presets[t][0];
        horizon = presets[t][1];
        ground = presets[t][2];
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

      return static_cast<float>(temps[static_cast<uint8_t>(type)]);
    }

    __attribute__((always_inline)) inline Color getColorAtY(int16_t y) const noexcept
    {
      if (unlikely(!enabled))
        return Color::BLACK;
      if (unlikely(y <= 0))
        return top;
      if (unlikely(y >= static_cast<int16_t>(SCREEN_HEIGHT)))
        return ground;

      const uint32_t T = (static_cast<uint32_t>(y) << 8) / static_cast<uint32_t>(SCREEN_HEIGHT);

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

        const uint32_t s = smoothstep8(((T - 166u) * 728u) >> 8);
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
