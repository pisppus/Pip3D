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

  namespace detail
  {
    alignas(32) inline constexpr uint8_t blueNoiseLut32x32[1024] = {
        55, 173, 79, 224, 65, 140, 34, 195, 158, 54, 17, 206, 62, 144, 240, 190,
        72, 40, 214, 54, 192, 5, 146, 60, 82, 185, 3, 138, 169, 25, 83, 245,
        143, 30, 192, 245, 12, 112, 236, 101, 2, 244, 113, 165, 225, 118, 47, 20,
        176, 251, 142, 84, 117, 160, 254, 177, 26, 238, 121, 72, 193, 213, 153, 13,
        111, 207, 99, 132, 155, 182, 85, 127, 219, 147, 42, 97, 184, 5, 83, 208,
        108, 61, 125, 228, 21, 100, 39, 90, 114, 53, 218, 41, 252, 129, 61, 234,
        166, 69, 4, 48, 215, 21, 204, 57, 73, 176, 200, 30, 249, 155, 133, 233,
        163, 9, 197, 32, 183, 220, 205, 137, 232, 167, 94, 144, 9, 105, 181, 44,
        220, 138, 252, 119, 76, 168, 39, 250, 10, 136, 84, 123, 54, 69, 194, 37,
        95, 147, 241, 73, 153, 48, 68, 7, 194, 17, 207, 161, 31, 76, 201, 90,
        151, 36, 176, 196, 230, 94, 149, 109, 184, 226, 20, 236, 215, 105, 175, 22,
        219, 52, 87, 111, 174, 128, 248, 149, 78, 125, 63, 184, 227, 242, 118, 22,
        239, 83, 104, 26, 55, 134, 238, 29, 159, 95, 63, 167, 149, 7, 78, 255,
        119, 166, 212, 1, 233, 19, 105, 186, 37, 244, 110, 86, 135, 56, 173, 11,
        186, 62, 223, 145, 12, 189, 68, 202, 47, 211, 114, 192, 41, 127, 203, 141,
        65, 189, 40, 135, 198, 61, 89, 222, 158, 24, 216, 45, 1, 157, 213, 130,
        43, 200, 113, 161, 213, 122, 87, 0, 130, 248, 77, 13, 241, 92, 229, 30,
        102, 13, 244, 77, 160, 33, 209, 119, 55, 176, 143, 190, 255, 103, 71, 93,
        165, 23, 242, 74, 45, 254, 170, 226, 155, 36, 142, 179, 60, 158, 48, 182,
        223, 154, 124, 98, 178, 250, 140, 5, 231, 96, 68, 19, 116, 204, 32, 227,
        121, 141, 3, 98, 180, 31, 108, 58, 196, 97, 24, 222, 107, 198, 2, 116,
        70, 207, 52, 230, 22, 109, 47, 80, 165, 132, 199, 235, 170, 52, 148, 247,
        58, 85, 209, 128, 232, 151, 15, 79, 182, 120, 238, 168, 134, 81, 248, 146,
        173, 16, 88, 195, 65, 150, 183, 205, 242, 11, 41, 89, 126, 80, 8, 183,
        18, 171, 192, 49, 66, 201, 137, 246, 218, 51, 71, 15, 43, 214, 29, 95,
        239, 38, 139, 165, 7, 225, 124, 30, 59, 112, 221, 154, 28, 197, 217, 106,
        70, 241, 219, 27, 162, 115, 88, 38, 4, 148, 204, 92, 189, 154, 63, 130,
        217, 188, 111, 254, 208, 101, 86, 191, 144, 75, 180, 249, 65, 137, 233, 157,
        34, 91, 145, 104, 8, 240, 211, 175, 129, 164, 109, 253, 123, 230, 171, 6,
        50, 79, 27, 127, 73, 43, 19, 246, 161, 211, 103, 17, 172, 96, 46, 117,
        185, 133, 42, 229, 187, 73, 55, 101, 27, 235, 59, 12, 35, 75, 113, 199,
        101, 163, 237, 57, 152, 174, 234, 134, 1, 37, 53, 123, 193, 6, 208, 253,
        11, 202, 78, 122, 168, 18, 141, 194, 221, 80, 187, 142, 177, 210, 18, 249,
        144, 221, 180, 12, 201, 215, 106, 60, 91, 226, 200, 236, 150, 85, 61, 164,
        24, 151, 246, 51, 214, 153, 251, 118, 45, 157, 98, 224, 53, 88, 134, 62,
        42, 23, 116, 94, 140, 33, 121, 188, 169, 141, 113, 76, 33, 131, 227, 110,
        99, 172, 64, 0, 97, 34, 85, 66, 20, 208, 3, 125, 243, 164, 186, 235,
        156, 82, 191, 67, 248, 49, 80, 10, 253, 68, 23, 162, 244, 179, 49, 215,
        239, 126, 194, 225, 112, 182, 234, 131, 174, 240, 72, 39, 109, 29, 8, 100,
        122, 207, 231, 4, 166, 224, 198, 153, 217, 44, 183, 212, 4, 93, 143, 72,
        28, 38, 82, 136, 206, 161, 9, 196, 106, 139, 167, 204, 150, 195, 218, 70,
        172, 35, 132, 103, 146, 27, 89, 128, 16, 107, 96, 57, 119, 201, 15, 187,
        115, 156, 243, 14, 46, 74, 57, 219, 28, 51, 90, 250, 59, 81, 140, 47,
        255, 17, 58, 181, 243, 114, 56, 178, 239, 139, 228, 156, 251, 40, 167, 232,
        205, 59, 179, 92, 147, 253, 124, 99, 237, 186, 11, 120, 19, 181, 229, 112,
        198, 160, 220, 76, 42, 210, 160, 71, 202, 31, 78, 190, 130, 67, 86, 138,
        3, 102, 220, 25, 191, 170, 36, 143, 81, 152, 209, 224, 133, 35, 93, 2,
        145, 87, 124, 193, 97, 22, 228, 1, 120, 51, 171, 8, 26, 210, 108, 48,
        250, 162, 69, 120, 231, 107, 213, 2, 177, 43, 67, 102, 159, 238, 171, 206,
        64, 29, 233, 10, 151, 135, 185, 87, 247, 147, 223, 91, 241, 152, 225, 175,
        33, 131, 202, 49, 84, 18, 64, 197, 245, 114, 21, 193, 52, 74, 118, 44,
        243, 105, 173, 50, 252, 110, 63, 166, 41, 102, 199, 62, 117, 184, 15, 77,
        237, 187, 9, 152, 247, 136, 158, 91, 128, 232, 169, 137, 251, 10, 216, 154,
        188, 131, 211, 71, 200, 34, 236, 216, 129, 13, 179, 136, 32, 54, 99, 146,
        44, 90, 110, 216, 178, 37, 226, 53, 14, 77, 212, 31, 86, 180, 100, 23,
        82, 14, 162, 93, 122, 6, 81, 156, 24, 209, 75, 255, 163, 218, 196, 121,
        210, 169, 58, 28, 70, 100, 206, 188, 164, 107, 60, 150, 203, 126, 235, 142,
        56, 249, 38, 222, 148, 178, 195, 56, 115, 230, 45, 108, 7, 84, 234, 21,
        74, 252, 148, 231, 132, 117, 6, 145, 254, 39, 222, 5, 111, 46, 67, 197,
        228, 116, 181, 66, 25, 245, 98, 139, 172, 89, 190, 149, 127, 177, 64, 138,
        0, 104, 199, 16, 185, 242, 83, 26, 123, 95, 191, 175, 247, 159, 32, 170,
        0, 88, 203, 133, 106, 46, 227, 14, 35, 246, 66, 20, 240, 205, 36, 159,
        221, 125, 40, 94, 163, 50, 214, 174, 69, 229, 135, 79, 25, 92, 217, 129,
        103, 155, 16, 237, 168, 75, 212, 126, 203, 157, 104, 223, 50, 96, 115, 189};
  }

  struct alignas(8) Sky
  {
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
      static constexpr float temps[] = {5500.0f, 2500.0f, 8000.0f, 4000.0f, 6500.0f,
                                        6800.0f, 7200.0f, 3800.0f, 9500.0f, 5000.0f};
      return (static_cast<uint8_t>(type) < 10) ? temps[type] : 5500.0f;
    }

    Color getLightColor() const { return Color::temp(getLightTemp()); }

    __attribute__((always_inline)) inline Color getColorAtXY(int16_t x, int16_t y, int16_t h) const
    {
      if (unlikely(!enabled))
        return Color::BLACK;
      if (unlikely(y <= 0))
        return top;
      if (unlikely(y >= h))
        return ground;

      const uint8_t thr = detail::blueNoiseLut32x32[((static_cast<uint16_t>(y) & 31u) << 5) |
                                                    (static_cast<uint16_t>(x) & 31u)];

      const uint32_t T = (static_cast<uint32_t>(y) << 8) / static_cast<uint32_t>(h);

      if (likely(T < 166))
      {
        const uint32_t st = (T * 395u) >> 8;
        const uint32_t s = (st * st * (768u - 2u * st)) >> 16;
        return lerp(top, horizon, s, thr);
      }
      else
      {
        uint32_t gt = ((T - 166u) * 728u) >> 8;
        if (gt > 256u)
          gt = 256u;
        const uint32_t s = (gt * gt * (768u - 2u * gt)) >> 16;
        return lerp(horizon, ground, s, thr);
      }
    }

    __attribute__((always_inline)) inline Color getColorAtY(int16_t y, int16_t h) const
    {
      return getColorAtXY(0, y, h);
    }

  private:
    __attribute__((always_inline)) inline Color lerp(Color c1, Color c2, uint32_t s, uint8_t thr) const
    {
      if (unlikely(s == 0))
        return c1;
      if (unlikely(s >= 256))
        return c2;

      const uint32_t alpha = (s + (thr >> 5)) >> 3;
      const uint32_t ia = 32u - alpha;
      const uint32_t c1_raw = c1.rgb565;
      const uint32_t c2_raw = c2.rgb565;

      const uint32_t rb1 = c1_raw & 0xF81Fu, rb2 = c2_raw & 0xF81Fu;
      const uint32_t g1 = c1_raw & 0x07E0u, g2 = c2_raw & 0x07E0u;

      const uint32_t rb = ((rb1 * ia + rb2 * alpha) >> 5) & 0xF81Fu;
      const uint32_t g = ((g1 * ia + g2 * alpha) >> 5) & 0x07E0u;

      return Color(static_cast<uint16_t>(rb | g));
    }
  };

  using Skybox = Sky;
}