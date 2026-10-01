#pragma once

#include <cstdint>

#include "Core/Platform.hpp"

#if PIP3D_SIMD

namespace pip3D
{
    namespace Simd
    {

#define FRAG_CTX_LOAD(reg, off) \
    "l32i " reg ", %[ctx], " #off "\n"

#define FRAG_D_PACK(qout, qlo, qhi)   \
    "ee.vsr.32  " #qlo ", " #qlo "\n" \
    "ee.vsr.32  " #qhi ", " #qhi "\n" \
    "ee.vunzip.16 " #qout ", " #qhi "\n"

#define FRAG_ZTEST(qmask, qd, qzb, qmask7f)               \
    "ee.andq        " #qmask ", " #qzb ", " #qmask7f "\n" \
    "ee.vcmp.gt.s16 " #qmask ", " #qd ", " #qmask "\n"

#define FRAG_MERGE(qdst, qsrc, qmask, qtmp)             \
    "ee.xorq        " #qtmp ", " #qdst ", " #qsrc "\n"  \
    "ee.andq        " #qtmp ", " #qtmp ", " #qmask "\n" \
    "ee.xorq        " #qdst ", " #qdst ", " #qtmp "\n"

#define FRAG_LOOP_NEXT(label)          \
    "addi            %[n], %[n], -1\n" \
    "bnez            %[n], " #label "b\n"

        PIP3D_FORCE_INLINE static void IRAM_ATTR fillSpan16(
            uint16_t *__restrict__ dst,
            uint32_t n8,
            uint16_t value) noexcept
        {
            alignas(16) uint16_t v16 = value;
            void *ctx[2] = {dst, &v16};
            uint32_t n = n8;

            asm volatile(
                "l32i a6,  %[ctx], 0\n"
                "l32i a7,  %[ctx], 4\n"
                "ee.vldbc.16   q0, a7\n"
                "0:\n"
                "ee.vst.128.ip q0, a6, 16\n"
                "addi          %[n], %[n], -1\n"
                "bnez          %[n], 0b\n"
                : [n] "+&r"(n)
                : [ctx] "r"(ctx)
                : "memory", "a6", "a7");
        }

        PIP3D_FORCE_INLINE static int16_t IRAM_ATTR
        scanSpanMaxZ(const uint16_t *__restrict__ zb, uint32_t n8) noexcept
        {
            alignas(16) uint16_t mask16 = 0x7FFF;
            alignas(16) uint16_t out[8] = {};
            void *ctx[3] = {const_cast<uint16_t *>(zb), &mask16, out};
            uint32_t n = n8;
            asm volatile(
                "l32i a6,  %[ctx], 0\n"
                "l32i a7,  %[ctx], 4\n"
                "l32i a8,  %[ctx], 8\n"
                "ee.vldbc.16.ip q5, a7, 0\n"
                "ee.vld.128.ip  q1, a6, 16\n"
                "ee.andq        q1, q1, q5\n"
                "addi %[n], %[n], -1\n"
                "beqz %[n], 2f\n"
                "1:\n"
                "ee.vld.128.ip  q0, a6, 16\n"
                "ee.andq        q0, q0, q5\n"
                "ee.vmax.s16    q1, q1, q0\n"
                "addi           %[n], %[n], -1\n"
                "bnez           %[n], 1b\n"
                "2:\n"
                "ee.vst.128.ip  q1, a8, 16\n"
                : [n] "+&r"(n)
                : [ctx] "r"(ctx)
                : "memory", "a6", "a7", "a8");

            int16_t m = out[0];
            for (uint32_t k = 1; k < 8; ++k)
                if (out[k] > m)
                    m = out[k];
            return m;
        }

        PIP3D_FORCE_INLINE static void IRAM_ATTR
        fillSpanZ8Pass(uint16_t *__restrict__ zb, uint16_t *__restrict__ fb,
                       uint32_t n8, int32_t depthFirst, int32_t dzFixed,
                       uint16_t color) noexcept
        {
            alignas(16) int32_t ramp[4] = {
                depthFirst, depthFirst + dzFixed,
                depthFirst + 2 * dzFixed, depthFirst + 3 * dzFixed};
            alignas(16) int32_t dz4 = dzFixed * 4;
            alignas(16) int32_t dz8 = dzFixed * 8;
            alignas(16) uint16_t color16 = color;
            struct SpanCtx
            {
                uint16_t *zb, *fb;
                const int32_t *ramp;
                const int32_t *dz4, *dz8;
                const uint16_t *col;
            };
            SpanCtx ctx = {zb, fb, ramp, &dz4, &dz8, &color16};
            uint32_t n = n8;

            asm volatile(
                FRAG_CTX_LOAD("a6", 0)
                FRAG_CTX_LOAD("a7", 4)
                FRAG_CTX_LOAD("a10", 8)
                FRAG_CTX_LOAD("a11", 12)
                FRAG_CTX_LOAD("a12", 16)
                FRAG_CTX_LOAD("a13", 20)
                "wsr.sar %[sar]\n"
                "ee.vld.128.ip   q2, a10, 0\n"
                "ee.vldbc.32.ip  q4, a12, 0\n"
                "ee.vldbc.32.ip  q5, a11, 0\n"
                "ee.vldbc.16.ip  q7, a13, 0\n"
                "ee.vadds.s32    q3, q2, q5\n"
                "0:\n"
                "ee.vsr.32       q6, q2\n"
                "ee.vsr.32       q1, q3\n"
                "ee.vunzip.16    q6, q1\n"
                "ee.vadds.s32    q2, q2, q4\n"
                "ee.vadds.s32    q3, q2, q5\n"
                "ee.vst.128.ip   q6, a6, 16\n"
                "ee.vst.128.ip   q7, a7, 16\n"
                FRAG_LOOP_NEXT(0)
                : [n] "+&r"(n)
                : [ctx] "r"(&ctx), [sar] "r"(14)
                : "memory", "a6", "a7", "a10", "a11", "a12", "a13");
        }

        PIP3D_FORCE_INLINE static void IRAM_ATTR
        fillSpanZ8(uint16_t *__restrict__ zb, uint16_t *__restrict__ fb,
                   uint32_t n8, int32_t depthFirst, int32_t dzFixed,
                   uint16_t color) noexcept
        {
            alignas(16) int32_t ramp[4] = {
                depthFirst, depthFirst + dzFixed,
                depthFirst + 2 * dzFixed, depthFirst + 3 * dzFixed};
            alignas(16) int32_t dz4 = dzFixed * 4;
            alignas(16) uint16_t color16 = color;
            alignas(16) uint16_t depthMask = 0x7FFF;

            struct SpanCtx
            {
                uint16_t *zb, *fb;
                const int32_t *ramp;
                const int32_t *dz4;
                const uint16_t *mask;
                const uint16_t *col;
            };
            SpanCtx ctx = {zb, fb, ramp, &dz4, &depthMask, &color16};
            uint32_t n = n8;

            asm volatile(
                FRAG_CTX_LOAD("a6", 0)
                FRAG_CTX_LOAD("a7", 4)
                FRAG_CTX_LOAD("a10", 8)
                FRAG_CTX_LOAD("a11", 12)
                FRAG_CTX_LOAD("a12", 16)
                FRAG_CTX_LOAD("a13", 20)
                "wsr.sar %[sar]\n"
                "ee.vld.128.ip   q2, a10, 0\n"
                "ee.vldbc.32.ip  q4, a11, 0\n"
                "ee.vldbc.16.ip  q5, a12, 0\n"
                "ee.vldbc.16.ip  q7, a13, 0\n"
                "0:\n"
                "ee.vld.128.ip   q0, a6, 0\n"
                "ee.vld.128.ip   q1, a7, 0\n"
                "ee.vadds.s32    q3, q2, q4\n"
                "ee.vsr.32       q6, q2\n"
                "ee.vsr.32       q3, q3\n"
                "ee.vunzip.16    q6, q3\n"
                "ee.vadds.s32    q2, q2, q4\n"
                "ee.vadds.s32    q2, q2, q4\n"
                FRAG_ZTEST(q3, q6, q0, q5)
                FRAG_MERGE(q0, q6, q3, q6)
                "ee.vst.128.ip   q0, a6, 16\n"
                FRAG_MERGE(q1, q7, q3, q6)
                "ee.vst.128.ip   q1, a7, 16\n"
                FRAG_LOOP_NEXT(0)
                : [n] "+&r"(n)
                : [ctx] "r"(&ctx), [sar] "r"(14)
                : "memory", "a6", "a7", "a10", "a11", "a12", "a13");
        }

        PIP3D_FORCE_INLINE static void IRAM_ATTR
        fillSkyRow128(uint16_t *__restrict__ dst, uint32_t n8,
                      uint32_t r8, uint32_t g8, uint32_t b8,
                      const uint32_t *__restrict__ dr32Row,
                      const uint32_t *__restrict__ dg32Row) noexcept
        {
            alignas(16) uint32_t r32 = r8;
            alignas(16) uint32_t g32 = g8;
            alignas(16) uint32_t b32 = b8;
            alignas(16) uint32_t c254 = 254;

            struct SkyCtx
            {
                uint16_t *dst;
                const uint32_t *dr, *dg;
                const uint32_t *r, *g, *b, *c254;
            };
            SkyCtx ctx = {dst, dr32Row, dg32Row, &r32, &g32, &b32, &c254};
            uint32_t n = n8;

            asm volatile(
                "l32i a6,  %[ctx], 0\n"
                "l32i a7,  %[ctx], 4\n"
                "l32i a8,  %[ctx], 8\n"
                "l32i a9,  %[ctx], 12\n"
                "l32i a10, %[ctx], 16\n"
                "l32i a11, %[ctx], 20\n"
                "l32i a12, %[ctx], 24\n"
                "ee.vldbc.32.ip q7, a12, 0\n"

                "ee.vld.128.ip  q0, a7, 0\n"
                "ee.vld.128.ip  q1, a8, 0\n"
                "ee.vldbc.32.ip q2, a9, 0\n"
                "ee.vadds.s32   q2, q2, q0\n"
                "ee.vmin.s32    q2, q2, q7\n"
                "wsr.sar %[s3]\n"
                "ee.vsr.32      q2, q2\n"
                "wsr.sar %[s11]\n"
                "ee.vsl.32      q2, q2\n"
                "ee.vldbc.32.ip q3, a11, 0\n"
                "ee.vadds.s32   q3, q3, q0\n"
                "ee.vmin.s32    q3, q3, q7\n"
                "wsr.sar %[s3]\n"
                "ee.vsr.32      q3, q3\n"
                "ee.vldbc.32.ip q4, a10, 0\n"
                "ee.vadds.s32   q4, q4, q1\n"
                "ee.vmin.s32    q4, q4, q7\n"
                "wsr.sar %[s2]\n"
                "ee.vsr.32      q4, q4\n"
                "wsr.sar %[s5]\n"
                "ee.vsl.32      q4, q4\n"
                "ee.orq         q5, q2, q4\n"
                "ee.orq         q5, q5, q3\n"
                "ee.vunzip.16   q5, q2\n"

                "ee.vld.128.ip  q0, a7, 16\n"
                "ee.vld.128.ip  q1, a8, 16\n"
                "ee.vldbc.32.ip q2, a9, 0\n"
                "ee.vadds.s32   q2, q2, q0\n"
                "ee.vmin.s32    q2, q2, q7\n"
                "wsr.sar %[s3]\n"
                "ee.vsr.32      q2, q2\n"
                "wsr.sar %[s11]\n"
                "ee.vsl.32      q2, q2\n"
                "ee.vldbc.32.ip q3, a11, 0\n"
                "ee.vadds.s32   q3, q3, q0\n"
                "ee.vmin.s32    q3, q3, q7\n"
                "wsr.sar %[s3]\n"
                "ee.vsr.32      q3, q3\n"
                "ee.vldbc.32.ip q4, a10, 0\n"
                "ee.vadds.s32   q4, q4, q1\n"
                "ee.vmin.s32    q4, q4, q7\n"
                "wsr.sar %[s2]\n"
                "ee.vsr.32      q4, q4\n"
                "wsr.sar %[s5]\n"
                "ee.vsl.32      q4, q4\n"
                "ee.orq         q6, q2, q4\n"
                "ee.orq         q6, q6, q3\n"
                "ee.vunzip.16   q6, q2\n"
                "0:\n"
                "ee.vst.l.64.ip q5, a6, 8\n"
                "ee.vst.l.64.ip q6, a6, 8\n"
                "addi           %[n], %[n], -1\n"
                "bnez           %[n], 0b\n"
                : [n] "+&r"(n)
                : [ctx] "r"(&ctx), [s3] "r"(3), [s11] "r"(11),
                  [s2] "r"(2), [s5] "r"(5)
                : "memory", "a6", "a7", "a8", "a9", "a10", "a11", "a12");
        }

    }
}

#else

namespace pip3D
{
    namespace Simd
    {
        inline void fillSpan16(uint16_t *dst, uint32_t n8, uint16_t value) noexcept
        {
            const uint32_t count = n8 * 8;
            for (uint32_t i = 0; i < count; ++i)
                dst[i] = value;
        }

        inline int16_t scanSpanMaxZ(const uint16_t *__restrict__ zb,
                                    uint32_t n8) noexcept
        {
            int16_t m = 0;
            const uint32_t count = n8 * 8;
            for (uint32_t i = 0; i < count; ++i)
            {
                const int16_t c = static_cast<int16_t>(zb[i] & 0x7FFFu);
                if (c > m)
                    m = c;
            }
            return m;
        }

        inline void fillSpanZ8Pass(uint16_t *zb, uint16_t *fb, uint32_t n8,
                                   int32_t depthFirst, int32_t dzFixed,
                                   uint16_t color) noexcept
        {
            int32_t depth = depthFirst;
            for (uint32_t i = 0; i < n8 * 8; ++i)
            {
                zb[i] = static_cast<uint16_t>(depth >> 14);
                fb[i] = color;
                depth += dzFixed;
            }
        }

        inline void fillSpanZ8(uint16_t *zb, uint16_t *fb, uint32_t n8,
                               int32_t depthFirst, int32_t dzFixed,
                               uint16_t color) noexcept
        {
            int32_t depth = depthFirst;
            for (uint32_t i = 0; i < n8 * 8; ++i)
            {
                const uint16_t d = static_cast<uint16_t>(depth >> 14);
                if (d > (zb[i] & 0x7FFF))
                {
                    zb[i] = d;
                    fb[i] = color;
                }
                depth += dzFixed;
            }
        }

        inline void fillSkyRow128(uint16_t *dst, uint32_t n8,
                                  uint32_t r8, uint32_t g8, uint32_t b8,
                                  const uint32_t *dr32Row,
                                  const uint32_t *dg32Row) noexcept
        {
            const uint32_t count = n8 * 8;
            for (uint32_t i = 0; i < count; ++i)
            {
                const uint32_t dr = dr32Row[i & 7u];
                const uint32_t dg = dg32Row[i & 7u];
                uint32_t r = (r8 + dr) >> 3;
                uint32_t g = (g8 + dg) >> 2;
                uint32_t b = (b8 + dr) >> 3;
                if (r > 31u)
                    r = 31u;
                if (g > 63u)
                    g = 63u;
                if (b > 31u)
                    b = 31u;
                dst[i] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
            }
        }

    }
}

#endif
