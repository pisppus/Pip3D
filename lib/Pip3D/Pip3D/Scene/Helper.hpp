#pragma once

#include "Rendering/Renderer.hpp"
#include "Geometry/Primitives.hpp"
#include "Core/Color.hpp"

namespace pip3D
{

    class SceneHelper
    {
    private:
        Renderer *renderer;
        float groundSize;
        float groundY;
        Color groundColor;
        bool hasSun;
        float sunIntensity;
        Plane *groundMesh;
        MeshInstance *groundInstance;

    public:
        SceneHelper(Renderer *r)
            : renderer(r), groundSize(15.0f), groundY(-1.5f),
              groundColor(Color::fromRGB888(100, 100, 100)),
              hasSun(false), sunIntensity(0.6f),
              groundMesh(nullptr), groundInstance(nullptr) {}

        ~SceneHelper()
        {
            if (groundInstance)
            {
                delete groundInstance;
                groundInstance = nullptr;
            }
            if (groundMesh)
            {
                delete groundMesh;
                groundMesh = nullptr;
            }
        }

        SceneHelper(const SceneHelper &) = delete;
        SceneHelper &operator=(const SceneHelper &) = delete;
        SceneHelper(SceneHelper &&) = delete;
        SceneHelper &operator=(SceneHelper &&) = delete;

        void addGround(float size, float y, Color color)
        {
            groundSize = size;
            groundY = y;
            groundColor = color;

            if (!groundMesh)
            {
                groundMesh = new Plane(size, size, 1, color);
                groundInstance = new MeshInstance(groundMesh);
            }
            else
            {
            }

            if (groundInstance)
            {
                groundInstance->setPosition(0.0f, y, 0.0f);
                groundInstance->setColor(color);
            }
        }

        void addSun(float intensity, float temperature)
        {
            hasSun = true;
            sunIntensity = intensity;
        }

        void renderGround()
        {
            if (groundInstance && renderer)
            {
                renderer->draw(groundInstance);
            }
        }

        MeshInstance *getGroundInstance() const { return groundInstance; }
        Plane *getGroundMesh() const { return groundMesh; }

        void renderSun(float glowIntensity, float temperature)
        {
            if (!renderer || !hasSun)
                return;

            Light *mainLight = renderer->getLight(0);
            if (!mainLight)
                return;

            Vector3 sunDir = mainLight->direction;
            Vector3 sunPos = sunDir * -20.0f;
            Color sunColor = Color::fromTemperature(temperature);

            renderer->drawSunSprite(sunPos, sunColor, glowIntensity);
        }

        void setSunPosition(float x, float y, float z)
        {
            if (!renderer)
                return;

            Vector3 sunPos(x, y, z);
            Vector3 sunDir = Vector3(0, 0, 0) - sunPos;
            sunDir.normalize();

            renderer->setLightDirection(sunDir);
            renderer->setLightPosition(sunPos);
        }

        void setSunPosition(float x, float y, float z, float temperature)
        {
            if (!renderer)
                return;

            Vector3 sunPos(x, y, z);
            Vector3 sunDir = Vector3(0, 0, 0) - sunPos;
            sunDir.normalize();

            Color sunColor = Color::fromTemperature(temperature);
            renderer->setMainDirectionalLight(sunDir, sunColor, 1.0f);
        }
    };

}
