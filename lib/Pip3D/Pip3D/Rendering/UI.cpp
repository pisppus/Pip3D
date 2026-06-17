#include "Renderer.hpp"
#include "Rendering/UI/HUD.hpp"
#include "Rendering/UI/Font.hpp"

namespace pip3D
{
    void Renderer::addDirtyRect(MeshInstance *instance, int16_t x, int16_t y, int16_t w, int16_t h)
    {
        DirtyRegionHelper::addDirtyRect(instance, x, y, w, h,
                                        viewport,
                                        worldInstanceDirty,
                                        worldDirtyMinX,
                                        worldDirtyMinY,
                                        worldDirtyMaxX,
                                        worldDirtyMaxY,
                                        hasWorldDirtyRegion);
    }

    void Renderer::addHudDirtyRect(int16_t x, int16_t y, int16_t w, int16_t h)
    {
        DirtyRegionHelper::addHudDirtyRect(x, y, w, h,
                                           viewport,
                                           hudDirtyMinX,
                                           hudDirtyMinY,
                                           hudDirtyMaxX,
                                           hudDirtyMaxY,
                                           hasHudDirtyRegion);
    }

    void Renderer::addDirtyFromSphere(MeshInstance *instance, const Vector3 &c, float r)
    {
        if (r <= 0.0f)
            return;

        Vector3 pc = project(c);
        Vector3 px = project(Vector3(c.x + r, c.y, c.z));
        Vector3 py = project(Vector3(c.x, c.y + r, c.z));
        Vector3 pz = project(Vector3(c.x, c.y, c.z + r));

        float dx = fabsf(px.x - pc.x);
        float dy = fabsf(px.y - pc.y);

        float t = fabsf(py.x - pc.x);
        if (t > dx)
            dx = t;
        t = fabsf(pz.x - pc.x);
        if (t > dx)
            dx = t;

        t = fabsf(py.y - pc.y);
        if (t > dy)
            dy = t;
        t = fabsf(pz.y - pc.y);
        if (t > dy)
            dy = t;

        float rScr = dx > dy ? dx : dy;

        int16_t x0 = (int16_t)(pc.x - rScr);
        int16_t y0 = (int16_t)(pc.y - rScr);
        int16_t x1 = (int16_t)(pc.x + rScr + 1.0f);
        int16_t y1 = (int16_t)(pc.y + rScr + 1.0f);

        addDirtyRect(instance, x0, y0, x1 - x0, y1 - y0);
    }

    void Renderer::drawText(int16_t x, int16_t y, const char *text, uint16_t color)
    {
        HudRenderer::drawText(framebuffer, x, y, text, color);

        int16_t w = HudRenderer::getTextWidth(text);
        addHudDirtyRect(x, y, w, 8);
    }

    void Renderer::drawText(int16_t x, int16_t y, const char *text, Color color)
    {
        drawText(x, y, text, color.rgb565);
    }

    void Renderer::drawTextAdaptive(int16_t x, int16_t y, const char *text)
    {
        uint16_t color = getAdaptiveTextColor(x, y);
        drawText(x, y, text, color);
    }

    uint16_t Renderer::getAdaptiveTextColor(int16_t x, int16_t y, int16_t width, int16_t height)
    {
        return HudRenderer::getAdaptiveTextColor(framebuffer, viewport, x, y, width, height);
    }

    int16_t Renderer::getTextWidth(const char *text)
    {
        return HudRenderer::getTextWidth(text);
    }
}