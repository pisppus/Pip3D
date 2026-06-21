#include "Renderer.hpp"
#include "Rendering/UI/HUD.hpp"
#include "Rendering/UI/Font.hpp"

namespace pip3D
{
    void Renderer::drawText(int16_t x, int16_t y, const char *text, uint16_t color)
    {
        HudRenderer::drawText(framebuffer, x, y, text, color);
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