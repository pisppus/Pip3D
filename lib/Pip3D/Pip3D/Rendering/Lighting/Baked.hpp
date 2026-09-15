#pragma once

#include <cstring>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"

namespace pip3D
{

    struct alignas(4) LMUVQuant
    {
        uint16_t u, v;
    };
    static_assert(sizeof(LMUVQuant) == 4);

    struct LMPaletteAtlas
    {
        const uint8_t *pal16 = nullptr;
        const uint8_t *indices = nullptr;
        uint16_t width = 0;
        uint16_t height = 0;
        uint16_t wBlocks = 0;
        const uint32_t *meta = nullptr;
        const uint8_t *pal8 = nullptr;
        const uint8_t *indices8 = nullptr;
        const uint8_t *rampC = nullptr;
        const uint8_t *rampIdx = nullptr;
        const uint8_t *pal32 = nullptr;
        const uint8_t *indices32 = nullptr;

        [[nodiscard]] PIP3D_FORCE_INLINE bool valid() const noexcept
        {
            return meta != nullptr && width > 0 && height > 0;
        }
    };

    enum class LMSampling : uint8_t
    {
        Nearest = 0,
        Dithered = 1
    };

    using enum LMSampling;

    PIP3D_FORCE_INLINE uint32_t lmScale5(uint32_t x) noexcept { return x + (x >> 4); }
    PIP3D_FORCE_INLINE uint32_t lmScale6(uint32_t x) noexcept { return x + (x >> 5); }

    PIP3D_FORCE_INLINE void lmScale565(uint16_t c, uint32_t &r, uint32_t &g, uint32_t &b) noexcept
    {
        r = lmScale5((c >> 11) & 0x1Fu);
        g = lmScale6((c >> 5) & 0x3Fu);
        b = lmScale5(c & 0x1Fu);
    }

    namespace detail
    {

        PIP3D_FORCE_INLINE bool lmBlockSolidColor(const LMPaletteAtlas &a, int32_t bx, int32_t by,
                                                  uint32_t &r, uint32_t &g, uint32_t &b) noexcept
        {
            const uint32_t m = a.meta[static_cast<uint32_t>(by) * a.wBlocks + static_cast<uint32_t>(bx)];
            if ((m >> 16) != 0x8000u)
                return false;
            lmScale565(static_cast<uint16_t>(m & 0xFFFFu), r, g, b);
            return true;
        }

        PIP3D_FORCE_INLINE bool lmRectSolidColor(const LMPaletteAtlas &a, int32_t bx0, int32_t bx1,
                                                 int32_t by0, int32_t by1,
                                                 uint32_t &r, uint32_t &g, uint32_t &b) noexcept
        {
            uint32_t r0, g0, b0;
            if (!lmBlockSolidColor(a, bx0, by0, r0, g0, b0))
                return false;
            for (int32_t by = by0; by <= by1; ++by)
            {
                for (int32_t bx = bx0; bx <= bx1; ++bx)
                {
                    uint32_t tr, tg, tb;
                    if (!lmBlockSolidColor(a, bx, by, tr, tg, tb) ||
                        tr != r0 || tg != g0 || tb != b0)
                        return false;
                }
            }
            r = r0;
            g = g0;
            b = b0;
            return true;
        }

        PIP3D_FORCE_INLINE uint32_t lmIdx3(const uint8_t *idx, uint32_t i) noexcept
        {
            const uint32_t bitPos = i * 3u;
            const uint32_t bytePos = bitPos >> 3;
            const uint32_t shift = bitPos & 7u;
            if (shift <= 5u)
                return (idx[bytePos] >> shift) & 7u;
            return ((idx[bytePos] | (static_cast<uint32_t>(idx[bytePos + 1]) << 8)) >> shift) & 7u;
        }

        PIP3D_FORCE_INLINE uint32_t lmIdx5(const uint8_t *idx, uint32_t i) noexcept
        {
            const uint32_t bitPos = i * 5u;
            const uint32_t bytePos = bitPos >> 3;
            const uint32_t shift = bitPos & 7u;
            if (shift <= 3u)
                return (idx[bytePos] >> shift) & 31u;
            return ((idx[bytePos] | (static_cast<uint32_t>(idx[bytePos + 1]) << 8)) >> shift) & 31u;
        }

        struct LMBlockSlot
        {
            uint32_t block = 0xFFFFFFFFu;
            uint8_t type = 0;
            uint8_t data[136];
        };

        struct LMTexelMemo
        {
            const LMPaletteAtlas *atlas = nullptr;
            uint32_t lastBlock = 0xFFFFFFFFu;
            uint8_t type = 0;
            const uint8_t *idx = nullptr;
            const uint8_t *palR = nullptr;
            const uint8_t *palG = nullptr;
            const uint8_t *palB = nullptr;
            uint8_t palStride = 3;
            uint8_t curSlot = 0;

            LMBlockSlot slots[2];

            PIP3D_FORCE_INLINE void reset(const LMPaletteAtlas &a) noexcept
            {
                atlas = &a;
                lastBlock = 0xFFFFFFFFu;
                type = 0;
                idx = nullptr;
                palR = palG = palB = nullptr;
                palStride = 3;
                curSlot = 0;
                slots[0].block = 0xFFFFFFFFu;
                slots[1].block = 0xFFFFFFFFu;
            }
        };

        PIP3D_FORCE_INLINE void lmFetchBlock(LMTexelMemo &st, uint32_t block) noexcept
        {
            st.lastBlock = block;

            if (st.slots[st.curSlot].block != block)
            {
                if (st.slots[st.curSlot ^ 1].block == block)
                    st.curSlot ^= 1;
                else
                {

                    st.curSlot ^= 1;
                    LMBlockSlot &slot = st.slots[st.curSlot];
                    const LMPaletteAtlas &a = *st.atlas;
                    const uint32_t m = a.meta[block];
                    const uint16_t d = static_cast<uint16_t>(m >> 16);
                    const uint32_t blk = m & 0xFFFFu;
                    slot.block = block;

                    if (d == 0x8000u)
                    {

                        slot.type = 1;
                        uint32_t r, g, b;
                        lmScale565(static_cast<uint16_t>(blk), r, g, b);
                        slot.data[0] = static_cast<uint8_t>(r);
                        slot.data[1] = static_cast<uint8_t>(g);
                        slot.data[2] = static_cast<uint8_t>(b);
                    }
                    else if (d == 0xC000u)
                    {

                        slot.type = 2;
                        const uint8_t *rc = a.rampC + blk * 6;
                        const int32_t r0 = rc[0], g0 = rc[1], b0 = rc[2];
                        const int32_t r1 = rc[3], g1 = rc[4], b1 = rc[5];
                        const int32_t kr = ((r1 - r0 < 0 ? r0 - r1 : r1 - r0) * 18724);
                        const int32_t kg = ((g1 - g0 < 0 ? g0 - g1 : g1 - g0) * 18724);
                        const int32_t kb = ((b1 - b0 < 0 ? b0 - b1 : b1 - b0) * 18724);
                        for (uint32_t t = 0; t < 8; ++t)
                        {
                            const int32_t mr = (kr * t + 65536) >> 17;
                            const int32_t mg = (kg * t + 65536) >> 17;
                            const int32_t mb = (kb * t + 65536) >> 17;
                            const int32_t rr = r1 < r0 ? r0 - mr : r0 + mr;
                            const int32_t gg = g1 < g0 ? g0 - mg : g0 + mg;
                            const int32_t bb = b1 < b0 ? b0 - mb : b0 + mb;
                            slot.data[t * 3 + 0] = static_cast<uint8_t>(rr + (rr >> 4));
                            slot.data[t * 3 + 1] = static_cast<uint8_t>(gg + (gg >> 5));
                            slot.data[t * 3 + 2] = static_cast<uint8_t>(bb + (bb >> 4));
                        }
                        std::memcpy(slot.data + 24, a.rampIdx + blk * 24, 24);
                    }
                    else if (d == 0x4000u)
                    {
                        slot.type = 3;
                        std::memcpy(slot.data, a.pal8 + (blk << 3) * 3, 24);
                        std::memcpy(slot.data + 24, a.indices8 + blk * 24, 24);
                    }
                    else if (d == 0x2000u)
                    {
                        slot.type = 4;
                        std::memcpy(slot.data, a.pal32 + (blk << 5) * 3, 96);
                        std::memcpy(slot.data + 96, a.indices32 + blk * 40, 40);
                    }
                    else
                    {
                        slot.type = 0;
                        std::memcpy(slot.data, a.pal16 + (blk << 4) * 3, 48);
                        std::memcpy(slot.data + 48, a.indices + (blk << 5), 32);
                    }
                }
            }

            const LMBlockSlot &cur = st.slots[st.curSlot];
            st.type = cur.type;
            st.palR = cur.data;
            st.palG = st.palR + 1;
            st.palB = st.palR + 2;
            st.palStride = 3;
            st.idx = cur.data + (st.type == 4 ? 96u : st.type == 0 ? 48u
                                                                   : 24u);
        }

        PIP3D_FORCE_INLINE void lmDecodeTexelC(const LMTexelMemo &st, uint32_t x, uint32_t y,
                                               uint32_t &r, uint32_t &g, uint32_t &b) noexcept
        {
            if (st.type == 1)
            {
                r = st.palR[0];
                g = st.palG[0];
                b = st.palB[0];
                return;
            }
            const uint32_t i = ((y & 7) << 3) | (x & 7);
            uint32_t t;
            if (st.type == 4)
                t = lmIdx5(st.idx, i);
            else if (st.type == 0)
            {
                const uint8_t bq = st.idx[i >> 1];
                t = (i & 1) ? (bq >> 4) : (bq & 0x0F);
            }
            else
                t = lmIdx3(st.idx, i);
            const uint32_t ti = t * st.palStride;
            r = st.palR[ti];
            g = st.palG[ti];
            b = st.palB[ti];
        }

        PIP3D_FORCE_INLINE void lmTexelAtC(LMTexelMemo &st, int32_t x, int32_t y,
                                           uint32_t &r, uint32_t &g, uint32_t &b) noexcept
        {
            const uint32_t block = (static_cast<uint32_t>(y) >> 3) * st.atlas->wBlocks + (static_cast<uint32_t>(x) >> 3);
            if (block != st.lastBlock)
                lmFetchBlock(st, block);
            lmDecodeTexelC(st, static_cast<uint32_t>(x), static_cast<uint32_t>(y), r, g, b);
        }

        PIP3D_FORCE_INLINE void lmSampleNearest(LMTexelMemo &st, int32_t lmX, int32_t lmY,
                                                int32_t maxXi, int32_t maxYi,
                                                uint32_t &lr, uint32_t &lg, uint32_t &lb) noexcept
        {
            int32_t x = lmX >> 16;
            int32_t y = lmY >> 16;
            if (x < 0)
                x = 0;
            else if (x > maxXi)
                x = maxXi;
            if (y < 0)
                y = 0;
            else if (y > maxYi)
                y = maxYi;
            lmTexelAtC(st, x, y, lr, lg, lb);
        }
    }

    enum class BakedLightMode : uint8_t
    {
        OFF = 0,
        FINAL = 1
    };
    using enum BakedLightMode;

    namespace ProbeConsts
    {

        constexpr float kSunScale = 0.65f;
        constexpr float kSunBase = 0.35f;
        constexpr float kSkyScaleFace = 0.35f;
        constexpr float kSkyBaseFace = 0.65f;
        PIP3D_FORCE_INLINE constexpr float sunMod(float ps) noexcept { return kSunBase + kSunScale * ps; }
        PIP3D_FORCE_INLINE constexpr float skyFaceMod(float pa) noexcept { return kSkyBaseFace + kSkyScaleFace * pa; }
    }

    struct BakedProbeGrid
    {
        Vector3 origin = Vector3(0.0f, 0.0f, 0.0f);
        float cellSize = 4.0f;
        float cellSizeY = 0.0f;
        float invCellSize = 0.25f;
        float invCellSizeY = 0.25f;
        uint16_t dimX = 0, dimY = 0, dimZ = 0;
        const uint16_t *probes = nullptr;

        Vector3 tint[4] = {Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f),
                           Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f)};

        float tintR[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float tintG[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float tintB[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float maxXf = 0.0f, maxYf = 0.0f, maxZf = 0.0f;

        PIP3D_FORCE_INLINE void rebuildInv() noexcept
        {
            invCellSize = (cellSize > 1e-6f) ? FastMath::fastReciprocal(cellSize) : 0.25f;
            const float csY = (cellSizeY > 0.1f) ? cellSizeY : cellSize;
            invCellSizeY = (csY > 1e-6f) ? FastMath::fastReciprocal(csY) : invCellSize;
            for (int k = 0; k < 4; ++k)
            {
                tintR[k] = tint[k].x;
                tintG[k] = tint[k].y;
                tintB[k] = tint[k].z;
            }
            maxXf = static_cast<float>(static_cast<int32_t>(dimX) - 1);
            maxYf = static_cast<float>(static_cast<int32_t>(dimY) - 1);
            maxZf = static_cast<float>(static_cast<int32_t>(dimZ) - 1);
        }

        [[nodiscard]] PIP3D_FORCE_INLINE bool valid() const noexcept
        {
            return probes != nullptr && dimX > 0 && dimY > 0 && dimZ > 0;
        }

        PIP3D_FORCE_INLINE uint16_t probeAt(uint16_t x, uint16_t y, uint16_t z) const noexcept
        {
            const uint32_t idx = (static_cast<uint32_t>(z) * dimY + y) * dimX + x;
            return probes[idx];
        }

        PIP3D_HOT void sample(const Vector3 &pos,
                              float &sunVis, float &skyAO, float &staticLuma,
                              uint8_t &tintIdx) const noexcept
        {

            const float fx = (pos.x - origin.x) * invCellSize;
            const float fy = (pos.y - origin.y) * invCellSizeY;
            const float fz = (pos.z - origin.z) * invCellSize;

            const float cx = clamp(fx, 0.0f, maxXf);
            const float cy = clamp(fy, 0.0f, maxYf);
            const float cz = clamp(fz, 0.0f, maxZf);

            float fade = fx < 0.0f ? -fx : fx - cx;
            float ofy = fy < 0.0f ? -fy : fy - cy;
            float ofz = fz < 0.0f ? -fz : fz - cz;
            if (ofy > fade)
                fade = ofy;
            if (ofz > fade)
                fade = ofz;
            if (fade > 1.0f)
                fade = 1.0f;

            const int32_t x0 = static_cast<int32_t>(cx);
            const int32_t y0 = static_cast<int32_t>(cy);
            const int32_t z0 = static_cast<int32_t>(cz);
            const int32_t x1 = (x0 + 1 < dimX) ? x0 + 1 : x0;
            const int32_t y1 = (y0 + 1 < dimY) ? y0 + 1 : y0;
            const int32_t z1 = (z0 + 1 < dimZ) ? z0 + 1 : z0;

            const float tx = cx - static_cast<float>(x0);
            const float ty = cy - static_cast<float>(y0);
            const float tz = cz - static_cast<float>(z0);

            uint16_t c0, c1, c2, c3;
            float w0, w1, w2, w3;
            if (tx >= ty)
            {
                if (ty >= tz)
                {
                    c0 = probeAt(x0, y0, z0);
                    c1 = probeAt(x1, y0, z0);
                    c2 = probeAt(x1, y1, z0);
                    c3 = probeAt(x1, y1, z1);
                    w0 = 1.0f - tx;
                    w1 = tx - ty;
                    w2 = ty - tz;
                    w3 = tz;
                }
                else if (tx >= tz)
                {
                    c0 = probeAt(x0, y0, z0);
                    c1 = probeAt(x1, y0, z0);
                    c2 = probeAt(x1, y0, z1);
                    c3 = probeAt(x1, y1, z1);
                    w0 = 1.0f - tx;
                    w1 = tx - tz;
                    w2 = tz - ty;
                    w3 = ty;
                }
                else
                {
                    c0 = probeAt(x0, y0, z0);
                    c1 = probeAt(x0, y0, z1);
                    c2 = probeAt(x1, y0, z1);
                    c3 = probeAt(x1, y1, z1);
                    w0 = 1.0f - tz;
                    w1 = tz - tx;
                    w2 = tx - ty;
                    w3 = ty;
                }
            }
            else
            {
                if (tx >= tz)
                {
                    c0 = probeAt(x0, y0, z0);
                    c1 = probeAt(x0, y1, z0);
                    c2 = probeAt(x1, y1, z0);
                    c3 = probeAt(x1, y1, z1);
                    w0 = 1.0f - ty;
                    w1 = ty - tx;
                    w2 = tx - tz;
                    w3 = tz;
                }
                else if (ty >= tz)
                {
                    c0 = probeAt(x0, y0, z0);
                    c1 = probeAt(x0, y1, z0);
                    c2 = probeAt(x0, y1, z1);
                    c3 = probeAt(x1, y1, z1);
                    w0 = 1.0f - ty;
                    w1 = ty - tz;
                    w2 = tz - tx;
                    w3 = tx;
                }
                else
                {
                    c0 = probeAt(x0, y0, z0);
                    c1 = probeAt(x0, y0, z1);
                    c2 = probeAt(x0, y1, z1);
                    c3 = probeAt(x1, y1, z1);
                    w0 = 1.0f - tz;
                    w1 = tz - ty;
                    w2 = ty - tx;
                    w3 = tx;
                }
            }

            if (c0 == c1 && c0 == c2 && c0 == c3)
            {
                sunVis = float((c0 >> 11) & 0x1F) * (1.0f / 31.0f);
                skyAO = float((c0 >> 6) & 0x1F) * (1.0f / 31.0f);
                staticLuma = float((c0 >> 2) & 0x0F) * (1.0f / 15.0f);
            }
            else
            {
                const float r0 = float((c0 >> 11) & 0x1F), r1 = float((c1 >> 11) & 0x1F), r2 = float((c2 >> 11) & 0x1F), r3 = float((c3 >> 11) & 0x1F);
                const float g0 = float((c0 >> 6) & 0x1F), g1 = float((c1 >> 6) & 0x1F), g2 = float((c2 >> 6) & 0x1F), g3 = float((c3 >> 6) & 0x1F);
                const float b0 = float((c0 >> 2) & 0x0F), b1 = float((c1 >> 2) & 0x0F), b2 = float((c2 >> 2) & 0x0F), b3 = float((c3 >> 2) & 0x0F);
                sunVis = (r0 * w0 + r1 * w1 + r2 * w2 + r3 * w3) * (1.0f / 31.0f);
                skyAO = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3) * (1.0f / 31.0f);
                staticLuma = (b0 * w0 + b1 * w1 + b2 * w2 + b3 * w3) * (1.0f / 15.0f);
            }

            tintIdx = static_cast<uint8_t>(c0 & 3u);

            if (fade > 0.0f)
            {
                sunVis += (1.0f - sunVis) * fade;
                skyAO += (1.0f - skyAO) * fade;
                staticLuma *= 1.0f - fade;
            }
        }

        PIP3D_FORCE_INLINE void sampleWithTint(const Vector3 &pos,
                                               float &sunVis, float &skyAO,
                                               float &staticR, float &staticG, float &staticB) const noexcept
        {
            float luma;
            uint8_t ti;
            sample(pos, sunVis, skyAO, luma, ti);
            staticR = tintR[ti] * luma;
            staticG = tintG[ti] * luma;
            staticB = tintB[ti] * luma;
        }
    };

    namespace Rasterizer
    {

        struct BakedLightState
        {
            uint8_t mode = 0;
            uint8_t lmSampling = static_cast<uint8_t>(LMSampling::Dithered);
            const BakedProbeGrid *probes = nullptr;
        };

        inline BakedLightState g_bakedState PIP3D_FAST_DATA;
    }
}
