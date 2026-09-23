#pragma once

#include "Geometry/Mesh.hpp"
#include "Math/Quant.hpp"

namespace pip3D
{
    namespace detail
    {
        static constexpr MeshChunk s_cubeChunks[1] = {
            { -32767, -32767, -32767, 32767, 32767, 32767, 0u, 0u, 8, 20, 12, 0, 0, 0, 0 },
        };

        static constexpr Face s_cubeFaces[12] = {
            { 0,1,2,0,1,2 }, { 0,3,1,0,3,1 }, { 4,5,6,4,5,6 }, { 4,7,5,4,7,5 }, { 3,7,1,8,9,10 }, { 3,5,7,8,11,9 },
            { 6,2,4,12,13,14 }, { 6,0,2,12,15,13 }, { 2,7,4,0,1,2 }, { 2,1,7,0,3,1 }, { 6,3,0,16,17,18 }, { 6,5,3,16,19,17 },
        };

        alignas(16) static constexpr uint8_t s_cubeData[168] = {
            0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0xFF,0xFE,0xFF,0x00,0x00,0xFE,0xFF,0x00,0x00,
            0x00,0x00,0x00,0x00,0xFE,0xFF,0x00,0x00,0xFE,0xFF,0x00,0x00,0xFE,0xFF,0x00,0x00,
            0xFE,0xFF,0xFE,0xFF,0x00,0x00,0x00,0x00,0xFE,0xFF,0xFE,0xFF,0xFE,0xFF,0xFE,0xFF,
            0x00,0x00,0xFF,0xFF,0x7F,0xFF,0xFF,0xFF,0x00,0x00,0x7F,0xFF,0xFF,0xFF,0xFF,0xFF,
            0x7F,0xFF,0x00,0x00,0x00,0x00,0x7F,0xFF,0x00,0x00,0xFF,0xFF,0x7F,0x7F,0xFF,0xFF,
            0x00,0x00,0x7F,0x7F,0xFF,0xFF,0xFF,0xFF,0x7F,0x7F,0x00,0x00,0x00,0x00,0x7F,0x7F,
            0x00,0x00,0x00,0x00,0xFF,0x7F,0xFF,0xFF,0xFF,0xFF,0xFF,0x7F,0xFF,0xFF,0x00,0x00,
            0xFF,0x7F,0x00,0x00,0xFF,0xFF,0xFF,0x7F,0x00,0x00,0x00,0x00,0x00,0x7F,0xFF,0xFF,
            0xFF,0xFF,0x00,0x7F,0xFF,0xFF,0x00,0x00,0x00,0x7F,0x00,0x00,0xFF,0xFF,0x00,0x7F,
            0x00,0x00,0xFF,0xFF,0x7F,0x00,0xFF,0xFF,0x00,0x00,0x7F,0x00,0xFF,0xFF,0xFF,0xFF,
            0x7F,0x00,0x00,0x00,0x00,0x00,0x7F,0x00,
        };

        static constexpr MeshChunk s_pyramidChunks[1] = {
            { -32767, -32767, -32767, 32767, 32767, 32767, 0u, 0u, 5, 13, 6, 0, 0, 0, 0 },
        };

        static constexpr Face s_pyramidFaces[6] = {
            { 0,1,2,0,1,2 }, { 0,3,1,0,1,2 }, { 0,4,3,3,4,5 }, { 0,2,4,6,7,8 }, { 2,1,3,9,10,11 }, { 2,3,4,9,11,12 },
        };

        alignas(16) static constexpr uint8_t s_pyramidData[108] = {
            0xFF,0x7F,0xFE,0xFF,0xFF,0x7F,0xFE,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0xFE,0xFF,0x00,0x00,0xFE,0xFF,0x00,0x00,0x00,0x00,0xFE,0xFF,0x00,0x80,
            0xFF,0xFF,0xAA,0xD4,0xFF,0xFF,0x00,0x00,0xAA,0xD4,0x00,0x00,0x00,0x00,0xAA,0xD4,
            0x00,0x80,0xFF,0xFF,0xAA,0x7F,0xFF,0xFF,0x00,0x00,0xAA,0x7F,0x00,0x00,0x00,0x00,
            0xAA,0x7F,0x00,0x80,0xFF,0xFF,0xAA,0x2A,0xFF,0xFF,0x00,0x00,0xAA,0x2A,0x00,0x00,
            0x00,0x00,0xAA,0x2A,0x00,0x00,0x00,0x00,0x00,0x7F,0xFF,0xFF,0x00,0x00,0x00,0x7F,
            0xFF,0xFF,0xFF,0xFF,0x00,0x7F,0x00,0x00,0xFF,0xFF,0x00,0x7F,
        };

        static constexpr MeshChunk s_octaChunks[1] = {
            { -32767, -32767, -32767, 32767, 32767, 32767, 0u, 0u, 6, 24, 8, 0, 0, 0, 0 },
        };

        static constexpr Face s_octaFaces[8] = {
            { 0,1,2,0,1,2 }, { 2,1,3,3,4,5 }, { 3,1,4,6,7,8 }, { 4,1,0,9,10,11 }, { 2,5,0,12,13,14 }, { 3,5,2,15,16,17 },
            { 4,5,3,18,19,20 }, { 0,5,4,21,22,23 },
        };

        alignas(16) static constexpr uint8_t s_octaData[180] = {
            0xFE,0xFF,0xFF,0x7F,0xFF,0x7F,0xFF,0x7F,0xFE,0xFF,0xFF,0x7F,0xFF,0x7F,0xFF,0x7F,
            0xFE,0xFF,0x00,0x00,0xFF,0x7F,0xFF,0x7F,0xFF,0x7F,0xFF,0x7F,0x00,0x00,0xFF,0x7F,
            0x00,0x00,0xFF,0x7F,0x00,0x00,0x00,0x00,0xAA,0xAA,0xFF,0xFF,0x00,0x00,0xAA,0xAA,
            0x00,0x80,0xFF,0xFF,0xAA,0xAA,0x00,0x00,0x00,0x00,0xAA,0x55,0xFF,0xFF,0x00,0x00,
            0xAA,0x55,0x00,0x80,0xFF,0xFF,0xAA,0x55,0x00,0x00,0x00,0x00,0xAA,0x2A,0xFF,0xFF,
            0x00,0x00,0xAA,0x2A,0x00,0x80,0xFF,0xFF,0xAA,0x2A,0x00,0x00,0x00,0x00,0xAA,0xD4,
            0xFF,0xFF,0x00,0x00,0xAA,0xD4,0x00,0x80,0xFF,0xFF,0xAA,0xD4,0x00,0x00,0x00,0x00,
            0x55,0xAA,0xFF,0xFF,0x00,0x00,0x55,0xAA,0x00,0x80,0xFF,0xFF,0x55,0xAA,0x00,0x00,
            0x00,0x00,0x55,0x55,0xFF,0xFF,0x00,0x00,0x55,0x55,0x00,0x80,0xFF,0xFF,0x55,0x55,
            0x00,0x00,0x00,0x00,0x55,0x2A,0xFF,0xFF,0x00,0x00,0x55,0x2A,0x00,0x80,0xFF,0xFF,
            0x55,0x2A,0x00,0x00,0x00,0x00,0x55,0xD4,0xFF,0xFF,0x00,0x00,0x55,0xD4,0x00,0x80,
            0xFF,0xFF,0x55,0xD4,
        };

    }

    class Cube : public Mesh
    {
    public:
        explicit Cube(float size = 1.0f)
            : Mesh(detail::s_cubeChunks, 1, detail::s_cubeData, detail::s_cubeFaces, 12, true, true, true)
        {
            autoScale(size);
            finalizeBounds(Vector3(0.0f, 0.0f, 0.0f), size * 0.8660254f);
            bindDeleter<Cube>();
        }
    };

    class Box : public Mesh
    {
    public:
        Box(float width = 1.0f, float height = 1.0f, float depth = 1.0f)
            : Mesh(FromInterleavedTag{}, static_cast<const Vertex *>(nullptr), 0,
                   static_cast<const Face16 *>(nullptr), 0, false, true, true)
        {
            const float size = fmaxf(fmaxf(width, height), depth);
            autoScale(size);

            const float halfSize = size * 0.5f;
            const float invHalfSize = FastMath::fastReciprocal(halfSize);
            const float qx = (width * 0.5f * invHalfSize) * 32767.0f;
            const float qy = (height * 0.5f * invHalfSize) * 32767.0f;
            const float qz = (depth * 0.5f * invHalfSize) * 32767.0f;

            const int16_t X = static_cast<int16_t>(lrintf(qx));
            const int16_t Y = static_cast<int16_t>(lrintf(qy));
            const int16_t Z = static_cast<int16_t>(lrintf(qz));

            constexpr uint16_t nNegZ = packNormalConstexpr(0.0f, 0.0f, -1.0f);
            constexpr uint16_t nPosZ = packNormalConstexpr(0.0f, 0.0f, 1.0f);
            constexpr uint16_t nPosY = packNormalConstexpr(0.0f, 1.0f, 0.0f);
            constexpr uint16_t nNegY = packNormalConstexpr(0.0f, -1.0f, 0.0f);
            constexpr uint16_t nPosX = packNormalConstexpr(1.0f, 0.0f, 0.0f);
            constexpr uint16_t nNegX = packNormalConstexpr(-1.0f, 0.0f, 0.0f);

            Vertex verts[24];
            Vertex *PIP3D_RESTRICT vPtr = verts;

            *vPtr++ = Vertex(-X, -Y, -Z, nNegZ, 0.0f, 1.0f);
            *vPtr++ = Vertex(X, -Y, -Z, nNegZ, 1.0f, 1.0f);
            *vPtr++ = Vertex(X, Y, -Z, nNegZ, 1.0f, 0.0f);
            *vPtr++ = Vertex(-X, Y, -Z, nNegZ, 0.0f, 0.0f);

            *vPtr++ = Vertex(X, -Y, Z, nPosZ, 0.0f, 1.0f);
            *vPtr++ = Vertex(-X, -Y, Z, nPosZ, 1.0f, 1.0f);
            *vPtr++ = Vertex(-X, Y, Z, nPosZ, 1.0f, 0.0f);
            *vPtr++ = Vertex(X, Y, Z, nPosZ, 0.0f, 0.0f);

            *vPtr++ = Vertex(-X, Y, -Z, nPosY, 0.0f, 0.0f);
            *vPtr++ = Vertex(X, Y, -Z, nPosY, 1.0f, 0.0f);
            *vPtr++ = Vertex(X, Y, Z, nPosY, 1.0f, 1.0f);
            *vPtr++ = Vertex(-X, Y, Z, nPosY, 0.0f, 1.0f);

            *vPtr++ = Vertex(-X, -Y, Z, nNegY, 0.0f, 0.0f);
            *vPtr++ = Vertex(X, -Y, Z, nNegY, 1.0f, 0.0f);
            *vPtr++ = Vertex(X, -Y, -Z, nNegY, 1.0f, 1.0f);
            *vPtr++ = Vertex(-X, -Y, -Z, nNegY, 0.0f, 1.0f);

            *vPtr++ = Vertex(X, -Y, -Z, nPosX, 0.0f, 1.0f);
            *vPtr++ = Vertex(X, -Y, Z, nPosX, 1.0f, 1.0f);
            *vPtr++ = Vertex(X, Y, Z, nPosX, 1.0f, 0.0f);
            *vPtr++ = Vertex(X, Y, -Z, nPosX, 0.0f, 0.0f);

            *vPtr++ = Vertex(-X, -Y, Z, nNegX, 0.0f, 1.0f);
            *vPtr++ = Vertex(-X, -Y, -Z, nNegX, 1.0f, 1.0f);
            *vPtr++ = Vertex(-X, Y, -Z, nNegX, 1.0f, 0.0f);
            *vPtr++ = Vertex(-X, Y, Z, nNegX, 0.0f, 0.0f);

            Face16 faces[12];
            Face16 *PIP3D_RESTRICT fPtr = faces;
            fPtr[0] = Face16(0, 2, 1);
            fPtr[1] = Face16(0, 3, 2);
            fPtr[2] = Face16(4, 6, 5);
            fPtr[3] = Face16(4, 7, 6);
            fPtr[4] = Face16(8, 10, 9);
            fPtr[5] = Face16(8, 11, 10);
            fPtr[6] = Face16(12, 14, 13);
            fPtr[7] = Face16(12, 15, 14);
            fPtr[8] = Face16(16, 18, 17);
            fPtr[9] = Face16(16, 19, 18);
            fPtr[10] = Face16(20, 22, 21);
            fPtr[11] = Face16(20, 23, 22);

            buildFromInterleaved(verts, 24, faces, 12, false, true, true);

            const float diagSq = width * width + height * height + depth * depth;
            const float diag = diagSq * FastMath::fastInvSqrt(diagSq);
            finalizeBounds(Vector3(0.0f, 0.0f, 0.0f), diag * 0.5f);
            bindDeleter<Box>();
        }
    };

    class Pyramid : public Mesh
    {
    public:
        explicit Pyramid(float size = 1.0f)
            : Mesh(detail::s_pyramidChunks, 1, detail::s_pyramidData, detail::s_pyramidFaces, 6, true, true, true)
        {
            autoScale(size);
            finalizeBounds(Vector3(0.0f, -size * 0.25f, 0.0f), size * 0.75f);
            bindDeleter<Pyramid>();
        }
    };

    class Octahedron : public Mesh
    {
    public:
        explicit Octahedron(float size = 1.0f)
            : Mesh(detail::s_octaChunks, 1, detail::s_octaData, detail::s_octaFaces, 8, true, true, true)
        {
            autoScale(size);
            finalizeBounds(Vector3(0.0f, 0.0f, 0.0f), size * 0.5f);
            bindDeleter<Octahedron>();
        }
    };

    class Plane : public Mesh
    {
    public:
        Plane(float width = 2.0f, float depth = 2.0f, uint8_t subdivisions = 1,
              float uvScale = 1.0f)
            : Mesh(FromInterleavedTag{}, static_cast<const Vertex *>(nullptr), 0,
                   static_cast<const Face16 *>(nullptr), 0, false, true, true)
        {
            autoScale((width > depth) ? width : depth);

            const uint8_t divs = subdivisions ? subdivisions : 1;
            const float ratioX = width / ((width > depth) ? width : depth);
            const float ratioZ = depth / ((width > depth) ? width : depth);
            const float qStartX = -ratioX * 32767.0f;
            const float qEndX = ratioX * 32767.0f;
            const float qStartZ = -ratioZ * 32767.0f;
            const float qEndZ = ratioZ * 32767.0f;
            const float stepX = (qEndX - qStartX) / divs;
            const float stepZ = (qEndZ - qStartZ) / divs;
            const float invDivs = FastMath::fastReciprocal(static_cast<float>(divs));
            const float scaleUV = invDivs * uvScale;

            constexpr uint16_t normalUpData = packNormalConstexpr(0.0f, 1.0f, 0.0f);
            constexpr uint16_t normalDownData = packNormalConstexpr(0.0f, -1.0f, 0.0f);

            const uint16_t topCount = static_cast<uint16_t>(divs + 1) * (divs + 1);
            const uint32_t vertCount = 2u * topCount;
            const uint32_t faceCount = 4u * static_cast<uint32_t>(divs) * divs;

            Vertex *verts = static_cast<Vertex *>(MemUtils::allocData(vertCount * sizeof(Vertex), 16));
            Face16 *faces = static_cast<Face16 *>(MemUtils::allocData(faceCount * sizeof(Face16), 4));
            if (unlikely(!verts || !faces))
            {
                MemUtils::freeData(verts);
                MemUtils::freeData(faces);
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Plane: alloc failed");
                return;
            }

            Vertex *PIP3D_RESTRICT vPtr = verts;
            for (int side = 0; side < 2; ++side)
            {
                const uint16_t nData = (side == 0) ? normalUpData : normalDownData;
                for (uint8_t z = 0; z <= divs; ++z)
                {
                    const float currentZ = qStartZ + static_cast<float>(z) * stepZ;
                    const int16_t qZ = (z == divs)
                                           ? static_cast<int16_t>(lrintf(qEndZ))
                                           : static_cast<int16_t>(lrintf(currentZ));
                    const float tv = static_cast<float>(z) * scaleUV;

                    for (uint8_t x = 0; x <= divs; ++x)
                    {
                        const float currentX = qStartX + static_cast<float>(x) * stepX;
                        const int16_t qX = (x == divs)
                                               ? static_cast<int16_t>(lrintf(qEndX))
                                               : static_cast<int16_t>(lrintf(currentX));
                        const float tu = static_cast<float>(x) * scaleUV;

                        *vPtr = Vertex(qX, 0, qZ, nData, tu, tv);
                        ++vPtr;
                    }
                }
            }

            Face16 *PIP3D_RESTRICT fPtr = faces;
            const uint16_t pitch = divs + 1;
            uint16_t i0 = 0;
            uint16_t i1 = pitch;
            for (uint8_t z = 0; z < divs; ++z)
            {
                for (uint8_t x = 0; x < divs; ++x)
                {
                    fPtr[0] = Face16(i0, i1, i0 + 1);
                    fPtr[1] = Face16(i0 + 1, i1, i1 + 1);
                    fPtr[2] = Face16(i0 + topCount, i0 + 1 + topCount, i1 + topCount);
                    fPtr[3] = Face16(i0 + 1 + topCount, i1 + 1 + topCount, i1 + topCount);
                    fPtr += 4;
                    ++i0;
                    ++i1;
                }
                ++i0;
                ++i1;
            }

            setSingleColorLighting(true);
            buildFromInterleaved(verts, vertCount, faces, faceCount, false, true, true);

            MemUtils::freeData(verts);
            MemUtils::freeData(faces);

            const float diagSq = width * width + depth * depth;
            const float diag = diagSq * FastMath::fastInvSqrt(diagSq);
            finalizeBounds(Vector3(0.0f, 0.0f, 0.0f), 0.5f * diag);
            bindDeleter<Plane>();
        }
    };

    class Circle : public Mesh
    {
    public:
        Circle(float radius = 1.0f, uint8_t segments = 16)
            : Mesh(FromInterleavedTag{}, static_cast<const Vertex *>(nullptr), 0,
                   static_cast<const Face16 *>(nullptr), 0, false, true, true)
        {
            autoScale(radius * 2.0f);

            const uint8_t segs = (segments == 0) ? 3 : (segments > 64 ? 64 : segments);

            const float invHalfSize = FastMath::fastReciprocal(radius);
            const float scaleR = (radius * invHalfSize) * 32767.0f;

            const uint16_t angleBinStep = 65536 / segs;
            float sinT[64], cosT[64];
            for (uint8_t j = 0; j < segs; ++j)
                FastMath::fastSinCosBin(static_cast<uint16_t>(j) * angleBinStep,
                                        sinT[j], cosT[j]);

            constexpr uint16_t normalUpData = packNormalConstexpr(0.0f, 1.0f, 0.0f);
            constexpr uint16_t normalDownData = packNormalConstexpr(0.0f, -1.0f, 0.0f);

            const uint16_t topCount = 1 + segs;

            Vertex verts[130];
            Face16 faces[128];
            Vertex *PIP3D_RESTRICT vPtr = verts;

            *vPtr = Vertex(0, 0, 0, normalUpData, 0.5f, 0.5f);
            ++vPtr;
            for (uint8_t j = 0; j < segs; ++j)
            {
                const int16_t qx = static_cast<int16_t>(lrintf(cosT[j] * scaleR));
                const int16_t qz = static_cast<int16_t>(lrintf(sinT[j] * scaleR));
                const float u = 0.5f + 0.5f * cosT[j];
                const float v = 0.5f + 0.5f * sinT[j];
                *vPtr = Vertex(qx, 0, qz, normalUpData, u, v);
                ++vPtr;
            }

            *vPtr = Vertex(0, 0, 0, normalDownData, 0.5f, 0.5f);
            ++vPtr;
            for (uint8_t j = 0; j < segs; ++j)
            {
                const int16_t qx = static_cast<int16_t>(lrintf(cosT[j] * scaleR));
                const int16_t qz = static_cast<int16_t>(lrintf(sinT[j] * scaleR));
                const float u = 0.5f + 0.5f * cosT[j];
                const float v = 0.5f + 0.5f * sinT[j];
                *vPtr = Vertex(qx, 0, qz, normalDownData, u, v);
                ++vPtr;
            }

            Face16 *PIP3D_RESTRICT fPtr = faces;
            for (uint8_t j = 0; j < segs; ++j)
            {
                const uint16_t curr = 1 + j;
                const uint16_t next = 1 + ((j + 1) % segs);
                *fPtr++ = Face16(0, next, curr);
                *fPtr++ = Face16(topCount, curr + topCount, next + topCount);
            }

            setSingleColorLighting(true);
            buildFromInterleaved(verts, static_cast<uint32_t>(vPtr - verts), faces,
                                 static_cast<uint32_t>(fPtr - faces), false, true, true);

            finalizeBounds(Vector3(0.0f, 0.0f, 0.0f), radius);
            bindDeleter<Circle>();
        }
    };

    class Cylinder : public Mesh
    {
    public:
        Cylinder(float radius = 1.0f, float height = 2.0f, uint8_t segments = 16,
                 float uvScaleU = 1.0f, float uvScaleV = 1.0f)
            : Mesh(FromInterleavedTag{}, static_cast<const Vertex *>(nullptr), 0,
                   static_cast<const Face16 *>(nullptr), 0, false, true, true)
        {
            const float size = (height > radius * 2.0f) ? height : radius * 2.0f;
            autoScale(size);

            const uint8_t segs = segments ? segments : 3;
            const float h = height * 0.5f;
            const float halfSize = size * 0.5f;
            const float invHalfSize = FastMath::fastReciprocal(halfSize);
            const float scaleR = (radius * invHalfSize) * 32767.0f;
            const float scaleH = (h * invHalfSize) * 32767.0f;
            const int16_t qH = static_cast<int16_t>(lrintf(scaleH));
            const int16_t mqH = static_cast<int16_t>(-qH);

            const uint16_t angleBinStep = 65536 / segs;
            const float invSegs = FastMath::fastReciprocal(static_cast<float>(segs));
            const float scaleU = invSegs * uvScaleU;

            constexpr uint16_t topNData = packNormalConstexpr(0.0f, 1.0f, 0.0f);
            constexpr uint16_t botNData = packNormalConstexpr(0.0f, -1.0f, 0.0f);

            Vertex verts[4 + 64 * 4];
            Face16 faces[64 * 4];

            const uint16_t sideTopStart = 0;
            const uint16_t sideBottomStart = segs + 1;
            const uint16_t topCenterIdx = 2 * segs + 2;
            const uint16_t topCapStart = 2 * segs + 3;
            const uint16_t bottomCenterIdx = 3 * segs + 3;
            const uint16_t bottomCapStart = 3 * segs + 4;

            verts[topCenterIdx] = Vertex(0, qH, 0, topNData, 0.5f, 0.5f);
            verts[bottomCenterIdx] = Vertex(0, mqH, 0, botNData, 0.5f, 0.5f);

            for (uint8_t i = 0; i <= segs; ++i)
            {
                const uint16_t angleBin = (i == segs) ? 0 : i * angleBinStep;
                float s, c;
                FastMath::fastSinCosBin(angleBin, s, c);
                const int16_t qRx = static_cast<int16_t>(lrintf(c * scaleR));
                const int16_t qRz = static_cast<int16_t>(lrintf(s * scaleR));
                const float u = static_cast<float>(i) * scaleU;

                PackedNormal sideN;
                sideN.set(c, 0.0f, s);
                const uint16_t sideNData = sideN.data;

                verts[sideTopStart + i] = Vertex(qRx, qH, qRz, sideNData, u, 0.0f);
                verts[sideBottomStart + i] = Vertex(qRx, mqH, qRz, sideNData, u, uvScaleV);

                if (i < segs)
                {
                    const float cu = 0.5f + 0.5f * c;
                    const float cv = 0.5f + 0.5f * s;
                    verts[topCapStart + i] = Vertex(qRx, qH, qRz, topNData, cu, cv);
                    verts[bottomCapStart + i] = Vertex(qRx, mqH, qRz, botNData, cu, cv);
                }
            }

            Face16 *PIP3D_RESTRICT fPtr = faces;
            for (uint8_t i = 0; i < segs; ++i)
            {
                const uint16_t next = i + 1;
                const uint16_t t1 = sideTopStart + i;
                const uint16_t t2 = sideTopStart + next;
                const uint16_t b1 = sideBottomStart + i;
                const uint16_t b2 = sideBottomStart + next;

                fPtr[0] = Face16(t1, t2, b1);
                fPtr[1] = Face16(t2, b2, b1);
                fPtr += 2;

                const uint16_t capNext = (i + 1 == segs) ? 0 : i + 1;
                fPtr[0] = Face16(topCenterIdx, topCapStart + capNext, topCapStart + i);
                fPtr[1] = Face16(bottomCenterIdx, bottomCapStart + i, bottomCapStart + capNext);
                fPtr += 2;
            }

            buildFromInterleaved(verts, static_cast<uint32_t>(4 + segs * 4), faces,
                                 static_cast<uint32_t>(segs * 4), false, true, true);

            const float diagSq = radius * radius + h * h;
            const float diag = diagSq * FastMath::fastInvSqrt(diagSq);
            finalizeBounds(Vector3(0.0f, 0.0f, 0.0f), diag);
            bindDeleter<Cylinder>();
        }
    };

    class Cone : public Mesh
    {
    public:
        Cone(float radius = 1.0f, float height = 2.0f, uint8_t segments = 16,
             float uvScaleU = 1.0f, float uvScaleV = 1.0f)
            : Mesh(FromInterleavedTag{}, static_cast<const Vertex *>(nullptr), 0,
                   static_cast<const Face16 *>(nullptr), 0, false, true, true)
        {
            const float size = (height > radius * 2.0f) ? height : radius * 2.0f;
            autoScale(size);

            const uint8_t segs = segments ? segments : 3;
            const float h = height * 0.5f;
            const float halfSize = size * 0.5f;
            const float invHalfSize = FastMath::fastReciprocal(halfSize);
            const float scaleR = (radius * invHalfSize) * 32767.0f;
            const float scaleH = (h * invHalfSize) * 32767.0f;
            const int16_t qH = static_cast<int16_t>(lrintf(scaleH));
            const int16_t mqH = static_cast<int16_t>(-qH);

            const uint16_t angleBinStep = 65536 / segs;
            const float invSegs = FastMath::fastReciprocal(static_cast<float>(segs));
            const float scaleU = invSegs * uvScaleU;

            constexpr uint16_t botNData = packNormalConstexpr(0.0f, -1.0f, 0.0f);

            Vertex verts[3 + 64 * 3];
            Face16 faces[64 * 2];

            const uint16_t sideApexStart = 0;
            const uint16_t sideBaseStart = segs + 1;
            const uint16_t baseCenterIdx = 2 * segs + 2;
            const uint16_t bottomCapStart = 2 * segs + 3;

            verts[baseCenterIdx] = Vertex(0, mqH, 0, botNData, 0.5f, 0.5f);

            for (uint8_t i = 0; i <= segs; ++i)
            {
                const uint16_t angleBin = (i == segs) ? 0 : i * angleBinStep;
                float s, c;
                FastMath::fastSinCosBin(angleBin, s, c);
                const int16_t qRx = static_cast<int16_t>(lrintf(c * scaleR));
                const int16_t qRz = static_cast<int16_t>(lrintf(s * scaleR));
                const float u = static_cast<float>(i) * scaleU;
                PackedNormal sideN;
                sideN.set(height * c, radius, height * s);
                const uint16_t sideNData = sideN.data;

                verts[sideApexStart + i] = Vertex(0, qH, 0, sideNData, u, 0.0f);
                verts[sideBaseStart + i] = Vertex(qRx, mqH, qRz, sideNData, u, uvScaleV);

                if (i < segs)
                {
                    verts[bottomCapStart + i] = Vertex(qRx, mqH, qRz, botNData,
                                                       0.5f + 0.5f * c, 0.5f + 0.5f * s);
                }
            }

            Face16 *PIP3D_RESTRICT fPtr = faces;
            for (uint8_t i = 0; i < segs; ++i)
            {
                const uint16_t next = i + 1;
                const uint16_t apex = sideApexStart + i;
                const uint16_t b1 = sideBaseStart + i;
                const uint16_t b2 = sideBaseStart + next;

                fPtr[0] = Face16(apex, b2, b1);
                fPtr += 1;

                const uint16_t capNext = (i + 1 == segs) ? 0 : i + 1;
                fPtr[0] = Face16(baseCenterIdx, bottomCapStart + i, bottomCapStart + capNext);
                fPtr += 1;
            }

            buildFromInterleaved(verts, static_cast<uint32_t>(3 + segs * 3), faces,
                                 static_cast<uint32_t>(segs * 2), false, true, true);

            float yc, r_bound;
            if (radius > height)
            {
                yc = -h;
                r_bound = radius;
            }
            else
            {
                yc = -(radius * radius) / (2.0f * height);
                r_bound = h - yc;
            }

            finalizeBounds(Vector3(0.0f, yc, 0.0f), r_bound);
            bindDeleter<Cone>();
        }
    };
}