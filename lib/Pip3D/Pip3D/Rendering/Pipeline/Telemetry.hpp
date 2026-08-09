#pragma once

#include <stdint.h>
#include "Math/Algebra.hpp"
#include "Debug/Flags.hpp"

namespace pip3D
{

    enum class SkipReason : uint8_t
    {
        NONE = 0,
        NEAR_FULLY = 1,
        BACKFACE = 2,
        DEGENERATE = 3,
        BAND_Y = 4,
        FRUSTUM_X = 5,
        CLIP_OUTCOUNT_LT3 = 6,
        NAN_PROJECT = 7,
    };

#if PIP3D_ENABLE_DRAW_TELEMETRY

    struct DrawTelemetry
    {
        uint32_t facesTotal;
        uint32_t facesNearFully;
        uint32_t facesBackface;
        uint32_t facesDegenerate;
        uint32_t facesBandY;
        uint32_t facesFrustumX;
        uint32_t facesClipOutcountLt3;
        uint32_t facesNanProject;
        uint32_t facesDrawnTextured;
        uint32_t facesDrawnClipped;
        uint32_t facesDrawnFlat;

        SkipReason lastSkipReason;
        uint32_t lastSkipFrame;
        uint16_t lastSkipFaceIdx;
        uint16_t lastSkipMeshId;
        float lastSkipD0, lastSkipD1, lastSkipD2;
        float lastSkipArea;
        float lastSkipP0x, lastSkipP0y, lastSkipP0z;
        float lastSkipP1x, lastSkipP1y;
        float lastSkipP2x, lastSkipP2y;
        float lastSkipV0x, lastSkipV0y, lastSkipV0z;
        float lastSkipCamX, lastSkipCamY, lastSkipCamZ;
        float lastSkipFwdX, lastSkipFwdY, lastSkipFwdZ;
        float lastSkipNear;
        int16_t lastSkipBandTop, lastSkipBandBottom;
        bool lastSkipPartiallyClipped;
        bool lastSkipTextured;

        void resetFrame()
        {
            facesTotal = 0;
            facesNearFully = 0;
            facesBackface = 0;
            facesDegenerate = 0;
            facesBandY = 0;
            facesFrustumX = 0;
            facesClipOutcountLt3 = 0;
            facesNanProject = 0;
            facesDrawnTextured = 0;
            facesDrawnClipped = 0;
            facesDrawnFlat = 0;
            lastSkipReason = SkipReason::NONE;
            lastSkipFrame = 0;
            lastSkipFaceIdx = 0;
            lastSkipMeshId = 0;
            lastSkipD0 = lastSkipD1 = lastSkipD2 = 0.0f;
            lastSkipArea = 0.0f;
            lastSkipP0x = lastSkipP0y = lastSkipP0z = 0.0f;
            lastSkipP1x = lastSkipP1y = 0.0f;
            lastSkipP2x = lastSkipP2y = 0.0f;
            lastSkipV0x = lastSkipV0y = lastSkipV0z = 0.0f;
            lastSkipCamX = lastSkipCamY = lastSkipCamZ = 0.0f;
            lastSkipFwdX = lastSkipFwdY = lastSkipFwdZ = 0.0f;
            lastSkipNear = 0.0f;
            lastSkipBandTop = 0;
            lastSkipBandBottom = 0;
            lastSkipPartiallyClipped = false;
            lastSkipTextured = false;
        }

        void recordSkip(SkipReason r,
                        uint32_t frame,
                        uint16_t faceIdx,
                        const void *meshPtr,
                        float d0, float d1, float d2,
                        float area,
                        float p0x, float p0y, float p0z,
                        float p1x, float p1y,
                        float p2x, float p2y,
                        float v0x, float v0y, float v0z,
                        float camX, float camY, float camZ,
                        float fwdX, float fwdY, float fwdZ,
                        float nearP,
                        int16_t bandTop, int16_t bandBottom,
                        bool partial, bool textured)
        {
            switch (r)
            {
            case SkipReason::NEAR_FULLY:
                facesNearFully++;
                break;
            case SkipReason::BACKFACE:
                facesBackface++;
                break;
            case SkipReason::DEGENERATE:
                facesDegenerate++;
                break;
            case SkipReason::BAND_Y:
                facesBandY++;
                break;
            case SkipReason::FRUSTUM_X:
                facesFrustumX++;
                break;
            case SkipReason::CLIP_OUTCOUNT_LT3:
                facesClipOutcountLt3++;
                break;
            case SkipReason::NAN_PROJECT:
                facesNanProject++;
                break;
            default:
                break;
            }

            lastSkipReason = r;
            lastSkipFrame = frame;
            lastSkipFaceIdx = faceIdx;
            lastSkipMeshId = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(meshPtr) & 0xFFFFu);
            lastSkipD0 = d0;
            lastSkipD1 = d1;
            lastSkipD2 = d2;
            lastSkipArea = area;
            lastSkipP0x = p0x;
            lastSkipP0y = p0y;
            lastSkipP0z = p0z;
            lastSkipP1x = p1x;
            lastSkipP1y = p1y;
            lastSkipP2x = p2x;
            lastSkipP2y = p2y;
            lastSkipV0x = v0x;
            lastSkipV0y = v0y;
            lastSkipV0z = v0z;
            lastSkipCamX = camX;
            lastSkipCamY = camY;
            lastSkipCamZ = camZ;
            lastSkipFwdX = fwdX;
            lastSkipFwdY = fwdY;
            lastSkipFwdZ = fwdZ;
            lastSkipNear = nearP;
            lastSkipBandTop = bandTop;
            lastSkipBandBottom = bandBottom;
            lastSkipPartiallyClipped = partial;
            lastSkipTextured = textured;
        }
    };

    extern DrawTelemetry g_drawTelemetry;

#else

    struct DrawTelemetry
    {
    };

    inline DrawTelemetry g_drawTelemetry{};

#endif

    struct DrawTelemetryClipVert
    {
        Vector3 pos;
        float u, v;
        float d;
        float lr, lg, lb;
    };

    inline DrawTelemetryClipVert lerpClipVert(const DrawTelemetryClipVert &a,
                                              const DrawTelemetryClipVert &b,
                                              float t)
    {
        DrawTelemetryClipVert r;
        r.pos = a.pos + (b.pos - a.pos) * t;
        r.u = a.u + (b.u - a.u) * t;
        r.v = a.v + (b.v - a.v) * t;
        r.d = a.d + (b.d - a.d) * t;
        r.lr = a.lr + (b.lr - a.lr) * t;
        r.lg = a.lg + (b.lg - a.lg) * t;
        r.lb = a.lb + (b.lb - a.lb) * t;
        return r;
    }
}
