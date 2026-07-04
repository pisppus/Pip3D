#include "Geometry/Billboard.hpp"
#include "Rendering/Renderer.hpp"

namespace pip3D
{
#if defined(PIP3D_DEBUG_BILLBOARD)
    static uint32_t s_bbDiagFrame = 0;
#endif

    void BillboardManager::render(Renderer &renderer)
    {
#if defined(PIP3D_DEBUG_BILLBOARD)
        if (billboards.empty())
        {
            static bool s_warnedEmpty = false;
            if (!s_warnedEmpty)
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "[BB] render: no billboards registered");
                s_warnedEmpty = true;
            }
            return;
        }

        s_bbDiagFrame++;
        const bool doDiag = ((s_bbDiagFrame % 60u) == 1u);
#else
        if (billboards.empty())
            return;
#endif

        const Camera &cam = renderer.getCamera();
        const Vector3 camPos = cam.position;
        const Vector3 camFwd = cam.forward();
        const Vector3 camRight = cam.right();
        const Vector3 camUp = cam.upVec();
        const bool perspective = (cam.projectionType == PERSPECTIVE || cam.projectionType == FISHEYE);

        const float fovRad = cam.fov * kDegToRad;
        const float projScale = perspective ? FastMath::fastReciprocal(tanf(fovRad * 0.5f)) : 1.0f;
        const float halfViewportHeight = renderer.getViewport().height * 0.5f;

        static std::vector<Billboard *> zwriteQueue;
        static std::vector<SortedBillboard> alphaQueue;
        zwriteQueue.clear();
        alphaQueue.clear();
        zwriteQueue.reserve(billboards.size());
        alphaQueue.reserve(billboards.size());

#if defined(PIP3D_DEBUG_BILLBOARD)
        if (doDiag)
        {
            LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                 "[BB] === render start frame=%u count=%u camPos=(%.2f,%.2f,%.2f) ===",
                 static_cast<unsigned>(s_bbDiagFrame),
                 static_cast<unsigned>(billboards.size()),
                 camPos.x, camPos.y, camPos.z);
        }
#endif

        size_t idx = 0;
        for (auto *bb : billboards)
        {
#if defined(PIP3D_DEBUG_BILLBOARD)
            if (doDiag)
            {
                LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                     "[BB] #%u: pos=(%.2f,%.2f,%.2f) size=%.2fx%.2f visible=%d tex=%p blend=%d orient=%d lit=%d",
                     static_cast<unsigned>(idx),
                     bb ? bb->position.x : 0, bb ? bb->position.y : 0, bb ? bb->position.z : 0,
                     bb ? bb->width : 0, bb ? bb->height : 0,
                     bb ? (int)bb->visible : -1,
                     bb ? static_cast<const void *>(bb->texture) : nullptr,
                     bb ? (int)bb->blend : -1,
                     bb ? (int)bb->orientation : -1,
                     bb ? (int)bb->lit : -1);
            }
#endif

            if (!bb || !bb->visible || !bb->texture)
            {
                ++idx;
                continue;
            }

            Vector3 worldQuad[4];
            float centerDist = 0.0f;
            if (!buildQuad(*bb, camPos, camRight, camUp, camFwd,
                           projScale, halfViewportHeight, perspective,
                           worldQuad, centerDist))
            {
#if defined(PIP3D_DEBUG_BILLBOARD)
                if (doDiag)
                    LOGW(::pip3D::Debug::LOG_MODULE_RENDER, "[BB] #%u: buildQuad false (degenerate/behind)", static_cast<unsigned>(idx));
#endif
                ++idx;
                continue;
            }

            if (perspective)
            {
                const float zView = (bb->position - camPos).dot(camFwd);
#if defined(PIP3D_DEBUG_BILLBOARD)
                if (doDiag)
                {
                    LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                         "[BB] #%u: zView=%.3f near=%.3f far=%.3f",
                         static_cast<unsigned>(idx), zView, cam.nearPlane, cam.farPlane);
                }
#endif
                if (zView <= cam.nearPlane || zView >= cam.farPlane)
                {
                    ++idx;
                    continue;
                }
            }

            if (bb->blend == BB_BLEND_ALPHA || bb->blend == BB_BLEND_ADDITIVE)
                alphaQueue.push_back({centerDist * centerDist, bb});
            else
                zwriteQueue.push_back(bb);

            ++idx;
        }

#if defined(PIP3D_DEBUG_BILLBOARD)
        if (doDiag)
        {
            LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                 "[BB] queues: zwrite=%u alpha=%u",
                 static_cast<unsigned>(zwriteQueue.size()),
                 static_cast<unsigned>(alphaQueue.size()));
        }
#endif

        for (auto *bb : zwriteQueue)
            renderer.drawBillboard(*bb);

        if (alphaQueue.size() > 1)
            std::sort(alphaQueue.begin(), alphaQueue.end());
        for (auto &sb : alphaQueue)
            renderer.drawBillboard(*sb.billboard);
    }
}
