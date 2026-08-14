#pragma once

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Math/Algebra.hpp"
#include "Camera/Camera.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"
#include "Geometry/Instance.hpp"
#include "Debug/Logging.hpp"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>

namespace pip3D
{
    static constexpr uint16_t GLOW_MAX_STAMPS = 32;
    static constexpr uint16_t GLOW_LUT_SIZE = 32;

    struct GlowSettings
    {
        bool enabled = true;
        float haloScale = 3.2f;
        float innerCoreScale = 1.30f;
        float innerCoreWhite = 0.85f;
        float outerHaloGain = 0.85f;
        float intensity = 1.2f;
        uint16_t minScreenRadius = 3;
        uint16_t maxScreenRadius = 220;

        bool softDepthOcclusion = true;

        float bleedMaxFrac = 0.85f;
    };

    enum GlowShape : uint8_t
    {
        GLOW_ROUND = 0,
        GLOW_ELONG_H = 1,
        GLOW_ELONG_V = 2,
        GLOW_ANAMORPHIC = 3,
        GLOW_RECT = 4,
        GLOW_SEGMENT = 5,
        GLOW_AUTO = 255
    };

    struct GlowContext
    {
        const Camera &cam;
        const CameraFrustum &frust;
        const Matrix4x4 &viewProj;
        const Viewport &viewport;

        GlowContext(const Camera &c, const CameraFrustum &f,
                    const Matrix4x4 &vp, const Viewport &v)
            : cam(c), frust(f), viewProj(vp), viewport(v) {}
    };

    struct alignas(16) GlowStamp
    {
        int16_t cx;
        int16_t cy;
        int16_t x1;
        int16_t y1;
        uint16_t innerRadiusX;
        uint16_t innerRadiusY;
        uint16_t outerRadiusX;
        uint16_t outerRadiusY;
        uint16_t coreColor565;
        uint16_t haloColor565;
        uint16_t zValue;
        uint8_t intensity;
        uint8_t shape;
        uint8_t visibility;
        uint8_t reserved_;
        float cosRot;
        float sinRot;
    };

    class Glow
    {
    public:
        Glow() noexcept = default;
        ~Glow() = default;
        Glow(const Glow &) = delete;
        Glow &operator=(const Glow &) = delete;

        bool init() noexcept;

        void clearStamps() noexcept { stampCount_ = 0; }

        bool registerInstance(const MeshInstance *inst, const GlowContext &ctx) noexcept;

        bool registerLight(const Vector3 &worldPos,
                           float lightWorldRadius,
                           Color color,
                           float intensity,
                           uint8_t shape,
                           const GlowContext &ctx,
                           const Vector3 *direction = nullptr,
                           float spotCosCutoff = -1.0f) noexcept;

        bool registerSegment(const Vector3 &p0,
                             const Vector3 &p1,
                             float tubeWorldRadius,
                             Color color,
                             float intensity,
                             const GlowContext &ctx) noexcept;

        bool registerBox(const Vector3 &centerPos,
                         float widthWorld,
                         float heightWorld,
                         const Quaternion &rot,
                         Color color,
                         float intensity,
                         const GlowContext &ctx) noexcept;

        bool registerSun(const Vector3 &sunDir,
                         Color sunColor,
                         float intensity,
                         const GlowContext &ctx) noexcept;

        void IRAM_ATTR compositeStamps(
            uint16_t *PIP3D_RESTRICT fbBand,
            uint16_t bandWidth,
            uint16_t bandHeight,
            int16_t bandY0,
            const ZBuffer *zBuf) noexcept;

        void setSettings(const GlowSettings &s) noexcept { settings_ = s; }
        const GlowSettings &settings() const noexcept { return settings_; }
        GlowSettings &settings() noexcept { return settings_; }

        uint16_t stampCount() const noexcept { return stampCount_; }

        static constexpr uint32_t memoryUsageBytes() noexcept
        {
            return GLOW_MAX_STAMPS * sizeof(GlowStamp) + 128u;
        }

    private:
        alignas(16) GlowStamp stamps_[GLOW_MAX_STAMPS] = {};
        alignas(16) uint8_t radialLutInner_[GLOW_LUT_SIZE] = {};
        alignas(16) uint8_t radialLutOuter_[GLOW_LUT_SIZE] = {};
        uint16_t stampCount_ = 0;
        GlowSettings settings_;
        bool initialized_ = false;

        PIP3D_FORCE_INLINE static uint16_t fastAdd565Saturate(uint16_t dst, uint16_t src) noexcept
        {
            const uint32_t d = dst;
            const uint32_t s = src;

            uint32_t rb = (d & 0xF81Fu) + (s & 0xF81Fu);
            uint32_t g = (d & 0x07E0u) + (s & 0x07E0u);

            const uint32_t b_overflow = rb & 0x0020u;
            rb |= (b_overflow - (b_overflow >> 5));
            rb &= ~b_overflow;

            const uint32_t r_overflow = rb & 0x10000u;
            rb |= (r_overflow - (r_overflow >> 5));

            const uint32_t g_overflow = g & 0x0800u;
            g |= (g_overflow - (g_overflow >> 6));
            g &= ~g_overflow;

            return static_cast<uint16_t>((rb & 0xF81Fu) | g);
        }

        PIP3D_FORCE_INLINE static uint16_t scaleColor565(uint16_t c, uint8_t factor) noexcept
        {
            if (factor == 0)
                return 0;
            if (factor == 255)
                return c;

            const uint32_t f = factor;
            const uint32_t r = ((c & 0xF800u) * f) >> 8;
            const uint32_t g = ((c & 0x07E0u) * f) >> 8;
            const uint32_t b = ((c & 0x001Fu) * f) >> 8;

            return static_cast<uint16_t>((r & 0xF800u) | (g & 0x07E0u) | (b & 0x001Fu));
        }

        PIP3D_FORCE_INLINE static uint16_t blend565(uint16_t c1, uint16_t c2, uint8_t t) noexcept
        {
            if (t == 0)
                return c1;
            if (t == 255)
                return c2;

            const uint32_t alpha = t;
            const uint32_t invA = 255u - alpha;

            const uint32_t r = (((c1 & 0xF800u) * invA + (c2 & 0xF800u) * alpha) >> 8) & 0xF800u;
            const uint32_t g = (((c1 & 0x07E0u) * invA + (c2 & 0x07E0u) * alpha) >> 8) & 0x07E0u;
            const uint32_t b = (((c1 & 0x001Fu) * invA + (c2 & 0x001Fu) * alpha) >> 8) & 0x001Fu;

            return static_cast<uint16_t>(r | g | b);
        }

        PIP3D_FORCE_INLINE uint8_t IRAM_ATTR computeStampVisibility(
            const GlowStamp &s,
            const uint16_t *PIP3D_RESTRICT zData,
            uint16_t bandWidth,
            uint16_t bandHeight,
            int16_t bandY0) const noexcept
        {
            const int32_t rx = s.innerRadiusX > 0 ? s.innerRadiusX : 4;
            const int32_t ry = s.innerRadiusY > 0 ? s.innerRadiusY : 4;

            const uint16_t lampZ = s.zValue;

            uint32_t visibleCount = 0;
            uint32_t totalCount = 0;

            for (int32_t dy = -1; dy <= 1; ++dy)
            {
                const int32_t y = s.cy + (dy * ry) / 2;
                const int32_t localY = y - bandY0;
                if (localY < 0 || localY >= bandHeight)
                    continue;

                const uint16_t *PIP3D_RESTRICT zRow = zData + localY * bandWidth;

                for (int32_t dx = -1; dx <= 1; ++dx)
                {
                    const int32_t x = s.cx + (dx * rx) / 2;
                    if (x < 0 || x >= static_cast<int32_t>(bandWidth))
                        continue;

                    const uint16_t zbufZ = zRow[x] & Z_DEPTH_MASK;
                    ++totalCount;
                    if (zbufZ == 0 || zbufZ <= lampZ + 6u)
                        ++visibleCount;
                }
            }

            if (totalCount == 0)
                return 255;

            return static_cast<uint8_t>((visibleCount * 255u) / totalCount);
        }

        void IRAM_ATTR drawStampPassRadial(
            const GlowStamp &s,
            uint16_t *PIP3D_RESTRICT fbBand,
            uint16_t bandWidth,
            uint16_t bandHeight,
            int16_t bandY0,
            const uint16_t *PIP3D_RESTRICT zData,
            bool innerPass) const noexcept;

        void IRAM_ATTR drawStampPassSegment(
            const GlowStamp &s,
            uint16_t *PIP3D_RESTRICT fbBand,
            uint16_t bandWidth,
            uint16_t bandHeight,
            int16_t bandY0,
            const uint16_t *PIP3D_RESTRICT zData,
            bool innerPass) const noexcept;

        void IRAM_ATTR drawStampPassRect(
            const GlowStamp &s,
            uint16_t *PIP3D_RESTRICT fbBand,
            uint16_t bandWidth,
            uint16_t bandHeight,
            int16_t bandY0,
            const uint16_t *PIP3D_RESTRICT zData,
            bool innerPass) const noexcept;

        PIP3D_FORCE_INLINE void IRAM_ATTR drawStampPass(
            const GlowStamp &s,
            uint16_t *PIP3D_RESTRICT fbBand,
            uint16_t bandWidth,
            uint16_t bandHeight,
            int16_t bandY0,
            const uint16_t *PIP3D_RESTRICT zData,
            bool innerPass) const noexcept
        {
            switch (s.shape)
            {
            case GLOW_SEGMENT:
                drawStampPassSegment(s, fbBand, bandWidth, bandHeight, bandY0, zData, innerPass);
                break;
            case GLOW_RECT:
                drawStampPassRect(s, fbBand, bandWidth, bandHeight, bandY0, zData, innerPass);
                break;
            default:
                drawStampPassRadial(s, fbBand, bandWidth, bandHeight, bandY0, zData, innerPass);
                break;
            }
        }
    };

    inline bool Glow::init() noexcept
    {
        std::memset(stamps_, 0, sizeof(stamps_));

        for (uint32_t i = 0; i < GLOW_LUT_SIZE; ++i)
        {
            const float rNorm = static_cast<float>(i) / static_cast<float>(GLOW_LUT_SIZE - 1);
            float v = 1.0f - powf(rNorm, 1.2f);
            if (v < 0.0f)
                v = 0.0f;
            v = v * v * (3.0f - 2.0f * v);
            uint32_t val = static_cast<uint32_t>(v * 255.0f + 0.5f);
            radialLutInner_[i] = static_cast<uint8_t>(val > 255u ? 255u : val);
        }

        for (uint32_t i = 0; i < GLOW_LUT_SIZE; ++i)
        {
            const float rSqNorm = static_cast<float>(i) / static_cast<float>(GLOW_LUT_SIZE - 1);
            float v = 1.0f - rSqNorm;
            if (v < 0.0f)
                v = 0.0f;
            v = v * v;
            uint32_t val = static_cast<uint32_t>(v * 255.0f + 0.5f);
            radialLutOuter_[i] = static_cast<uint8_t>(val > 255u ? 255u : val);
        }

        initialized_ = true;
        LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
             "Pip3D Glow Engine Initialized. DRAM Memory: %u Bytes",
             (unsigned)memoryUsageBytes());
        return true;
    }

    inline bool Glow::registerInstance(const MeshInstance *inst, const GlowContext &ctx) noexcept
    {
        if (!inst || !inst->isEmissive() || !initialized_ || !settings_.enabled)
            return false;

        const Vector3 centerPos = inst->center();
        const float worldRadius = inst->radius();
        if (worldRadius <= 0.001f)
            return false;

        Color glowColor = inst->emissiveColor();
        float intensity = inst->emissiveIntensity();
        uint8_t shape = inst->emissiveShape();

        const Vector3 userBox = inst->emissiveBoxSize();
        Vector3 localHalf(0.0f, 0.0f, 0.0f);

        const Mesh *mesh = inst->getMesh();
        if (mesh)
            mesh->getLocalExtents(localHalf);

        const Vector3 scl = inst->getScale();
        const Vector3 worldSizeAuto(
            localHalf.x * 2.0f * scl.x,
            localHalf.y * 2.0f * scl.y,
            localHalf.z * 2.0f * scl.z);

        const float userW = (userBox.x > 0.001f) ? userBox.x : worldSizeAuto.x;
        const float userH = (userBox.y > 0.001f) ? userBox.y : worldSizeAuto.y;
        const float userD = (userBox.z > 0.001f) ? userBox.z : worldSizeAuto.z;

        if (shape == GLOW_AUTO)
        {
            if (userW > userH * 1.4f && userW > userD * 1.4f)
            {
                return registerBox(centerPos, userW, userH,
                                   inst->getRotation(),
                                   glowColor, intensity, ctx);
            }
            if (userH > userW * 1.4f && userH > userD * 1.4f)
            {
                shape = GLOW_ELONG_V;
            }
            else
            {
                shape = GLOW_ROUND;
            }
        }

        if (shape == GLOW_RECT)
        {
            return registerBox(centerPos, userW, userH,
                               inst->getRotation(),
                               glowColor, intensity, ctx);
        }

        return registerLight(centerPos, worldRadius, glowColor, intensity, shape, ctx);
    }

    inline bool Glow::registerLight(
        const Vector3 &worldPos,
        float lightWorldRadius,
        Color color,
        float intensity,
        uint8_t shape,
        const GlowContext &ctx,
        const Vector3 *direction,
        float spotCosCutoff) noexcept
    {
        if (!initialized_ || !settings_.enabled || intensity <= 0.01f)
            return false;
        if (stampCount_ >= GLOW_MAX_STAMPS || lightWorldRadius <= 0.0f)
            return false;

        const Vector3 toLight = worldPos - ctx.cam.position;
        const float distSq = toLight.lengthSquared();
        if (distSq < 0.04f)
            return false;

        const Vector3 camFwd = ctx.cam.forward();
        const float eyeZ = toLight.dot(camFwd);
        if (eyeZ <= 0.08f)
            return false;

        if (direction && spotCosCutoff > -0.99f)
        {
            const Vector3 dirNorm = *direction;
            const Vector3 lightToCam = -toLight * FastMath::fastInvSqrt(distSq);
            const float cosAngle = lightToCam.dot(dirNorm);
            if (cosAngle < spotCosCutoff)
                return false;
            const float spotAtten = (cosAngle - spotCosCutoff) / (1.0f - spotCosCutoff);
            intensity *= spotAtten * spotAtten;
            if (intensity <= 0.01f)
                return false;
        }

        const float cullRadius = lightWorldRadius * settings_.haloScale;
        if (!ctx.frust.testSphere(worldPos, cullRadius))
            return false;

        const Vector3 scr = CameraController::project(worldPos, ctx.viewProj, ctx.viewport);
        if (scr.z <= 0.0f)
            return false;

        const Vector3 frontWorld = worldPos - camFwd * lightWorldRadius;
        const Vector3 frontScr = CameraController::project(frontWorld, ctx.viewProj, ctx.viewport);
        const uint16_t lampZ = (frontScr.z > 0.0f)
                                   ? static_cast<uint16_t>(frontScr.z + 4.0f)
                                   : Z_DEPTH_MAX;

        const float vfovRad = ctx.cam.fov * kDegToRad;
        const float focal = (static_cast<float>(ctx.viewport.height) * 0.5f) / tanf(vfovRad * 0.5f);
        const float meshScreenRadius = focal * lightWorldRadius / eyeZ;

        float innerR = meshScreenRadius * settings_.innerCoreScale;
        float outerR = meshScreenRadius * settings_.haloScale;

        innerR = clamp(innerR, static_cast<float>(settings_.minScreenRadius), static_cast<float>(settings_.maxScreenRadius));
        outerR = clamp(outerR, static_cast<float>(settings_.minScreenRadius), static_cast<float>(settings_.maxScreenRadius));

        GlowStamp s;
        s.cx = static_cast<int16_t>(scr.x);
        s.cy = static_cast<int16_t>(scr.y);
        s.x1 = s.cx;
        s.y1 = s.cy;
        s.cosRot = 1.0f;
        s.sinRot = 0.0f;

        switch (shape)
        {
        case GLOW_ELONG_V:
            s.innerRadiusX = static_cast<uint16_t>(innerR * 0.75f + 0.5f);
            s.innerRadiusY = static_cast<uint16_t>(innerR * 1.25f + 0.5f);
            s.outerRadiusX = static_cast<uint16_t>(outerR * 0.80f + 0.5f);
            s.outerRadiusY = static_cast<uint16_t>(outerR * 1.25f + 0.5f);
            break;
        case GLOW_ELONG_H:
        case GLOW_ANAMORPHIC:
            s.innerRadiusX = static_cast<uint16_t>(innerR * 1.30f + 0.5f);
            s.innerRadiusY = static_cast<uint16_t>(innerR * 0.75f + 0.5f);
            s.outerRadiusX = static_cast<uint16_t>(outerR * (shape == GLOW_ANAMORPHIC ? 2.8f : 1.4f) + 0.5f);
            s.outerRadiusY = static_cast<uint16_t>(outerR * 0.70f + 0.5f);
            break;
        case GLOW_ROUND:
        default:
            s.innerRadiusX = static_cast<uint16_t>(innerR + 0.5f);
            s.innerRadiusY = static_cast<uint16_t>(innerR + 0.5f);
            s.outerRadiusX = static_cast<uint16_t>(outerR + 0.5f);
            s.outerRadiusY = static_cast<uint16_t>(outerR + 0.5f);
            break;
        }

        const uint8_t whiteAmt = static_cast<uint8_t>(settings_.innerCoreWhite * 255.0f);
        s.coreColor565 = blend565(color.rgb565, Color::WHITE, whiteAmt);
        s.haloColor565 = color.rgb565;

        s.zValue = lampZ;
        s.intensity = static_cast<uint8_t>(clamp(intensity, 0.0f, 2.0f) * 127.5f);
        s.shape = shape;
        s.visibility = 255;
        s.reserved_ = 0;

        stamps_[stampCount_++] = s;
        return true;
    }

    inline bool Glow::registerSegment(
        const Vector3 &p0,
        const Vector3 &p1,
        float tubeWorldRadius,
        Color color,
        float intensity,
        const GlowContext &ctx) noexcept
    {
        if (!initialized_ || !settings_.enabled || intensity <= 0.01f)
            return false;
        if (stampCount_ >= GLOW_MAX_STAMPS)
            return false;

        const Vector3 mid = (p0 + p1) * 0.5f;
        const float segLen = (p1 - p0).length();
        const float cullRadius = (segLen * 0.5f + tubeWorldRadius) * settings_.haloScale;

        if (!ctx.frust.testSphere(mid, cullRadius))
            return false;

        const Vector3 scr0 = CameraController::project(p0, ctx.viewProj, ctx.viewport);
        const Vector3 scr1 = CameraController::project(p1, ctx.viewProj, ctx.viewport);
        if (scr0.z <= 0.0f && scr1.z <= 0.0f)
            return false;

        const Vector3 toMid = mid - ctx.cam.position;
        const float eyeZ = toMid.dot(ctx.cam.forward());
        if (eyeZ <= 0.08f)
            return false;

        const float vfovRad = ctx.cam.fov * kDegToRad;
        const float focal = (static_cast<float>(ctx.viewport.height) * 0.5f) / tanf(vfovRad * 0.5f);
        const float meshScreenRadius = focal * tubeWorldRadius / eyeZ;

        float innerR = clamp(meshScreenRadius * settings_.innerCoreScale, static_cast<float>(settings_.minScreenRadius), static_cast<float>(settings_.maxScreenRadius));
        float outerR = clamp(meshScreenRadius * settings_.haloScale, static_cast<float>(settings_.minScreenRadius), static_cast<float>(settings_.maxScreenRadius));

        GlowStamp s;
        s.cx = static_cast<int16_t>(scr0.x);
        s.cy = static_cast<int16_t>(scr0.y);
        s.x1 = static_cast<int16_t>(scr1.x);
        s.y1 = static_cast<int16_t>(scr1.y);
        s.innerRadiusX = static_cast<uint16_t>(innerR + 0.5f);
        s.innerRadiusY = static_cast<uint16_t>(innerR + 0.5f);
        s.outerRadiusX = static_cast<uint16_t>(outerR + 0.5f);
        s.outerRadiusY = static_cast<uint16_t>(outerR + 0.5f);
        s.cosRot = 1.0f;
        s.sinRot = 0.0f;

        const uint8_t whiteAmt = static_cast<uint8_t>(settings_.innerCoreWhite * 255.0f);
        s.coreColor565 = blend565(color.rgb565, Color::WHITE, whiteAmt);
        s.haloColor565 = color.rgb565;

        const float minZ = fminf(scr0.z, scr1.z);
        s.zValue = (minZ > 0.0f) ? static_cast<uint16_t>(minZ + 4.0f) : Z_DEPTH_MAX;
        s.intensity = static_cast<uint8_t>(clamp(intensity, 0.0f, 2.0f) * 127.5f);
        s.shape = GLOW_SEGMENT;
        s.visibility = 255;
        s.reserved_ = 0;

        stamps_[stampCount_++] = s;
        return true;
    }

    inline bool Glow::registerBox(
        const Vector3 &centerPos,
        float widthWorld,
        float heightWorld,
        const Quaternion &rot,
        Color color,
        float intensity,
        const GlowContext &ctx) noexcept
    {
        if (!initialized_ || !settings_.enabled || intensity <= 0.01f)
            return false;
        if (stampCount_ >= GLOW_MAX_STAMPS)
            return false;
        if (widthWorld <= 0.01f || heightWorld <= 0.01f)
            return false;

        const Vector3 fromCam = centerPos - ctx.cam.position;
        const float distSq = fromCam.lengthSquared();
        if (distSq < 0.04f)
            return false;

        const float diag = sqrtf(widthWorld * widthWorld + heightWorld * heightWorld) * 0.5f;
        if (!ctx.frust.testSphere(centerPos, diag * settings_.haloScale))
            return false;

        const Vector3 scr = CameraController::project(centerPos, ctx.viewProj, ctx.viewport);
        if (scr.z <= 0.0f)
            return false;

        const float eyeZ = fromCam.dot(ctx.cam.forward());
        if (eyeZ <= 0.08f)
            return false;

        const Vector3 rightWorld = rot.rotate(Vector3(1.0f, 0.0f, 0.0f)) * (widthWorld * 0.5f);
        const Vector3 upWorld = rot.rotate(Vector3(0.0f, 1.0f, 0.0f)) * (heightWorld * 0.5f);

        const Vector3 scrR = CameraController::project(centerPos + rightWorld, ctx.viewProj, ctx.viewport);
        const Vector3 scrU = CameraController::project(centerPos + upWorld, ctx.viewProj, ctx.viewport);

        const float vx = scrR.x - scr.x;
        const float vy = scrR.y - scr.y;
        const float ux = scrU.x - scr.x;
        const float uy = scrU.y - scr.y;

        const float halfScrW = sqrtf(vx * vx + vy * vy);
        const float halfScrH = sqrtf(ux * ux + uy * uy);

        float innerRx = clamp(halfScrW * settings_.innerCoreScale,
                              4.0f, static_cast<float>(settings_.maxScreenRadius));
        float innerRy = clamp(halfScrH * settings_.innerCoreScale,
                              4.0f, static_cast<float>(settings_.maxScreenRadius));
        float outerRx = clamp(halfScrW * settings_.haloScale,
                              8.0f, static_cast<float>(settings_.maxScreenRadius));
        float outerRy = clamp(halfScrH * settings_.haloScale,
                              8.0f, static_cast<float>(settings_.maxScreenRadius));

        const float rotAngle = atan2f(vy, vx);
        const float cosR = cosf(-rotAngle);
        const float sinR = sinf(-rotAngle);

        GlowStamp s;

        s.cx = static_cast<int16_t>(scr.x);
        s.cy = static_cast<int16_t>(scr.y);
        s.x1 = s.cx;
        s.y1 = s.cy;
        s.innerRadiusX = static_cast<uint16_t>(innerRx + 0.5f);
        s.innerRadiusY = static_cast<uint16_t>(innerRy + 0.5f);
        s.outerRadiusX = static_cast<uint16_t>(outerRx + 0.5f);
        s.outerRadiusY = static_cast<uint16_t>(outerRy + 0.5f);
        s.cosRot = cosR;
        s.sinRot = sinR;

        const uint8_t whiteAmt = static_cast<uint8_t>(settings_.innerCoreWhite * 255.0f);
        s.coreColor565 = blend565(color.rgb565, Color::WHITE, whiteAmt);
        s.haloColor565 = color.rgb565;

        s.zValue = static_cast<uint16_t>(scr.z + 4.0f);
        s.intensity = static_cast<uint8_t>(clamp(intensity, 0.0f, 2.0f) * 127.5f);
        s.shape = GLOW_RECT;
        s.visibility = 255;
        s.reserved_ = 0;

        stamps_[stampCount_++] = s;
        return true;
    }

    inline bool Glow::registerSun(
        const Vector3 &sunDir,
        Color sunColor,
        float intensity,
        const GlowContext &ctx) noexcept
    {
        if (!initialized_ || !settings_.enabled || intensity <= 0.01f)
            return false;
        if (stampCount_ >= GLOW_MAX_STAMPS)
            return false;

        if (sunDir.y <= 0.05f || sunDir.dot(ctx.cam.forward()) <= 0.1f)
            return false;

        const float skyDist = ctx.cam.farPlane * 0.85f;
        const Vector3 sunPos = ctx.cam.position + sunDir * skyDist;
        const Vector3 scr = CameraController::project(sunPos, ctx.viewProj, ctx.viewport);
        if (scr.z <= 0.0f)
            return false;

        const float sunR = 65.0f;
        GlowStamp s;
        s.cx = static_cast<int16_t>(scr.x);
        s.cy = static_cast<int16_t>(scr.y);
        s.x1 = s.cx;
        s.y1 = s.cy;
        s.innerRadiusX = static_cast<uint16_t>(sunR * 0.45f);
        s.innerRadiusY = static_cast<uint16_t>(sunR * 0.45f);
        s.outerRadiusX = static_cast<uint16_t>(sunR);
        s.outerRadiusY = static_cast<uint16_t>(sunR);
        s.coreColor565 = Color::WHITE;
        s.haloColor565 = sunColor.rgb565;
        s.zValue = Z_DEPTH_MAX;
        s.intensity = static_cast<uint8_t>(clamp(intensity, 0.0f, 2.0f) * 127.5f);
        s.shape = GLOW_ANAMORPHIC;
        s.cosRot = 1.0f;
        s.sinRot = 0.0f;
        s.visibility = 255;
        s.reserved_ = 0;

        stamps_[stampCount_++] = s;
        return true;
    }

    inline void IRAM_ATTR Glow::drawStampPassRadial(
        const GlowStamp &s,
        uint16_t *PIP3D_RESTRICT fbBand,
        uint16_t bandWidth,
        uint16_t bandHeight,
        int16_t bandY0,
        const uint16_t *PIP3D_RESTRICT zData,
        bool innerPass) const noexcept
    {
        const int32_t rx = innerPass ? s.innerRadiusX : s.outerRadiusX;
        const int32_t ry = innerPass ? s.innerRadiusY : s.outerRadiusY;
        if (rx <= 0 || ry <= 0)
            return;

        int32_t yTopLocal = s.cy - ry - bandY0;
        int32_t yBotLocal = s.cy + ry - bandY0;
        if (yBotLocal < 0 || yTopLocal >= bandHeight)
            return;

        if (yTopLocal < 0)
            yTopLocal = 0;
        if (yBotLocal >= bandHeight)
            yBotLocal = bandHeight - 1;

        int32_t xL = s.cx - rx;
        int32_t xR = s.cx + rx;
        if (xR < 0 || xL >= bandWidth)
            return;
        if (xL < 0)
            xL = 0;
        if (xR >= bandWidth)
            xR = bandWidth - 1;

        const float passGain = innerPass ? 1.0f : settings_.outerHaloGain;
        const uint8_t passIntensity = static_cast<uint8_t>(
            (static_cast<uint32_t>(s.intensity) *
             static_cast<uint32_t>(clamp(settings_.intensity * passGain, 0.0f, 2.0f) * 127.5f)) >>
            7);

        if (passIntensity == 0)
            return;

        const uint16_t passColor = innerPass ? s.coreColor565 : s.haloColor565;
        const uint8_t *PIP3D_RESTRICT lut = innerPass ? radialLutInner_ : radialLutOuter_;

        alignas(16) uint16_t stampPalette[GLOW_LUT_SIZE];
        alignas(16) uint16_t stampPaletteBleed[GLOW_LUT_SIZE];

        const uint32_t Kx = (31u * 65536u) / static_cast<uint32_t>(rx * rx);
        const uint32_t Ky = (31u * 65536u) / static_cast<uint32_t>(ry * ry);

        const bool useBleed = (!innerPass) && settings_.softDepthOcclusion && (settings_.bleedMaxFrac > 0.001f);
        const uint8_t bleedFull = useBleed ? static_cast<uint8_t>(clamp(settings_.bleedMaxFrac * 255.0f, 0.0f, 255.0f) + 0.5f) : 0u;
        const uint8_t stampVis = s.visibility;
        const uint8_t bleedAtten = static_cast<uint8_t>((static_cast<uint32_t>(stampVis) * static_cast<uint32_t>(bleedFull)) >> 8);

        for (uint32_t i = 0; i < GLOW_LUT_SIZE; ++i)
        {
            const uint8_t alpha = (static_cast<uint32_t>(lut[i]) * passIntensity) >> 8;
            const uint16_t col = scaleColor565(passColor, alpha);
            stampPalette[i] = col;
            stampPaletteBleed[i] = (useBleed && bleedAtten < 255u) ? scaleColor565(col, bleedAtten) : col;
        }

        alignas(16) uint32_t xTermFixed[256];
        const int32_t maxDx = (rx > 255) ? 255 : rx;
        for (int32_t dx = 0; dx <= maxDx; ++dx)
        {
            xTermFixed[dx] = static_cast<uint32_t>(dx * dx) * Kx;
        }

        const uint16_t lampZ = s.zValue;

        for (int32_t y = yTopLocal; y <= yBotLocal; ++y)
        {
            const int32_t globalY = y + bandY0;
            const int32_t dy = (globalY >= s.cy) ? (globalY - s.cy) : (s.cy - globalY);
            if (dy > ry)
                continue;

            const uint32_t Qy = static_cast<uint32_t>(dy * dy) * Ky;
            if (Qy >= (GLOW_LUT_SIZE << 16))
                continue;

            uint16_t *PIP3D_RESTRICT fbRow = fbBand + y * bandWidth;
            const uint16_t *PIP3D_RESTRICT zRow = zData ? (zData + y * bandWidth) : nullptr;

            PIP3D_PREFETCH_W(fbRow + bandWidth);
            if (zRow)
                PIP3D_PREFETCH_R(zRow + bandWidth);

            for (int32_t x = xL; x <= xR; ++x)
            {
                const int32_t dx = (x >= s.cx) ? (x - s.cx) : (s.cx - x);
                if (dx > maxDx)
                    continue;

                const uint32_t q16 = Qy + xTermFixed[dx];
                const uint32_t idx = q16 >> 16;
                if (idx >= GLOW_LUT_SIZE)
                {
                    if (x >= s.cx)
                        break;
                    continue;
                }

                bool isBleed = false;
                if (zRow)
                {
                    const uint16_t zbufZ = zRow[x] & Z_DEPTH_MASK;
                    if (zbufZ != 0 && zbufZ > lampZ + 6u)
                    {
                        if (innerPass || !useBleed || bleedAtten == 0u)
                            continue;
                        isBleed = true;
                    }
                }

                const uint16_t addColor = isBleed ? stampPaletteBleed[idx] : stampPalette[idx];
                if (!addColor)
                    continue;

                fbRow[x] = fastAdd565Saturate(fbRow[x], addColor);
            }
        }
    }

    inline void IRAM_ATTR Glow::drawStampPassSegment(
        const GlowStamp &s,
        uint16_t *PIP3D_RESTRICT fbBand,
        uint16_t bandWidth,
        uint16_t bandHeight,
        int16_t bandY0,
        const uint16_t *PIP3D_RESTRICT zData,
        bool innerPass) const noexcept
    {
        const int32_t rx = innerPass ? s.innerRadiusX : s.outerRadiusX;
        const int32_t ry = innerPass ? s.innerRadiusY : s.outerRadiusY;
        if (rx <= 0 || ry <= 0)
            return;

        const int16_t minY = s.cy < s.y1 ? s.cy : s.y1;
        const int16_t maxY = s.cy > s.y1 ? s.cy : s.y1;
        int32_t yTopLocal = minY - ry - bandY0;
        int32_t yBotLocal = maxY + ry - bandY0;
        if (yBotLocal < 0 || yTopLocal >= bandHeight)
            return;

        if (yTopLocal < 0)
            yTopLocal = 0;
        if (yBotLocal >= bandHeight)
            yBotLocal = bandHeight - 1;

        const int16_t minX = s.cx < s.x1 ? s.cx : s.x1;
        const int16_t maxX = s.cx > s.x1 ? s.cx : s.x1;
        int32_t xL = minX - rx;
        int32_t xR = maxX + rx;
        if (xR < 0 || xL >= bandWidth)
            return;
        if (xL < 0)
            xL = 0;
        if (xR >= bandWidth)
            xR = bandWidth - 1;

        const float passGain = innerPass ? 1.0f : settings_.outerHaloGain;
        const uint8_t passIntensity = static_cast<uint8_t>(
            (static_cast<uint32_t>(s.intensity) *
             static_cast<uint32_t>(clamp(settings_.intensity * passGain, 0.0f, 2.0f) * 127.5f)) >>
            7);

        if (passIntensity == 0)
            return;

        const uint16_t passColor = innerPass ? s.coreColor565 : s.haloColor565;
        const uint8_t *PIP3D_RESTRICT lut = innerPass ? radialLutInner_ : radialLutOuter_;

        alignas(16) uint16_t stampPalette[GLOW_LUT_SIZE];
        alignas(16) uint16_t stampPaletteBleed[GLOW_LUT_SIZE];

        const uint32_t Kx = (31u * 65536u) / static_cast<uint32_t>(rx * rx);
        const uint32_t Ky = (31u * 65536u) / static_cast<uint32_t>(ry * ry);

        const bool useBleed = (!innerPass) && settings_.softDepthOcclusion && (settings_.bleedMaxFrac > 0.001f);
        const uint8_t bleedFull = useBleed ? static_cast<uint8_t>(clamp(settings_.bleedMaxFrac * 255.0f, 0.0f, 255.0f) + 0.5f) : 0u;
        const uint8_t stampVis = s.visibility;
        const uint8_t bleedAtten = static_cast<uint8_t>((static_cast<uint32_t>(stampVis) * static_cast<uint32_t>(bleedFull)) >> 8);

        for (uint32_t i = 0; i < GLOW_LUT_SIZE; ++i)
        {
            const uint8_t alpha = (static_cast<uint32_t>(lut[i]) * passIntensity) >> 8;
            const uint16_t col = scaleColor565(passColor, alpha);
            stampPalette[i] = col;
            stampPaletteBleed[i] = (useBleed && bleedAtten < 255u) ? scaleColor565(col, bleedAtten) : col;
        }

        const float segDx = static_cast<float>(s.x1 - s.cx);
        const float segDy = static_cast<float>(s.y1 - s.cy);
        const float lenSq = segDx * segDx + segDy * segDy;
        const float invSegLenSq = (lenSq > 1e-4f) ? (1.0f / lenSq) : 0.0f;

        const uint16_t lampZ = s.zValue;

        for (int32_t y = yTopLocal; y <= yBotLocal; ++y)
        {
            const int32_t globalY = y + bandY0;
            uint16_t *PIP3D_RESTRICT fbRow = fbBand + y * bandWidth;
            const uint16_t *PIP3D_RESTRICT zRow = zData ? (zData + y * bandWidth) : nullptr;

            const float py = static_cast<float>(globalY - s.cy);

            for (int32_t x = xL; x <= xR; ++x)
            {
                bool isBleed = false;
                if (zRow)
                {
                    const uint16_t zbufZ = zRow[x] & Z_DEPTH_MASK;
                    if (zbufZ != 0 && zbufZ > lampZ + 6u)
                    {
                        if (innerPass || !useBleed || bleedAtten == 0u)
                            continue;
                        isBleed = true;
                    }
                }

                const float px = static_cast<float>(x - s.cx);
                float t = (px * segDx + py * segDy) * invSegLenSq;
                if (t < 0.0f)
                    t = 0.0f;
                else if (t > 1.0f)
                    t = 1.0f;

                const float projX = static_cast<float>(s.cx) + t * segDx;
                const float projY = static_cast<float>(s.cy) + t * segDy;
                const float dx = static_cast<float>(x) - projX;
                const float dy = static_cast<float>(globalY) - projY;

                const uint32_t q16 = static_cast<uint32_t>(dx * dx * Kx + dy * dy * Ky);
                const uint32_t idx = q16 >> 16;
                if (idx >= GLOW_LUT_SIZE)
                    continue;

                const uint16_t addColor = isBleed ? stampPaletteBleed[idx] : stampPalette[idx];
                if (!addColor)
                    continue;

                fbRow[x] = fastAdd565Saturate(fbRow[x], addColor);
            }
        }
    }

    inline void IRAM_ATTR Glow::drawStampPassRect(
        const GlowStamp &s,
        uint16_t *PIP3D_RESTRICT fbBand,
        uint16_t bandWidth,
        uint16_t bandHeight,
        int16_t bandY0,
        const uint16_t *PIP3D_RESTRICT zData,
        bool innerPass) const noexcept
    {
        const int32_t rx = innerPass ? s.innerRadiusX : s.outerRadiusX;
        const int32_t ry = innerPass ? s.innerRadiusY : s.outerRadiusY;
        if (rx <= 0 || ry <= 0)
            return;

        int32_t yTopLocal = s.cy - ry - bandY0;
        int32_t yBotLocal = s.cy + ry - bandY0;
        if (yBotLocal < 0 || yTopLocal >= bandHeight)
            return;

        if (yTopLocal < 0)
            yTopLocal = 0;
        if (yBotLocal >= bandHeight)
            yBotLocal = bandHeight - 1;

        int32_t xL = s.cx - rx;
        int32_t xR = s.cx + rx;
        if (xR < 0 || xL >= bandWidth)
            return;
        if (xL < 0)
            xL = 0;
        if (xR >= bandWidth)
            xR = bandWidth - 1;

        const float passGain = innerPass ? 1.0f : settings_.outerHaloGain;
        const uint8_t passIntensity = static_cast<uint8_t>(
            (static_cast<uint32_t>(s.intensity) *
             static_cast<uint32_t>(clamp(settings_.intensity * passGain, 0.0f, 2.0f) * 127.5f)) >>
            7);

        if (passIntensity == 0)
            return;

        const uint16_t passColor = innerPass ? s.coreColor565 : s.haloColor565;
        const uint8_t *PIP3D_RESTRICT lut = innerPass ? radialLutInner_ : radialLutOuter_;

        alignas(16) uint16_t stampPalette[GLOW_LUT_SIZE];
        alignas(16) uint16_t stampPaletteBleed[GLOW_LUT_SIZE];

        const uint32_t Kx = (31u * 65536u) / static_cast<uint32_t>(rx * rx);
        const uint32_t Ky = (31u * 65536u) / static_cast<uint32_t>(ry * ry);

        const bool useBleed = (!innerPass) && settings_.softDepthOcclusion && (settings_.bleedMaxFrac > 0.001f);
        const uint8_t bleedFull = useBleed ? static_cast<uint8_t>(clamp(settings_.bleedMaxFrac * 255.0f, 0.0f, 255.0f) + 0.5f) : 0u;
        const uint8_t stampVis = s.visibility;
        const uint8_t bleedAtten = static_cast<uint8_t>((static_cast<uint32_t>(stampVis) * static_cast<uint32_t>(bleedFull)) >> 8);

        for (uint32_t i = 0; i < GLOW_LUT_SIZE; ++i)
        {
            const uint8_t alpha = (static_cast<uint32_t>(lut[i]) * passIntensity) >> 8;
            const uint16_t col = scaleColor565(passColor, alpha);
            stampPalette[i] = col;
            stampPaletteBleed[i] = (useBleed && bleedAtten < 255u) ? scaleColor565(col, bleedAtten) : col;
        }

        alignas(16) uint32_t xTermFixed[256];
        const int32_t maxDx = (rx > 255) ? 255 : rx;
        for (int32_t dx = 0; dx <= maxDx; ++dx)
        {
            xTermFixed[dx] = static_cast<uint32_t>(dx * dx) * Kx;
        }

        const bool hasRotation = (fabsf(s.cosRot - 1.0f) > 0.001f || fabsf(s.sinRot) > 0.001f);
        const int32_t halfInnerX = innerPass ? (s.innerRadiusX >> 1) : 0;
        const int32_t halfInnerY = innerPass ? (s.innerRadiusY >> 1) : 0;
        const uint16_t lampZ = s.zValue;

        for (int32_t y = yTopLocal; y <= yBotLocal; ++y)
        {
            const int32_t globalY = y + bandY0;
            uint16_t *PIP3D_RESTRICT fbRow = fbBand + y * bandWidth;
            const uint16_t *PIP3D_RESTRICT zRow = zData ? (zData + y * bandWidth) : nullptr;

            const float fdy = static_cast<float>(globalY - s.cy);

            for (int32_t x = xL; x <= xR; ++x)
            {
                bool isBleed = false;
                if (zRow)
                {
                    const uint16_t zbufZ = zRow[x] & Z_DEPTH_MASK;
                    if (zbufZ != 0 && zbufZ > lampZ + 6u)
                    {
                        if (innerPass || !useBleed || bleedAtten == 0u)
                            continue;
                        isBleed = true;
                    }
                }

                const float fdx = static_cast<float>(x - s.cx);
                float lx = fdx;
                float ly = fdy;
                if (hasRotation)
                {
                    lx = fdx * s.cosRot - fdy * s.sinRot;
                    ly = fdx * s.sinRot + fdy * s.cosRot;
                }

                int32_t dx = static_cast<int32_t>(fabsf(lx)) - halfInnerX;
                int32_t dy = static_cast<int32_t>(fabsf(ly)) - halfInnerY;
                if (dx < 0)
                    dx = 0;
                if (dy < 0)
                    dy = 0;

                if (dx > maxDx)
                    continue;

                const uint32_t Qy = static_cast<uint32_t>(dy * dy) * Ky;
                const uint32_t q16 = Qy + xTermFixed[dx];
                const uint32_t idx = q16 >> 16;
                if (idx >= GLOW_LUT_SIZE)
                    continue;

                const uint16_t addColor = isBleed ? stampPaletteBleed[idx] : stampPalette[idx];
                if (!addColor)
                    continue;

                fbRow[x] = fastAdd565Saturate(fbRow[x], addColor);
            }
        }
    }

    inline void IRAM_ATTR Glow::compositeStamps(
        uint16_t *PIP3D_RESTRICT fbBand,
        uint16_t bandWidth,
        uint16_t bandHeight,
        int16_t bandY0,
        const ZBuffer *zBuf) noexcept
    {
        if (!initialized_ || !settings_.enabled || stampCount_ == 0 || !fbBand)
            return;

        const uint16_t *zData = zBuf ? zBuf->data() : nullptr;

        if (zData && settings_.softDepthOcclusion)
        {
            for (uint16_t i = 0; i < stampCount_; ++i)
            {
                GlowStamp &s = stamps_[i];
                const int32_t ry = s.outerRadiusY > 0 ? s.outerRadiusY : 4;
                if (s.cy + ry >= bandY0 && s.cy - ry < bandY0 + bandHeight)
                {
                    s.visibility = computeStampVisibility(
                        s, zData, bandWidth, bandHeight, bandY0);
                }
            }
        }

        for (uint16_t i = 0; i < stampCount_; ++i)
        {
            drawStampPass(stamps_[i], fbBand, bandWidth, bandHeight, bandY0, zData, false);
        }

        for (uint16_t i = 0; i < stampCount_; ++i)
        {
            drawStampPass(stamps_[i], fbBand, bandWidth, bandHeight, bandY0, zData, true);
        }
    }
}