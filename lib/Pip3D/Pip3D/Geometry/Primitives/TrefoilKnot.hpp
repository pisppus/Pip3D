#pragma once

#include "Geometry/Mesh.hpp"

namespace pip3D
{

    class TrefoilKnot : public Mesh
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

    public:
        TrefoilKnot(float scale = 1.0f,
                    uint8_t segments = 64,
                    uint8_t tubeSegments = 12,
                    float uvScaleU = 1.0f,
                    float uvScaleV = 1.0f)
            : Mesh(static_cast<uint16_t>(((segments ? segments : 3) + 1) *
                                         ((tubeSegments ? tubeSegments : 3) + 1)),
                   static_cast<uint16_t>((segments ? segments : 3) *
                                         (tubeSegments ? tubeSegments : 3) * 2))
        {
            autoScale(scale * 7.5f);
            if (!vertices_ || !faces_)
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "TrefoilKnot: base alloc failed");
                return;
            }

            const uint8_t segs = segments ? segments : 3;
            const uint8_t tubeSegs = tubeSegments ? tubeSegments : 3;

            constexpr float tubeScale = 0.55f;
            const float tubeRadius = tubeScale * scale;

            alignas(16) float cosC[65];
            alignas(16) float sinC[65];
            const float invTS = FastMath::fastReciprocal(static_cast<float>(tubeSegs));
            for (uint8_t j = 0; j < tubeSegs; ++j)
            {
                const float a = kTwoPi * static_cast<float>(j) * invTS;
                FastMath::fastSinCos(a, sinC[j], cosC[j]);
            }
            cosC[tubeSegs] = cosC[0];
            sinC[tubeSegs] = sinC[0];

            Vector3 *PIP3D_RESTRICT path =
                static_cast<Vector3 *>(MemUtils::allocData((segs + 1) * sizeof(Vector3), 16));
            Frame *PIP3D_RESTRICT frames =
                static_cast<Frame *>(MemUtils::allocData((segs + 1) * sizeof(Frame), 16));

            if (unlikely(!path || !frames))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "TrefoilKnot: path/frame alloc failed");
                if (path)
                    MemUtils::freeData(path);
                if (frames)
                    MemUtils::freeData(frames);
                return;
            }

            const float invS = FastMath::fastReciprocal(static_cast<float>(segs));
            for (uint8_t i = 0; i < segs; ++i)
            {
                const float t = kTwoPi * static_cast<float>(i) * invS;
                float s2, c2, s3, c3;
                FastMath::fastSinCos(2.0f * t, s2, c2);
                FastMath::fastSinCos(3.0f * t, s3, c3);

                const float r = scale * (2.0f + c3);
                path[i].x = r * c2;
                path[i].y = r * s2;
                path[i].z = scale * (-s3 * 1.4f);
            }
            path[segs] = path[0];

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

            for (uint8_t i = 1; i < segs; ++i)
            {
                frames[i].T = path[i + 1] - path[i];
                frames[i].T.normalize();

                const Vector3 delta = path[i] - path[i - 1];
                frames[i].N = transportN(frames[i - 1].N, frames[i - 1].T,
                                         frames[i].T, delta);
                frames[i].N.normalize();
                frames[i].B = frames[i].T.cross(frames[i].N);
            }

            {
                const Vector3 delta_last = path[0] - path[segs - 1];
                Vector3 N_transported = transportN(frames[segs - 1].N,
                                                   frames[segs - 1].T,
                                                   frames[0].T, delta_last);
                N_transported.normalize();

                float cosA = clamp(N_transported.dot(frames[0].N), -1.0f, 1.0f);
                float angle_diff = acosf(cosA);

                const Vector3 cr = N_transported.cross(frames[0].N);
                if (cr.dot(frames[0].T) < 0.0f)
                    angle_diff = -angle_diff;

                const float angle_step = angle_diff * invS;
                float c_step, s_step;
                FastMath::fastSinCos(angle_step, s_step, c_step);
                float c_total = 1.0f, s_total = 0.0f;

                for (uint8_t i = 1; i < segs; ++i)
                {
                    const float new_c = c_total * c_step - s_total * s_step;
                    const float new_s = s_total * c_step + c_total * s_step;
                    c_total = new_c;
                    s_total = new_s;

                    const Vector3 oN = frames[i].N;
                    const Vector3 oB = frames[i].B;
                    frames[i].N = oN * c_total + oB * s_total;
                    frames[i].B = oB * c_total - oN * s_total;
                }
            }

            frames[segs] = frames[0];

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

                for (uint8_t j = 0; j <= tubeSegs; ++j)
                {
                    const float cn = cosC[j];
                    const float sn = sinC[j];

                    const float dx = cn * Nx + sn * Bx;
                    const float dy = cn * Ny + sn * By;
                    const float dz = cn * Nz + sn * Bz;

                    const float vx = px_c + tubeRadius * dx;
                    const float vy = py_c + tubeRadius * dy;
                    const float vz = pz_c + tubeRadius * dz;

                    const int32_t qxi = lrintf(vx * invQ);
                    const int32_t qyi = lrintf(vy * invQ);
                    const int32_t qzi = lrintf(vz * invQ);

                    PackedNormal nrm;
                    nrm.set(dx, dy, dz);

                    *vPtr = Vertex(
                        static_cast<int16_t>(qxi < -32768 ? -32768 : (qxi > 32767 ? 32767 : qxi)),
                        static_cast<int16_t>(qyi < -32768 ? -32768 : (qyi > 32767 ? 32767 : qyi)),
                        static_cast<int16_t>(qzi < -32768 ? -32768 : (qzi > 32767 ? 32767 : qzi)),
                        nrm.data,
                        u,
                        static_cast<float>(j) * invTS * uvScaleV);
                    ++vPtr;
                }
            }

            MemUtils::freeData(frames);
            MemUtils::freeData(path);

            vertexCount_ = static_cast<uint16_t>(vPtr - vertices_);

            Face *PIP3D_RESTRICT fPtr = faces_;
            const uint16_t pitch = tubeSegs + 1;
            for (uint8_t i = 0; i < segs; ++i)
            {
                const uint16_t rowCurrent = static_cast<uint16_t>(i) * pitch;
                const uint16_t rowNext = rowCurrent + pitch;

                for (uint8_t j = 0; j < tubeSegs; ++j)
                {
                    const uint16_t a = rowCurrent + j;
                    const uint16_t b = rowNext + j;
                    const uint16_t c = rowNext + j + 1;
                    const uint16_t d = rowCurrent + j + 1;

                    fPtr[0] = Face(a, c, b);
                    fPtr[1] = Face(a, d, c);
                    fPtr += 2;
                }
            }

            faceCount_ = static_cast<uint16_t>(fPtr - faces_);

            finalizeGeometry(vertexCount_, faceCount_,
                             Vector3(0.0f, 0.0f, 0.0f),
                             scale * 3.55f);
            bindDeleter<TrefoilKnot>();
        }
    };

}