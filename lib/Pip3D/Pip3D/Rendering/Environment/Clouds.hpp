#pragma once

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Math/Algebra.hpp"
#include "CloudsData.hpp"

namespace pip3D
{
    struct alignas(16) CloudLayer
    {
    private:
        alignas(16) mutable uint16_t shadeLut[256];
        float cloudHeight;
        float driftX;
        float driftZ;
        mutable float cachedTanHalfVfov_;
        mutable float cachedTanHalfHfov_;
        mutable float cachedUvScale_;
        float cachedAlphaMul32_;
        Color cloudColor;

        float cloudAlpha;
        float cloudScale;
        float driftSpeedX;
        float driftSpeedZ;
        mutable float cachedVfov_;
        mutable float cachedHfov_;
        bool enabled;
        mutable bool lutDirty;

        static PIP3D_FORCE_INLINE PIP3D_HOT uint16_t
        blend565(uint32_t dst, uint32_t src, uint32_t a5) noexcept
        {
            const uint32_t invA = 32u - a5;
            const uint32_t rb = (((dst & 0xF81Fu) * invA + (src & 0xF81Fu) * a5) >> 5) & 0xF81Fu;
            const uint32_t g = (((dst & 0x07E0u) * invA + (src & 0x07E0u) * a5) >> 5) & 0x07E0u;
            return static_cast<uint16_t>(rb | g);
        }

        PIP3D_FORCE_INLINE IRAM_ATTR void buildLuts() const noexcept
        {
            const uint32_t r5 = cloudColor.r5();
            const uint32_t g6 = cloudColor.g6();
            const uint32_t b5 = cloudColor.b5();

            for (uint32_t s = 0; s < 256u; ++s)
            {
                const uint32_t shade = 184u + (s * 90u) / 255u;
                uint32_t r = (r5 * shade + 128u) >> 8;
                uint32_t g = (g6 * shade + 128u) >> 8;
                uint32_t b = (b5 * shade + 128u) >> 8;
                if (r > 31u)
                    r = 31u;
                if (g > 63u)
                    g = 63u;
                if (b > 31u)
                    b = 31u;
                shadeLut[s] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
            }
            lutDirty = false;
        }

        PIP3D_FORCE_INLINE void prepareForFrame(float vfovRad, float hfovRad) const noexcept
        {
            if (unlikely(lutDirty))
                buildLuts();
            if (unlikely(vfovRad != cachedVfov_))
            {
                cachedVfov_ = vfovRad;
                cachedTanHalfVfov_ = tanf(vfovRad * 0.5f);
            }
            if (unlikely(hfovRad != cachedHfov_))
            {
                cachedHfov_ = hfovRad;
                cachedTanHalfHfov_ = tanf(hfovRad * 0.5f);
            }
            if (unlikely(cachedUvScale_ == 0.0f))
                cachedUvScale_ = static_cast<float>(CLOUDS_PANO_W) * FastMath::fastReciprocal(cloudScale);
        }

        PIP3D_FORCE_INLINE void wrapDrift() noexcept
        {
            const float period = static_cast<float>(CLOUDS_PANO_W) * cloudScale;
            const float halfP = period * 0.5f;

            if (driftX > halfP)
                driftX -= period;
            else if (driftX < -halfP)
                driftX += period;
            if (driftZ > halfP)
                driftZ -= period;
            else if (driftZ < -halfP)
                driftZ += period;
        }

    public:
        CloudLayer() noexcept
            : shadeLut{},
              cloudHeight(800.0f),
              driftX(0.0f), driftZ(0.0f),
              cachedTanHalfVfov_(-1.0f),
              cachedTanHalfHfov_(-1.0f),
              cachedUvScale_(0.0f),
              cachedAlphaMul32_(32.0f),
              cloudColor(Color(CLOUDS_DEFAULT_COLOR)),
              cloudAlpha(1.0f),
              cloudScale(2048.0f),
              driftSpeedX(0.0f), driftSpeedZ(0.0f),
              cachedVfov_(-1.0f),
              cachedHfov_(-1.0f),
              enabled(false),
              lutDirty(true)
        {
        }

        CloudLayer(const CloudLayer &) = delete;
        CloudLayer &operator=(const CloudLayer &) = delete;

        PIP3D_FORCE_INLINE bool isReady() const noexcept { return enabled; }
        PIP3D_FORCE_INLINE void setEnabled(bool e) noexcept { enabled = e; }

        void reset() noexcept
        {
            cloudColor = Color(CLOUDS_DEFAULT_COLOR);
            cloudAlpha = 1.0f;
            lutDirty = true;
        }

        void setCloudColor(Color c) noexcept
        {
            if (c.rgb565 != cloudColor.rgb565)
            {
                cloudColor = c;
                lutDirty = true;
            }
        }

        void setCloudAlpha(float a) noexcept
        {
            const float clamped = (a < 0.0f) ? 0.0f : (a > 1.0f) ? 1.0f
                                                                 : a;
            cloudAlpha = clamped;
            cachedAlphaMul32_ = clamped * 32.0f;
        }

        void setCloudHeight(float metersAboveCam) noexcept
        {
            cloudHeight = (metersAboveCam < 1.0f) ? 1.0f : metersAboveCam;
        }

        void setCloudScale(float metersPerTile) noexcept
        {
            cloudScale = (metersPerTile < 1.0f) ? 1.0f : metersPerTile;
            cachedUvScale_ = 0.0f;
        }

        void setDriftAngle(float angleDeg, float speedMps) noexcept
        {
            float s, c;
            FastMath::fastSinCos(angleDeg * kDegToRad, s, c);
            driftSpeedX = c * speedMps;
            driftSpeedZ = s * speedMps;
        }

        void update(float dtSeconds) noexcept
        {
            driftX += driftSpeedX * dtSeconds;
            driftZ += driftSpeedZ * dtSeconds;
            wrapDrift();
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        PIP3D_FORCE_INLINE PIP3D_HOT IRAM_ATTR void
        drawClouds(uint16_t *__restrict__ buf,
                   int16_t bandOffY,
                   const Vector3 &camPos,
                   const Vector3 &camFwd,
                   const Vector3 &camRight,
                   const Vector3 &rawCamUp,
                   float vfovRad,
                   float hfovRad) const noexcept
        {
            drawCloudsImpl<WIDTH, HEIGHT, false>(buf, nullptr, bandOffY,
                                                 camPos, camFwd, camRight, rawCamUp,
                                                 vfovRad, hfovRad);
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        PIP3D_FORCE_INLINE PIP3D_HOT IRAM_ATTR void
        drawCloudsZTested(uint16_t *__restrict__ buf,
                          const uint16_t *__restrict__ zBuf,
                          int16_t bandOffY,
                          const Vector3 &camPos,
                          const Vector3 &camFwd,
                          const Vector3 &camRight,
                          const Vector3 &rawCamUp,
                          float vfovRad,
                          float hfovRad) const noexcept
        {
            drawCloudsImpl<WIDTH, HEIGHT, true>(buf, zBuf, bandOffY,
                                                camPos, camFwd, camRight, rawCamUp,
                                                vfovRad, hfovRad);
        }

    private:
        template <uint16_t WIDTH, uint16_t HEIGHT, bool Z_TEST>
        PIP3D_HOT IRAM_ATTR void
        drawCloudsImpl(uint16_t *__restrict__ buf,
                       const uint16_t *__restrict__ zBuf,
                       int16_t bandOffY,
                       const Vector3 &camPos,
                       const Vector3 &camFwd,
                       const Vector3 &camRight,
                       const Vector3 &rawCamUp,
                       float vfovRad,
                       float hfovRad) const noexcept
        {
            if (unlikely(!enabled))
                return;

            prepareForFrame(vfovRad, hfovRad);

            Vector3 camUp = camRight.cross(camFwd);
            if (unlikely(camUp.lengthSquared() < 1e-4f))
                camUp = rawCamUp;

            const float tanHalfVfov = cachedTanHalfVfov_;
            const float tanHalfHfov = cachedTanHalfHfov_;
            const float invHalfH = 2.0f / static_cast<float>(SCREEN_HEIGHT);
            const float invHalfW = 2.0f / static_cast<float>(SCREEN_WIDTH);
            const float uvScale = cachedUvScale_;

            constexpr float kMinFadeDist = 1200.0f;
            constexpr float kMaxFadeDist = 2400.0f;
            constexpr float kInvFadeRange = 1.0f / (kMaxFadeDist - kMinFadeDist);

            const float srx_uv = invHalfW * tanHalfHfov * camRight.x * uvScale;
            const float srz_uv = invHalfW * tanHalfHfov * camRight.z * uvScale;
            const float baseRx = camFwd.x - tanHalfHfov * camRight.x;
            const float baseRz = camFwd.z - tanHalfHfov * camRight.z;

            const float planeX = camPos.x - driftX;
            const float planeZ = camPos.z - driftZ;

            const uint8_t *PIP3D_RESTRICT alphaBase = &detail::s_cloudsAlphaData[0][0];
            const uint8_t *PIP3D_RESTRICT shadeBase = &detail::s_cloudsShadeData[0][0];
            const uint16_t *PIP3D_RESTRICT lutPtr = shadeLut;

            constexpr uint16_t widthHalf = WIDTH >> 1;
            constexpr uint16_t UNROLL = 4;
            static_assert(widthHalf % UNROLL == 0, "widthHalf must be multiple of UNROLL");

            for (uint16_t yb = 0; yb < HEIGHT; ++yb)
            {
                const int16_t globalY = static_cast<int16_t>(bandOffY + yb);
                const float yN = 1.0f - (static_cast<float>(globalY) + 0.5f) * invHalfH;
                const float yFactor = yN * tanHalfVfov;

                const float vy = camFwd.y + yFactor * camUp.y;
                if (vy <= 0.02f)
                    continue;

                float t = cloudHeight * FastMath::fastReciprocal(vy);
                if (t >= kMaxFadeDist)
                    t = kMaxFadeDist;

                const float distFade = (t > kMinFadeDist)
                                           ? (kMaxFadeDist - t) * kInvFadeRange
                                           : 1.0f;
                const uint32_t rowAlphaMul32 =
                    static_cast<uint32_t>(cachedAlphaMul32_ * distFade + 0.5f);
                if (unlikely(rowAlphaMul32 == 0u))
                    continue;

                const float rx0 = baseRx + yFactor * camUp.x;
                const float rz0 = baseRz + yFactor * camUp.z;
                const float worldX0 = planeX + rx0 * t;
                const float worldZ0 = planeZ + rz0 * t;

                const float du = srx_uv * t;
                const float dv = srz_uv * t;

                int32_t u_fp = static_cast<int32_t>(worldX0 * uvScale * 65536.0f);
                int32_t v_fp = static_cast<int32_t>(worldZ0 * uvScale * 65536.0f);
                const int32_t du_fp2 = static_cast<int32_t>(du * 131072.0f);
                const int32_t dv_fp2 = static_cast<int32_t>(dv * 131072.0f);

                uint32_t *__restrict__ fbRow32 =
                    reinterpret_cast<uint32_t *>(buf + static_cast<size_t>(yb) * WIDTH);

                const uint16_t *__restrict__ zbRow = Z_TEST
                                                         ? (zBuf + static_cast<size_t>(yb) * WIDTH)
                                                         : nullptr;

                uint16_t x2 = 0;
                for (; x2 + UNROLL <= widthHalf; x2 += UNROLL)
                {

                    const uint32_t ti0 = (((uint32_t)v_fp >> 8) & 0xFF00u) | (((uint32_t)u_fp >> 16) & 0xFFu);
                    const uint8_t a8_0 = alphaBase[ti0];
                    u_fp += du_fp2;
                    v_fp += dv_fp2;

                    const uint32_t ti1 = (((uint32_t)v_fp >> 8) & 0xFF00u) | (((uint32_t)u_fp >> 16) & 0xFFu);
                    const uint8_t a8_1 = alphaBase[ti1];
                    u_fp += du_fp2;
                    v_fp += dv_fp2;

                    const uint32_t ti2 = (((uint32_t)v_fp >> 8) & 0xFF00u) | (((uint32_t)u_fp >> 16) & 0xFFu);
                    const uint8_t a8_2 = alphaBase[ti2];
                    u_fp += du_fp2;
                    v_fp += dv_fp2;

                    const uint32_t ti3 = (((uint32_t)v_fp >> 8) & 0xFF00u) | (((uint32_t)u_fp >> 16) & 0xFFu);
                    const uint8_t a8_3 = alphaBase[ti3];
                    u_fp += du_fp2;
                    v_fp += dv_fp2;

                    const uint32_t a5_0 = (static_cast<uint32_t>(a8_0) * rowAlphaMul32 + 127u) >> 8;
                    const uint32_t a5_1 = (static_cast<uint32_t>(a8_1) * rowAlphaMul32 + 127u) >> 8;
                    const uint32_t a5_2 = (static_cast<uint32_t>(a8_2) * rowAlphaMul32 + 127u) >> 8;
                    const uint32_t a5_3 = (static_cast<uint32_t>(a8_3) * rowAlphaMul32 + 127u) >> 8;

                    if (a5_0 > 0u)
                    {
                        if constexpr (Z_TEST)
                        {

                            const uint16_t z0a = zbRow[x2 * 2];
                            const uint16_t z0b = zbRow[x2 * 2 + 1];
                            if (z0a == 0 || z0b == 0)
                            {
                                const uint16_t sh = lutPtr[shadeBase[ti0]];
                                if (z0a == 0 && z0b == 0)
                                {

                                    const uint16_t px = (a5_0 >= 32u) ? sh
                                                                      : blend565(static_cast<uint16_t>(fbRow32[x2]), sh, a5_0);
                                    fbRow32[x2] = static_cast<uint32_t>(px) | (static_cast<uint32_t>(px) << 16);
                                }
                                else
                                {

                                    const uint32_t orig = fbRow32[x2];
                                    const uint16_t orig0 = static_cast<uint16_t>(orig);
                                    const uint16_t orig1 = static_cast<uint16_t>(orig >> 16);
                                    const uint16_t px0 = (z0a == 0)
                                                             ? ((a5_0 >= 32u) ? sh : blend565(orig0, sh, a5_0))
                                                             : orig0;
                                    const uint16_t px1 = (z0b == 0)
                                                             ? ((a5_0 >= 32u) ? sh : blend565(orig1, sh, a5_0))
                                                             : orig1;
                                    fbRow32[x2] = static_cast<uint32_t>(px0) | (static_cast<uint32_t>(px1) << 16);
                                }
                            }
                        }
                        else
                        {
                            const uint16_t sh = lutPtr[shadeBase[ti0]];
                            const uint16_t px = (a5_0 >= 32u) ? sh
                                                              : blend565(static_cast<uint16_t>(fbRow32[x2]), sh, a5_0);
                            fbRow32[x2] = static_cast<uint32_t>(px) | (static_cast<uint32_t>(px) << 16);
                        }
                    }
                    if (a5_1 > 0u)
                    {
                        if constexpr (Z_TEST)
                        {
                            const uint16_t z1a = zbRow[(x2 + 1) * 2];
                            const uint16_t z1b = zbRow[(x2 + 1) * 2 + 1];
                            if (z1a == 0 || z1b == 0)
                            {
                                const uint16_t sh = lutPtr[shadeBase[ti1]];
                                if (z1a == 0 && z1b == 0)
                                {
                                    const uint16_t px = (a5_1 >= 32u) ? sh
                                                                      : blend565(static_cast<uint16_t>(fbRow32[x2 + 1]), sh, a5_1);
                                    fbRow32[x2 + 1] = static_cast<uint32_t>(px) | (static_cast<uint32_t>(px) << 16);
                                }
                                else
                                {
                                    const uint32_t orig = fbRow32[x2 + 1];
                                    const uint16_t orig0 = static_cast<uint16_t>(orig);
                                    const uint16_t orig1 = static_cast<uint16_t>(orig >> 16);
                                    const uint16_t px0 = (z1a == 0)
                                                             ? ((a5_1 >= 32u) ? sh : blend565(orig0, sh, a5_1))
                                                             : orig0;
                                    const uint16_t px1 = (z1b == 0)
                                                             ? ((a5_1 >= 32u) ? sh : blend565(orig1, sh, a5_1))
                                                             : orig1;
                                    fbRow32[x2 + 1] = static_cast<uint32_t>(px0) | (static_cast<uint32_t>(px1) << 16);
                                }
                            }
                        }
                        else
                        {
                            const uint16_t sh = lutPtr[shadeBase[ti1]];
                            const uint16_t px = (a5_1 >= 32u) ? sh
                                                              : blend565(static_cast<uint16_t>(fbRow32[x2 + 1]), sh, a5_1);
                            fbRow32[x2 + 1] = static_cast<uint32_t>(px) | (static_cast<uint32_t>(px) << 16);
                        }
                    }
                    if (a5_2 > 0u)
                    {
                        if constexpr (Z_TEST)
                        {
                            const uint16_t z2a = zbRow[(x2 + 2) * 2];
                            const uint16_t z2b = zbRow[(x2 + 2) * 2 + 1];
                            if (z2a == 0 || z2b == 0)
                            {
                                const uint16_t sh = lutPtr[shadeBase[ti2]];
                                if (z2a == 0 && z2b == 0)
                                {
                                    const uint16_t px = (a5_2 >= 32u) ? sh
                                                                      : blend565(static_cast<uint16_t>(fbRow32[x2 + 2]), sh, a5_2);
                                    fbRow32[x2 + 2] = static_cast<uint32_t>(px) | (static_cast<uint32_t>(px) << 16);
                                }
                                else
                                {
                                    const uint32_t orig = fbRow32[x2 + 2];
                                    const uint16_t orig0 = static_cast<uint16_t>(orig);
                                    const uint16_t orig1 = static_cast<uint16_t>(orig >> 16);
                                    const uint16_t px0 = (z2a == 0)
                                                             ? ((a5_2 >= 32u) ? sh : blend565(orig0, sh, a5_2))
                                                             : orig0;
                                    const uint16_t px1 = (z2b == 0)
                                                             ? ((a5_2 >= 32u) ? sh : blend565(orig1, sh, a5_2))
                                                             : orig1;
                                    fbRow32[x2 + 2] = static_cast<uint32_t>(px0) | (static_cast<uint32_t>(px1) << 16);
                                }
                            }
                        }
                        else
                        {
                            const uint16_t sh = lutPtr[shadeBase[ti2]];
                            const uint16_t px = (a5_2 >= 32u) ? sh
                                                              : blend565(static_cast<uint16_t>(fbRow32[x2 + 2]), sh, a5_2);
                            fbRow32[x2 + 2] = static_cast<uint32_t>(px) | (static_cast<uint32_t>(px) << 16);
                        }
                    }
                    if (a5_3 > 0u)
                    {
                        if constexpr (Z_TEST)
                        {
                            const uint16_t z3a = zbRow[(x2 + 3) * 2];
                            const uint16_t z3b = zbRow[(x2 + 3) * 2 + 1];
                            if (z3a == 0 || z3b == 0)
                            {
                                const uint16_t sh = lutPtr[shadeBase[ti3]];
                                if (z3a == 0 && z3b == 0)
                                {
                                    const uint16_t px = (a5_3 >= 32u) ? sh
                                                                      : blend565(static_cast<uint16_t>(fbRow32[x2 + 3]), sh, a5_3);
                                    fbRow32[x2 + 3] = static_cast<uint32_t>(px) | (static_cast<uint32_t>(px) << 16);
                                }
                                else
                                {
                                    const uint32_t orig = fbRow32[x2 + 3];
                                    const uint16_t orig0 = static_cast<uint16_t>(orig);
                                    const uint16_t orig1 = static_cast<uint16_t>(orig >> 16);
                                    const uint16_t px0 = (z3a == 0)
                                                             ? ((a5_3 >= 32u) ? sh : blend565(orig0, sh, a5_3))
                                                             : orig0;
                                    const uint16_t px1 = (z3b == 0)
                                                             ? ((a5_3 >= 32u) ? sh : blend565(orig1, sh, a5_3))
                                                             : orig1;
                                    fbRow32[x2 + 3] = static_cast<uint32_t>(px0) | (static_cast<uint32_t>(px1) << 16);
                                }
                            }
                        }
                        else
                        {
                            const uint16_t sh = lutPtr[shadeBase[ti3]];
                            const uint16_t px = (a5_3 >= 32u) ? sh
                                                              : blend565(static_cast<uint16_t>(fbRow32[x2 + 3]), sh, a5_3);
                            fbRow32[x2 + 3] = static_cast<uint32_t>(px) | (static_cast<uint32_t>(px) << 16);
                        }
                    }
                }

                for (; x2 < widthHalf; ++x2)
                {
                    const uint32_t texIdx = (((uint32_t)v_fp >> 8) & 0xFF00u) | (((uint32_t)u_fp >> 16) & 0xFFu);
                    const uint8_t a8 = alphaBase[texIdx];
                    const uint32_t a5 = (static_cast<uint32_t>(a8) * rowAlphaMul32 + 127u) >> 8;
                    if (a5 > 0u)
                    {
                        if constexpr (Z_TEST)
                        {
                            const uint16_t za = zbRow[x2 * 2];
                            const uint16_t zb = zbRow[x2 * 2 + 1];
                            if (za == 0 || zb == 0)
                            {
                                const uint16_t shaded = lutPtr[shadeBase[texIdx]];
                                if (za == 0 && zb == 0)
                                {
                                    const uint16_t outPix = (a5 >= 32u) ? shaded
                                                                        : blend565(static_cast<uint16_t>(fbRow32[x2]), shaded, a5);
                                    fbRow32[x2] = static_cast<uint32_t>(outPix) | (static_cast<uint32_t>(outPix) << 16);
                                }
                                else
                                {
                                    const uint32_t orig = fbRow32[x2];
                                    const uint16_t orig0 = static_cast<uint16_t>(orig);
                                    const uint16_t orig1 = static_cast<uint16_t>(orig >> 16);
                                    const uint16_t px0 = (za == 0)
                                                             ? ((a5 >= 32u) ? shaded : blend565(orig0, shaded, a5))
                                                             : orig0;
                                    const uint16_t px1 = (zb == 0)
                                                             ? ((a5 >= 32u) ? shaded : blend565(orig1, shaded, a5))
                                                             : orig1;
                                    fbRow32[x2] = static_cast<uint32_t>(px0) | (static_cast<uint32_t>(px1) << 16);
                                }
                            }
                        }
                        else
                        {
                            const uint16_t shaded = lutPtr[shadeBase[texIdx]];
                            const uint16_t outPix = (a5 >= 32u) ? shaded
                                                                : blend565(static_cast<uint16_t>(fbRow32[x2]), shaded, a5);
                            fbRow32[x2] = static_cast<uint32_t>(outPix) | (static_cast<uint32_t>(outPix) << 16);
                        }
                    }
                    u_fp += du_fp2;
                    v_fp += dv_fp2;
                }

                if constexpr (WIDTH & 1u)
                {
                    const uint16_t xLast = WIDTH - 1;
                    const uint32_t texIdx = (((uint32_t)v_fp >> 8) & 0xFF00u) | (((uint32_t)u_fp >> 16) & 0xFFu);
                    const uint8_t a8 = alphaBase[texIdx];
                    const uint32_t a5 = (static_cast<uint32_t>(a8) * rowAlphaMul32 + 127u) >> 8;
                    if (a5 > 0u)
                    {
                        if constexpr (Z_TEST)
                        {
                            if (zbRow[xLast] == 0)
                            {
                                const uint16_t shaded = lutPtr[shadeBase[texIdx]];
                                buf[static_cast<size_t>(yb) * WIDTH + xLast] =
                                    (a5 >= 32u) ? shaded
                                                : blend565(buf[static_cast<size_t>(yb) * WIDTH + xLast], shaded, a5);
                            }
                        }
                        else
                        {
                            const uint16_t shaded = lutPtr[shadeBase[texIdx]];
                            buf[static_cast<size_t>(yb) * WIDTH + xLast] =
                                (a5 >= 32u) ? shaded
                                            : blend565(buf[static_cast<size_t>(yb) * WIDTH + xLast], shaded, a5);
                        }
                    }
                }
            }
        }
    };
}