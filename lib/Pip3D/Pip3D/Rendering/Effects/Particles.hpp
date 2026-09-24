#pragma once

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Debug/Logging.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Renderer.hpp"
#include "Rendering/Pipeline/Billboard.hpp"
#include "Physics/Physics.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

namespace pip3D
{
    enum ParticleType : uint8_t
    {
        PARTICLE_BILLBOARD,
        PARTICLE_SPARK_STRETCH,
        PARTICLE_TURBULENT,
        PARTICLE_MASKED,
        PARTICLE_TEXTURED
    };

    struct Particle
    {
        Vector3 position;
        Vector3 velocity;
        float lifetime;
        float age;
        Color startColor;
        Color endColor;
        float startSize;
        float endSize;
        bool alive;

        Particle()
            : position(0, 0, 0), velocity(0, 0, 0), lifetime(1.0f), age(0.0f),
              startColor(Color::WHITE), endColor(Color::WHITE),
              startSize(4.0f), endSize(0.0f),
              alive(false) {}
    };

    struct ParticleEmitterConfig
    {
        uint16_t maxParticles;
        float emitRate;
        float minLifetime;
        float maxLifetime;
        float initialSpeed;
        float spread;
        Vector3 acceleration;
        Color startColor;
        Color endColor;
        float startSize;
        float endSize;
        bool looping;
        bool additive;

        ParticleType type;
        bool physicsCollision;

        const Texture *texture = nullptr;
        uint16_t chromaKey = 0xF81F;
        bool cutout = true;

        ParticleEmitterConfig()
            : maxParticles(64), emitRate(30.0f),
              minLifetime(0.4f), maxLifetime(0.8f),
              initialSpeed(1.0f), spread(0.4f),
              acceleration(0.0f, 0.0f, 0.0f),
              startColor(Color::fromRGB888(255, 255, 255)),
              endColor(Color::fromRGB888(0, 0, 0)),
              startSize(4.0f), endSize(0.0f),
              looping(true), additive(false),
              type(PARTICLE_BILLBOARD),
              physicsCollision(false) {}
    };

    class ParticleEmitter
    {
    private:
        Vector3 position;
        Vector3 velocityOffset;
        ParticleEmitterConfig config;
        std::vector<Particle> particles;
        float emitAccumulator;
        bool enabled;

        static void drawThickLineAdditive(uint16_t *fb, ZBuffer &zBuf, int16_t depth,
                                          int x0, int y0, int x1, int y1,
                                          uint16_t color, uint8_t alpha, int width,
                                          int bandTop, int bandBottom)
        {
            int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy, e2;

            const uint32_t s_rb = color & 0xF81F;
            const uint32_t s_g = color & 0x07E0;
            const uint32_t a = alpha >> 3;
            const uint32_t inv_a = 32 - a;
            const uint32_t s_rb_a = s_rb * a;
            const uint32_t s_g_a = s_g * a;

            auto plot = [&](int px, int py)
            {
                if (py >= bandTop && py < bandBottom && px >= 0 && px < width)
                {
                    int localY = py - bandTop;

                    const uint16_t stored = zBuf.data()[static_cast<size_t>(localY) * SCREEN_WIDTH + px] & Z_DEPTH_MASK;
                    if (stored != 0 && stored > depth + 5)
                        return;

                    size_t idx = (size_t)localY * width + px;
                    const uint32_t dst = fb[idx];
                    const uint32_t rb = (dst & 0xF81F);
                    const uint32_t g = (dst & 0x07E0);
                    const uint32_t blended_rb = ((rb * inv_a + s_rb_a) >> 5) & 0xF81F;
                    const uint32_t blended_g = ((g * inv_a + s_g_a) >> 5) & 0x07E0;
                    fb[idx] = static_cast<uint16_t>(blended_rb | blended_g);
                }
            };

            while (true)
            {
                plot(x0, y0);
                plot(x0 + 1, y0);
                plot(x0, y0 + 1);
                plot(x0 + 1, y0 + 1);

                if (x0 == x1 && y0 == y1)
                    break;
                e2 = 2 * err;
                if (e2 >= dy)
                {
                    err += dy;
                    x0 += sx;
                }
                if (e2 <= dx)
                {
                    err += dx;
                    y0 += sy;
                }
            }
        }

    public:
        ParticleEmitter(const ParticleEmitterConfig &cfg, const Vector3 &pos = Vector3())
            : position(pos), velocityOffset(0, 0, 0), config(cfg),
              emitAccumulator(0.0f), enabled(true)
        {
            particles.resize(config.maxParticles);
        }

        void setPosition(const Vector3 &pos) { position = pos; }
        const Vector3 &getPosition() const { return position; }
        void setVelocityOffset(const Vector3 &v) { velocityOffset = v; }
        void setEnabled(bool e) { enabled = e; }
        bool isEnabled() const { return enabled; }

        void triggerBurst(int count)
        {
            if (count <= 0)
                return;
            for (int i = 0; i < count; ++i)
                spawnParticle();
        }

        void update(float dt, PhysicsWorld *physicsWorld = nullptr)
        {
            if (dt <= 0.0f)
                return;

            if (enabled && config.emitRate > 0.0f && config.looping)
            {
                emitAccumulator += config.emitRate * dt;
                int toEmit = (int)emitAccumulator;
                emitAccumulator -= toEmit;
                for (int i = 0; i < toEmit; ++i)
                    spawnParticle();
            }

            for (size_t i = 0; i < particles.size(); ++i)
            {
                Particle &p = particles[i];
                if (!p.alive)
                    continue;

                p.age += dt;
                if (p.age >= p.lifetime)
                {
                    p.alive = false;
                    continue;
                }

                p.velocity += config.acceleration * dt;

                if (config.type == PARTICLE_TURBULENT)
                {
                    float wave = sinf(p.age * 5.0f + p.position.y * 2.0f);
                    p.velocity.x += wave * 1.5f * dt;
                    p.velocity.z += wave * 1.5f * dt;
                }

                if (config.physicsCollision && physicsWorld)
                {
                    Vector3 displacement = p.velocity * dt;
                    float stepDist = displacement.length();

                    if (stepDist > 1e-4f)
                    {
                        Ray ray(p.position, p.velocity);
                        RaycastHit hit;

                        if (physicsWorld->raycast(ray, hit, stepDist))
                        {
                            p.position = hit.point + hit.normal * 0.03f;
                            float dot = p.velocity.dot(hit.normal);
                            Vector3 reflected = p.velocity - hit.normal * (2.0f * dot);
                            p.velocity = reflected * 0.55f;
                        }
                        else
                        {
                            p.position += displacement;
                        }
                    }
                }
                else
                {
                    p.position += p.velocity * dt;
                }
            }
        }

        void render(Renderer &renderer) const
        {
            const Viewport &vp = renderer.getViewport();
            const int16_t width = vp.width;

#if PIP3D_TILED_RENDERING
#else
            uint16_t *fb = renderer.getFrameBuffer();
            if (!fb)
                return;

            int16_t bandTop = g_bandOffsetY;
            int16_t bandH = g_bandHeight;
            int16_t bandBottom = bandTop + bandH;

            const Camera &cam = renderer.getCamera();
            const float fovRad = cam.fov * kDegToRad;
            const float projScale = 1.0f / tanf(fovRad * 0.5f);
            const float halfViewportHeight = renderer.getViewport().height * 0.5f;
            BillboardFrameContext bbCtx = makeBillboardFrameContext(
                cam, vp, renderer.getViewProjMatrix(), renderer.getFrustum());

            static constexpr size_t MAX_TEXTURED_QUADS = 64;
            static BillboardQuad texturedQuads[MAX_TEXTURED_QUADS];
            size_t texturedQuadCount = 0;

            for (size_t i = 0; i < particles.size(); ++i)
            {
                const Particle &p = particles[i];
                if (!p.alive)
                    continue;

                float t = p.age / p.lifetime;
                t = clamp(t, 0.0f, 1.0f);

                uint8_t alpha = (uint8_t)((1.0f - t) * COLOR_BYTE_MAX_F);
                if (alpha == 0)
                    continue;

                float z_view = (p.position - cam.position).dot(cam.forward());
                if (z_view <= cam.nearPlane)
                    continue;

                Color col = p.startColor.blend(p.endColor, (uint8_t)(t * COLOR_BYTE_MAX_F));

                Vector3 screen = renderer.project(p.position);
                if (screen.z <= 0.0f || screen.z >= static_cast<float>(Z_DEPTH_MAX))
                    continue;

                const uint16_t particle_depth = static_cast<uint16_t>(screen.z);
                ZBuffer &zBuf = renderer.getZBuffer();

                if (config.type == PARTICLE_SPARK_STRETCH)
                {
                    Vector3 screenPrev = renderer.project(p.position - p.velocity * 0.12f);

                    drawThickLineAdditive(fb, zBuf, particle_depth,
                                          (int)screen.x, (int)screen.y,
                                          (int)screenPrev.x, (int)screenPrev.y,
                                          col.rgb565, alpha, width, bandTop, bandBottom);
                    continue;
                }

                if (config.type == PARTICLE_TEXTURED && config.texture != nullptr)
                {
                    if (texturedQuadCount < MAX_TEXTURED_QUADS)
                    {
                        const float size_world = (p.startSize + (p.endSize - p.startSize) * t) * 0.04f;

                        BillboardQuad q = {};
                        q.texture = config.texture;
                        q.chromaKey = config.chromaKey;
                        q.alpha = alpha;
                        q.blend = static_cast<uint8_t>(config.cutout ? BB_BLEND_CUTOUT : BB_BLEND_ALPHA);
                        q.lit = false;

                        if (buildBillboardQuadGeometry(
                                p.position, size_world, size_world,
                                BB_SCREEN_ALIGNED, 0.0f,
                                false, col,
                                bbCtx, q))
                            texturedQuads[texturedQuadCount++] = q;
                    }
                    continue;
                }

                float size_world = (p.startSize + (p.endSize - p.startSize) * t) * 0.04f;
                float radius_pixels = (size_world * projScale / z_view) * halfViewportHeight;

                int radius = (int)radius_pixels;
                if (radius <= 0)
                    continue;

                int cx = (int)screen.x;
                int cy = (int)screen.y;

                int r2 = radius * radius;

                uint32_t inv_r2 = 65536 / r2;
                if (inv_r2 == 0)
                    inv_r2 = 1;

                int y0 = cy - radius;
                int y1 = cy + radius;

                if (y0 < bandTop)
                    y0 = bandTop;
                if (y1 >= bandBottom)
                    y1 = bandBottom - 1;
                if (y0 > y1)
                    continue;

                const uint32_t s_rb = col.rgb565 & 0xF81F;
                const uint32_t s_g = col.rgb565 & 0x07E0;

                for (int y = y0; y <= y1; ++y)
                {
                    int dy = y - cy;
                    int dy2 = dy * dy;
                    int x0 = clamp(cx - radius, 0, (int)width - 1);
                    int x1 = clamp(cx + radius, 0, (int)width - 1);
                    int localY = y - bandTop;
                    size_t idx = (size_t)localY * width + x0;

                    for (int x = x0; x <= x1; ++x, ++idx)
                    {
                        int dx = x - cx;
                        int dist2 = dx * dx + dy2;
                        if (dist2 > r2)
                            continue;

                        const uint16_t stored_depth = zBuf.data()[static_cast<size_t>(localY) * SCREEN_WIDTH + x] & Z_DEPTH_MASK;
                        if (stored_depth != 0 && stored_depth > particle_depth + 5)
                            continue;

                        uint32_t dist_scaled = dist2 * inv_r2;
                        if (dist_scaled >= 65536)
                            continue;

                        uint8_t a;
                        if (config.type == PARTICLE_MASKED)
                        {
                            uint32_t k = 65536 - dist_scaled;
                            uint32_t k_sq = (k * k) >> 16;
                            a = (alpha * k_sq) >> 16;
                        }
                        else
                        {
                            a = (alpha * (65536 - dist_scaled)) >> 16;
                        }

                        if (a == 0)
                            continue;

                        if (config.additive)
                        {
                            const uint16_t dst = fb[idx];
                            const uint32_t cur_a = a >> 3;

                            uint32_t rDst = (dst >> 11) & 0x1F;
                            uint32_t gDst = (dst >> 5) & 0x3F;
                            uint32_t bDst = dst & 0x1F;

                            const uint32_t rSrc = s_rb >> 11;
                            const uint32_t gSrc = s_g >> 5;
                            const uint32_t bSrc = s_rb & 0x1F;

                            rDst += (rSrc * cur_a) >> 5;
                            gDst += (gSrc * cur_a) >> 5;
                            bDst += (bSrc * cur_a) >> 5;

                            if (rDst > 31u)
                                rDst = 31u;
                            if (gDst > 63u)
                                gDst = 63u;
                            if (bDst > 31u)
                                bDst = 31u;

                            fb[idx] = (uint16_t)((rDst << 11) | (gDst << 5) | bDst);
                        }
                        else
                        {
                            const uint32_t dst = fb[idx];
                            const uint32_t cur_a = a >> 3;
                            const uint32_t inv_a = 32 - cur_a;

                            const uint32_t rb = (dst & 0xF81F);
                            const uint32_t g = (dst & 0x07E0);
                            const uint32_t blended_rb = ((rb * inv_a + s_rb * cur_a) >> 5) & 0xF81F;
                            const uint32_t blended_g = ((g * inv_a + s_g * cur_a) >> 5) & 0x07E0;

                            fb[idx] = static_cast<uint16_t>(blended_rb | blended_g);
                        }
                    }
                }
            }

            if (texturedQuadCount > 0)
            {
                if (texturedQuadCount > 1 && !config.cutout)
                {
                    std::sort(texturedQuads, texturedQuads + texturedQuadCount,
                              [](const BillboardQuad &a, const BillboardQuad &b)
                              { return a.distSq > b.distSq; });
                }
                renderer.drawBillboardQuads(texturedQuads, texturedQuadCount);
            }
#endif
        }

    private:
        void spawnParticle()
        {
            for (size_t i = 0; i < particles.size(); ++i)
            {
                Particle &p = particles[i];
                if (p.alive)
                    continue;

                float life = randomRange(config.minLifetime, config.maxLifetime);
                if (life <= 0.0f)
                    life = 0.1f;

                float rx = random01() - 0.5f;
                float rz = random01() - 0.5f;

                Vector3 dir(rx * config.spread, 1.0f, rz * config.spread);
                dir.normalize();

                Vector3 vel = dir * config.initialSpeed + velocityOffset;

                p.position = position;
                p.velocity = vel;
                p.lifetime = life;
                p.age = 0.0f;
                p.startColor = config.startColor;
                p.endColor = config.endColor;
                p.startSize = config.startSize;
                p.endSize = config.endSize;
                p.alive = true;
                return;
            }
        }

        static float random01() { return (float)random(0L, 32767L) / 32767.0f; }
        static float randomRange(float a, float b) { return a + (b - a) * random01(); }
    };

    class FXSystem
    {
    private:
        std::vector<ParticleEmitter *> emitters;

    public:
        FXSystem() {}
        ~FXSystem() { clear(); }

        ParticleEmitter *createEmitter(const ParticleEmitterConfig &cfg, const Vector3 &pos = Vector3())
        {
            ParticleEmitter *e = new ParticleEmitter(cfg, pos);
            emitters.push_back(e);
            return e;
        }

        void destroyEmitter(ParticleEmitter *emitter)
        {
            if (!emitter)
                return;
            for (size_t i = 0; i < emitters.size(); ++i)
            {
                if (emitters[i] == emitter)
                {
                    delete emitters[i];
                    emitters[i] = emitters.back();
                    emitters.pop_back();
                    return;
                }
            }
        }

        void clear()
        {
            for (size_t i = 0; i < emitters.size(); ++i)
                delete emitters[i];
            emitters.clear();
        }

        void update(float dt, PhysicsWorld *world = nullptr)
        {
            for (size_t i = 0; i < emitters.size(); ++i)
                emitters[i]->update(dt, world);
        }

        void render(Renderer &renderer) const
        {
            for (size_t i = 0; i < emitters.size(); ++i)
                emitters[i]->render(renderer);
        }

        ParticleEmitter *createFire(const Vector3 &pos)
        {
            ParticleEmitterConfig cfg;
            cfg.type = PARTICLE_BILLBOARD;
            cfg.maxParticles = 120;
            cfg.emitRate = 95.0f;
            cfg.minLifetime = 0.5f;
            cfg.maxLifetime = 1.0f;
            cfg.initialSpeed = 2.4f;
            cfg.spread = 0.6f;
            cfg.acceleration = Vector3(0.0f, 4.0f, 0.0f);
            cfg.startColor = Color::fromRGB888(255, 195, 30);
            cfg.endColor = Color::fromRGB888(170, 10, 0);
            cfg.startSize = 18.0f;
            cfg.endSize = 3.0f;
            cfg.looping = true;
            cfg.additive = true;
            return createEmitter(cfg, pos);
        }

        ParticleEmitter *createTexturedEmitter(const Vector3 &pos,
                                               const Texture *texture,
                                               bool cutout = true)
        {
            ParticleEmitterConfig cfg;
            cfg.type = PARTICLE_TEXTURED;
            cfg.texture = texture;
            cfg.cutout = cutout;
            cfg.chromaKey = 0xF81F;
            cfg.maxParticles = 80;
            cfg.emitRate = 60.0f;
            cfg.minLifetime = 0.5f;
            cfg.maxLifetime = 1.0f;
            cfg.initialSpeed = 2.0f;
            cfg.spread = 0.5f;
            cfg.acceleration = Vector3(0.0f, 3.5f, 0.0f);
            cfg.startColor = Color::WHITE;
            cfg.endColor = Color::fromRGB888(180, 80, 0);
            cfg.startSize = 18.0f;
            cfg.endSize = 4.0f;
            cfg.looping = true;
            cfg.additive = !cutout;
            return createEmitter(cfg, pos);
        }

        ParticleEmitter *createSmoke(const Vector3 &pos)
        {
            ParticleEmitterConfig cfg;
            cfg.type = PARTICLE_TURBULENT;
            cfg.maxParticles = 90;
            cfg.emitRate = 45.0f;
            cfg.minLifetime = 1.0f;
            cfg.maxLifetime = 1.8f;
            cfg.initialSpeed = 1.5f;
            cfg.spread = 0.3f;
            cfg.acceleration = Vector3(0.1f, 1.8f, 0.1f);
            cfg.startColor = Color::fromRGB888(140, 140, 140);
            cfg.endColor = Color::fromRGB888(45, 45, 45);
            cfg.startSize = 15.0f;
            cfg.endSize = 25.0f;
            cfg.looping = true;
            cfg.additive = false;
            return createEmitter(cfg, pos);
        }

        ParticleEmitter *createExplosion(const Vector3 &pos)
        {
            ParticleEmitterConfig cfg;
            cfg.type = PARTICLE_MASKED;
            cfg.maxParticles = 80;
            cfg.emitRate = 0.0f;
            cfg.minLifetime = 0.4f;
            cfg.maxLifetime = 0.9f;
            cfg.initialSpeed = 4.5f;
            cfg.spread = 1.5f;
            cfg.acceleration = Vector3(0.0f, -0.8f, 0.0f);
            cfg.startColor = Color::fromRGB888(255, 230, 150);
            cfg.endColor = Color::fromRGB888(60, 5, 0);
            cfg.startSize = 12.0f;
            cfg.endSize = 24.0f;
            cfg.looping = false;
            cfg.additive = false;

            ParticleEmitter *e = createEmitter(cfg, pos);
            e->triggerBurst(cfg.maxParticles);
            return e;
        }

        ParticleEmitter *createSparks(const Vector3 &pos)
        {
            ParticleEmitterConfig cfg;
            cfg.type = PARTICLE_SPARK_STRETCH;
            cfg.physicsCollision = true;
            cfg.maxParticles = 200;
            cfg.emitRate = 150.0f;
            cfg.minLifetime = 0.8f;
            cfg.maxLifetime = 1.8f;
            cfg.initialSpeed = 7.5f;
            cfg.spread = 1.3f;
            cfg.acceleration = Vector3(0.0f, -11.0f, 0.0f);
            cfg.startColor = Color::fromRGB888(255, 255, 200);
            cfg.endColor = Color::fromRGB888(255, 40, 0);
            cfg.startSize = 6.0f;
            cfg.endSize = 1.5f;
            cfg.looping = true;
            cfg.additive = true;
            return createEmitter(cfg, pos);
        }

        ParticleEmitter *createTrail(const Vector3 &pos)
        {
            ParticleEmitterConfig cfg;
            cfg.maxParticles = 64;
            cfg.emitRate = 40.0f;
            cfg.minLifetime = 0.4f;
            cfg.maxLifetime = 0.8f;
            cfg.initialSpeed = 0.3f;
            cfg.spread = 0.4f;
            cfg.startColor = Color::fromRGB888(200, 220, 255);
            cfg.endColor = Color::fromRGB888(80, 120, 220);
            cfg.startSize = 3.0f;
            cfg.endSize = 1.0f;
            cfg.looping = true;
            cfg.additive = true;
            return createEmitter(cfg, pos);
        }
    };
}