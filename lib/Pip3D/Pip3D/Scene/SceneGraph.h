#pragma once

#include "SceneNode.h"
#include "Rendering/Renderer.h"
#include "Utils/ObjectUtils.h"

namespace pip3D
{

    __attribute__((always_inline, hot)) inline void MeshNode::render(Renderer *renderer)
    {
        if (!visible || !enabled || !mesh)
            return;

        const Matrix4x4 &world = getWorldTransform();
        Vector3 worldPos;
        Vector3 worldScale;
        Matrix4x4 basis;
        decomposeWorldTransform(world, worldPos, worldScale, basis);
        Vector3 worldEuler = basisToEulerDegrees(basis);

        mesh->setPosition(worldPos.x, worldPos.y, worldPos.z);
        mesh->setRotation(worldEuler.x, worldEuler.y, worldEuler.z);
        mesh->setScale(worldScale.x, worldScale.y, worldScale.z);

        if (castShadows)
        {
            ObjectHelper::renderWithShadow(renderer, mesh);
        }
        else
        {
            renderer->drawMesh(mesh);
        }

        Node::render(renderer);
    }

    class SceneGraph
    {
    private:
        Node *root;
        Renderer *renderer;
        CameraNode *activeCamera;
        std::vector<LightNode *> lights;

    public:
        SceneGraph(Renderer *rend)
            : renderer(rend), activeCamera(nullptr)
        {
            root = new Node("Root");
        }

        ~SceneGraph()
        {
            delete root;
        }

        Node *getRoot() { return root; }

        template <typename T>
        T *createNode(const String &name = "")
        {
            T *node = new T(name.isEmpty() ? String("Node") : name);
            root->addChild(node);
            return node;
        }

        MeshNode *createMeshNode(Mesh *mesh, const String &name = "MeshNode")
        {
            MeshNode *node = new MeshNode(mesh, name, false);
            root->addChild(node);
            return node;
        }

        CameraNode *createCameraNode(const String &name = "Camera")
        {
            CameraNode *node = new CameraNode(name);
            root->addChild(node);
            return node;
        }

        LightNode *createLightNode(LightType type = LIGHT_DIRECTIONAL, const String &name = "Light")
        {
            LightNode *node = new LightNode(name);
            node->setLightType(type);
            root->addChild(node);
            lights.push_back(node);
            return node;
        }

        void setActiveCamera(CameraNode *camera)
        {
            activeCamera = camera;
        }

        CameraNode *getActiveCamera() { return activeCamera; }

        void update(float deltaTime)
        {
            root->update(deltaTime);
        }

        __attribute__((hot)) inline void render()
        {
            if (!renderer)
                return;

            if (activeCamera)
            {
                activeCamera->applyToCamera(renderer->getCamera());
            }

            renderer->clearLights();

            int lightIndex = 0;
            for (size_t i = 0; i < lights.size() && lightIndex < 4; i++)
            {
                LightNode *node = lights[i];
                if (!node || !node->isEnabled() || !node->isVisible())
                    continue;

                Light light;
                node->applyToLight(light);
                renderer->setLight(lightIndex, light);
                lightIndex++;
            }

            for (int band = 0; band < SCREEN_BAND_COUNT; ++band)
            {
                renderer->beginFrameBand(band);
                root->render(renderer);
                renderer->drawSkyboxBackground();
                renderer->endFrameBand(band);
            }
        }

        Node *findNode(const String &name)
        {
            return findNodeRecursive(root, name);
        }

    private:
        Node *findNodeRecursive(Node *node, const String &name)
        {
            if (node->getName() == name)
            {
                return node;
            }

            for (size_t i = 0; i < node->getChildCount(); i++)
            {
                Node *found = findNodeRecursive(node->getChild(i), name);
                if (found)
                    return found;
            }

            return nullptr;
        }
    };

    class SceneBuilder
    {
    private:
        SceneGraph *scene;

    public:
        SceneBuilder(Renderer *renderer)
        {
            scene = new SceneGraph(renderer);
        }

        ~SceneBuilder()
        {
        }

        SceneGraph *getScene() { return scene; }

        SceneBuilder &withCamera(float x, float y, float z, float fov = 60.0f)
        {
            CameraNode *camera = scene->createCameraNode("MainCamera");
            camera->setPosition(x, y, z);
            camera->setFOV(fov);
            scene->setActiveCamera(camera);
            return *this;
        }

        SceneBuilder &withSun(float x, float y, float z, const Color &color = Color::WHITE)
        {
            LightNode *sun = scene->createLightNode(LIGHT_DIRECTIONAL, "Sun");
            sun->setDirection(Vector3(x, y, z));
            sun->setColor(color);
            sun->setIntensity(1.0f);
            return *this;
        }

        SceneBuilder &withPointLight(float x, float y, float z, const Color &color = Color::WHITE)
        {
            LightNode *light = scene->createLightNode(LIGHT_POINT, "PointLight");
            light->setPosition(x, y, z);
            light->setColor(color);
            light->setIntensity(1.0f);
            return *this;
        }

        MeshNode *addMesh(Mesh *mesh, const String &name = "Object")
        {
            return scene->createMeshNode(mesh, name);
        }

        SceneGraph *build()
        {
            return scene;
        }
    };

}

