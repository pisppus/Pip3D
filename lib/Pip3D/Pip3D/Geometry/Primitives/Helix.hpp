#pragma once

#include "Geometry/Mesh.hpp"

namespace pip3D
{
    class Helix : public Mesh
    {
    private:
        PIP3D_FORCE_INLINE static Vector3 transportN(const Vector3 &N_prev,
                                                     const Vector3 &T_prev,
                                                     const Vector3 &T_curr,
                                                     const Vector3 &path_delta)
        {
            const float c1 = path_delta.dot(path_delta);
            if (c1 < 1e-8f)
                return N_prev;

            const float inv_c1 = FastMath::fastReciprocal(c1);
            const float k1 = 2.0f * path_delta.dot(N_prev) * inv_c1;
            const float k1t = 2.0f * path_delta.dot(T_prev) * inv_c1;
            const Vector3 NL = N_prev - path_delta * k1;
            const Vector3 TL = T_prev - path_delta * k1t;

            const Vector3 v2 = T_curr - TL;
            const float c2 = v2.dot(v2);
            if (c2 < 1e-8f)
                return NL;

            return NL - v2 * (2.0f * v2.dot(NL) * FastMath::fastReciprocal(c2));
        }

        struct Frame
        {
            Vector3 T, N, B;
        };

        static PIP3D_FORCE_INLINE int16_t saturateInt16(int32_t v)
        {
            if (v < -32768)
                return -32768;
            if (v > 32767)
                return 32767;
            return static_cast<int16_t>(v);
        }

    public:
        Helix(float radius = 0.5f, float height = 2.0f, float turns = 3.0f,
              uint8_t segments = 48, uint8_t tubeSegments = 8,
              float uvScaleU = 1.0f, float uvScaleV = 1.0f,
              float tubeRadius = -1.0f)
            : Mesh(static_cast<uint16_t>(((segments ? segments : 3) + 1) *
                                             ((tubeSegments ? tubeSegments : 3)) +
                                         2),
                   static_cast<uint16_t>(((segments ? segments : 3) *
                                          (tubeSegments ? tubeSegments : 3) * 2) +
                                         (tubeSegments ? tubeSegments : 3) * 2))
        {
            const uint8_t segs = (segments == 0) ? 3 : (segments > 128 ? 128 : segments);
            const uint8_t tubeSegs = (tubeSegments == 0) ? 3 : (tubeSegments > 32 ? 32 : tubeSegments);
            const float safeTurns = (turns > 0.01f) ? turns : 0.01f;

            const float tubeR = (tubeRadius > 0.0f) ? tubeRadius : (0.3f * radius);

            const float halfHeight = height * 0.5f;
            const float boundSq = radius * radius + halfHeight * halfHeight;
            const float boundR = boundSq * FastMath::fastInvSqrt(boundSq) + tubeR;
            const float size = boundR * 2.0f;
            autoScale(size);
            if (unlikely(!vertices_ || !faces_))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Helix: base alloc failed");
                return;
            }

            float cosC[32], sinC[32];
            const float invTS = FastMath::fastReciprocal(static_cast<float>(tubeSegs));
            for (uint8_t j = 0; j < tubeSegs; ++j)
            {
                const float a = kTwoPi * static_cast<float>(j) * invTS;
                FastMath::fastSinCos(a, sinC[j], cosC[j]);
            }

            const size_t pathBytes = static_cast<size_t>(segs + 1) * sizeof(Vector3);
            const size_t frameBytes = static_cast<size_t>(segs + 1) * sizeof(Frame);
            uint8_t *block = static_cast<uint8_t *>(
                MemUtils::allocData(pathBytes + frameBytes, 16));
            if (unlikely(!block))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Helix: path/frame alloc failed");
                return;
            }
            Vector3 *PIP3D_RESTRICT path = reinterpret_cast<Vector3 *>(block);
            Frame *PIP3D_RESTRICT frames = reinterpret_cast<Frame *>(block + pathBytes);

            const float invS = FastMath::fastReciprocal(static_cast<float>(segs));
            const float totalAngle = kTwoPi * safeTurns;
            const float yStep = height * invS;
            for (uint8_t i = 0; i <= segs; ++i)
            {
                const float t = totalAngle * static_cast<float>(i) * invS;
                float st, ct;
                FastMath::fastSinCos(t, st, ct);
                path[i].x = radius * ct;
                path[i].y = -halfHeight + static_cast<float>(i) * yStep;
                path[i].z = radius * st;
            }

            frames[0].T = path[1] - path[0];
            frames[0].T.normalize();
            {
                const Vector3 U(fabsf(frames[0].T.y) > 0.9f
                                    ? Vector3(1, 0, 0)
                                    : Vector3(0, 1, 0));
                frames[0].N = frames[0].T.cross(U);
                frames[0].N.normalize();
                frames[0].B = frames[0].T.cross(frames[0].N);
            }

            for (uint8_t i = 1; i <= segs; ++i)
            {
                if (i < segs)
                    frames[i].T = path[i + 1] - path[i];
                else
                    frames[i].T = path[i] - path[i - 1];
                frames[i].T.normalize();

                const Vector3 delta = path[i] - path[i - 1];
                frames[i].N = transportN(frames[i - 1].N, frames[i - 1].T,
                                         frames[i].T, delta);
                frames[i].N.normalize();
                frames[i].B = frames[i].T.cross(frames[i].N);
            }

            Vertex *PIP3D_RESTRICT vPtr = vertices_;
            const float invQ = (qScale_ > 1e-6f)
                                   ? FastMath::fastReciprocal(qScale_)
                                   : 1.0f;

            for (uint8_t i = 0; i <= segs; ++i)
            {
                const float px_c = path[i].x;
                const float py_c = path[i].y;
                const float pz_c = path[i].z;

                const float Nx = frames[i].N.x, Ny = frames[i].N.y, Nz = frames[i].N.z;
                const float Bx = frames[i].B.x, By = frames[i].B.y, Bz = frames[i].B.z;

                const float u = static_cast<float>(i) * invS * uvScaleU;

                for (uint8_t j = 0; j < tubeSegs; ++j)
                {
                    const float cn = cosC[j];
                    const float sn = sinC[j];

                    const float dx = cn * Nx + sn * Bx;
                    const float dy = cn * Ny + sn * By;
                    const float dz = cn * Nz + sn * Bz;

                    const float vx = px_c + tubeR * dx;
                    const float vy = py_c + tubeR * dy;
                    const float vz = pz_c + tubeR * dz;

                    PackedNormal nrm;
                    nrm.set(dx, dy, dz);

                    *vPtr = Vertex(
                        saturateInt16(lrintf(vx * invQ)),
                        saturateInt16(lrintf(vy * invQ)),
                        saturateInt16(lrintf(vz * invQ)),
                        nrm.data,
                        u,
                        static_cast<float>(j) * invTS * uvScaleV);
                    ++vPtr;
                }
            }

            const uint16_t tubeSurfaceVerts = static_cast<uint16_t>(segs + 1) * tubeSegs;
            const uint16_t cap0CenterIdx = tubeSurfaceVerts;
            const uint16_t cap1CenterIdx = tubeSurfaceVerts + 1;

            {
                const float px_c = path[0].x;
                const float py_c = path[0].y;
                const float pz_c = path[0].z;
                const float Tx = frames[0].T.x, Ty = frames[0].T.y, Tz = frames[0].T.z;

                PackedNormal capN;
                capN.set(-Tx, -Ty, -Tz);
                *vPtr = Vertex(
                    saturateInt16(lrintf(px_c * invQ)),
                    saturateInt16(lrintf(py_c * invQ)),
                    saturateInt16(lrintf(pz_c * invQ)),
                    capN.data, 0.5f, 0.5f);
                ++vPtr;
            }

            {
                const float px_c = path[segs].x;
                const float py_c = path[segs].y;
                const float pz_c = path[segs].z;
                const float Tx = frames[segs].T.x, Ty = frames[segs].T.y, Tz = frames[segs].T.z;

                PackedNormal capN;
                capN.set(Tx, Ty, Tz);
                *vPtr = Vertex(
                    saturateInt16(lrintf(px_c * invQ)),
                    saturateInt16(lrintf(py_c * invQ)),
                    saturateInt16(lrintf(pz_c * invQ)),
                    capN.data, 0.5f, 0.5f);
                ++vPtr;
            }

            MemUtils::freeData(block);

            vertexCount_ = static_cast<uint16_t>(vPtr - vertices_);

            Face *PIP3D_RESTRICT fPtr = faces_;
            const uint16_t pitch = tubeSegs;

            for (uint8_t i = 0; i < segs; ++i)
            {
                const uint16_t rowCurr = static_cast<uint16_t>(i) * pitch;
                const uint16_t rowNext = static_cast<uint16_t>(i + 1) * pitch;

                for (uint8_t j = 0; j < tubeSegs; ++j)
                {
                    const uint16_t jNext = static_cast<uint16_t>((j + 1) % tubeSegs);

                    const uint16_t a = rowCurr + j;
                    const uint16_t b = rowNext + j;
                    const uint16_t c = rowNext + jNext;
                    const uint16_t d = rowCurr + jNext;

                    fPtr[0] = Face(a, c, b);
                    fPtr[1] = Face(a, d, c);
                    fPtr += 2;
                }
            }

            for (uint8_t j = 0; j < tubeSegs; ++j)
            {
                const uint16_t rim0 = j;
                const uint16_t rim1 = static_cast<uint16_t>((j + 1) % tubeSegs);
                fPtr[0] = Face(cap0CenterIdx, rim1, rim0);
                fPtr += 1;
            }

            {
                const uint16_t lastRow = static_cast<uint16_t>(segs) * pitch;
                for (uint8_t j = 0; j < tubeSegs; ++j)
                {
                    const uint16_t rim0 = lastRow + j;
                    const uint16_t rim1 = lastRow + static_cast<uint16_t>((j + 1) % tubeSegs);
                    fPtr[0] = Face(cap1CenterIdx, rim0, rim1);
                    fPtr += 1;
                }
            }

            faceCount_ = static_cast<uint16_t>(fPtr - faces_);

            finalizeGeometry(vertexCount_, faceCount_,
                             Vector3(0.0f, 0.0f, 0.0f),
                             boundR);
            bindDeleter<Helix>();
        }
    };

}
