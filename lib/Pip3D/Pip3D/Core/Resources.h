#pragma once
#include "Core/Platform.h"
#include "Core/Events.h"
#include "Core/Memory.h"

namespace pip3D
{
  enum ResourceType
  {
    RES_TEXTURE = 0,
    RES_MESH = 1,
    RES_SOUND = 2,
    RES_DATA = 3
  };

  struct ResourceManager
  {
  private:
    struct Resource
    {
      char *path;
      void *data;
      size_t size;
      ResourceType type;
      uint32_t refCount;
      uint32_t lastAccess;
      bool loaded;
    };

    static constexpr int MAX_RESOURCES = 64;
    static Resource resources[MAX_RESOURCES];
    static int resourceCount;
    static size_t totalMemory;
    static size_t maxMemory;

  public:
    static void init(size_t maxMem = 1024 * 1024)
    {
      maxMemory = maxMem;
      totalMemory = 0;
      resourceCount = 0;

      for (int i = 0; i < MAX_RESOURCES; i++)
      {
        resources[i] = {nullptr, nullptr, 0, RES_DATA, 0, 0, false};
      }

      LOGI(::pip3D::Debug::LOG_MODULE_RESOURCES,
           "ResourceManager initialized, maxMem=%u bytes, maxResources=%d",
           static_cast<unsigned int>(maxMemory),
           MAX_RESOURCES);
    }

    static void *load(const char *path, ResourceType type, size_t size)
    {
      if (!path)
      {
        LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
             "ResourceManager::load called with null path (type=%d, size=%u)",
             static_cast<int>(type),
             static_cast<unsigned int>(size));
        return nullptr;
      }
      int idx = findResource(path);

      if (idx != -1)
      {
        resources[idx].refCount++;
        resources[idx].lastAccess = millis();
        LOGI(::pip3D::Debug::LOG_MODULE_RESOURCES,
             "Resource '%s' already loaded, refCount=%u", path, resources[idx].refCount);
        return resources[idx].data;
      }

      if (resourceCount >= MAX_RESOURCES)
      {
        cleanup();
        if (resourceCount >= MAX_RESOURCES)
        {
          LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
               "ResourceManager capacity exceeded while loading '%s' (MAX_RESOURCES=%d)",
               path,
               MAX_RESOURCES);
          return nullptr;
        }
      }

      if (totalMemory + size > maxMemory)
      {
        freeOldest();
        if (totalMemory + size > maxMemory)
        {
          LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
               "Not enough memory to load resource '%s' (size=%u, used=%u, max=%u)",
               path,
               static_cast<unsigned int>(size),
               static_cast<unsigned int>(totalMemory),
               static_cast<unsigned int>(maxMemory));
          return nullptr;
        }
      }

      void *data = MemUtils::allocAligned(size);
      if (!data)
      {
        LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
             "MemUtils::allocAligned failed for resource '%s' (size=%u)",
             path,
             static_cast<unsigned int>(size));
        return nullptr;
      }

      char *ownedPath = duplicatePath(path);
      if (!ownedPath)
      {
        MemUtils::freeAligned(data);
        LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
             "Failed to duplicate resource path '%s'",
             path);
        return nullptr;
      }

      idx = resourceCount++;
      resources[idx] = {ownedPath, data, size, type, 1, millis(), true};
      totalMemory += size;

      LOGI(::pip3D::Debug::LOG_MODULE_RESOURCES,
           "Loaded resource '%s' (type=%d, size=%u bytes, totalMemory=%u/%u)",
           path,
           static_cast<int>(type),
           static_cast<unsigned int>(size),
           static_cast<unsigned int>(totalMemory),
           static_cast<unsigned int>(maxMemory));

      EventType ev = EVENT_USER_CUSTOM;
      if (type == RES_TEXTURE)
        ev = EVENT_TEXTURE_LOADED;
      else if (type == RES_MESH)
        ev = EVENT_MESH_LOADED;
      EventSystem::emit(ev, (void *)path);
      return data;
    }

    static void unload(const char *path)
    {
      int idx = findResource(path);
      if (idx != -1)
      {
        Resource &res = resources[idx];
        if (res.refCount == 0)
        {
          LOGW(::pip3D::Debug::LOG_MODULE_RESOURCES,
               "Resource '%s' reached unload() with refCount already at 0; removing stale slot",
               res.path ? res.path : "<null>");
          removeResourceAt(idx);
          return;
        }

        res.refCount--;
        res.lastAccess = millis();

        if (res.refCount == 0)
        {
          const unsigned int freedSize = static_cast<unsigned int>(res.size);
          char releasedPath[96] = {0};
          if (res.path)
          {
            strncpy(releasedPath, res.path, sizeof(releasedPath) - 1);
            releasedPath[sizeof(releasedPath) - 1] = '\0';
          }
          removeResourceAt(idx);

          LOGI(::pip3D::Debug::LOG_MODULE_RESOURCES,
               "Unloaded resource '%s' (size=%u bytes, totalMemory=%u/%u)",
               releasedPath[0] ? releasedPath : "<null>",
               freedSize,
               static_cast<unsigned int>(totalMemory),
               static_cast<unsigned int>(maxMemory));
        }
        else
        {
          LOGI(::pip3D::Debug::LOG_MODULE_RESOURCES,
               "Decreased refCount for resource '%s' to %u",
               res.path ? res.path : "<null>",
               static_cast<unsigned int>(res.refCount));
        }
      }
      else
      {
        LOGW(::pip3D::Debug::LOG_MODULE_RESOURCES,
             "Attempt to unload unknown resource '%s'", path ? path : "<null>");
      }
    }

    static void unloadAll()
    {
      for (int i = 0; i < resourceCount; i++)
      {
        releaseResource(resources[i]);
      }
      resourceCount = 0;
      totalMemory = 0;

      LOGI(::pip3D::Debug::LOG_MODULE_RESOURCES,
           "All resources unloaded, memory usage reset to 0");
    }

    static size_t getMemoryUsage() { return totalMemory; }
    static size_t getMaxMemory() { return maxMemory; }
    static int getResourceCount() { return resourceCount; }

    static void printStatus()
    {
      LOGI(::pip3D::Debug::LOG_MODULE_RESOURCES,
           "Resources: %d/%d, Memory: %u/%u KB",
           resourceCount,
           MAX_RESOURCES,
           static_cast<unsigned int>(totalMemory / 1024u),
           static_cast<unsigned int>(maxMemory / 1024u));

      for (int i = 0; i < resourceCount; i++)
      {
        if (resources[i].loaded)
        {
          LOGI(::pip3D::Debug::LOG_MODULE_RESOURCES,
               "  %s: %u bytes, refs=%u",
               resources[i].path ? resources[i].path : "<null>",
               static_cast<unsigned int>(resources[i].size),
               static_cast<unsigned int>(resources[i].refCount));
        }
      }
    }

  private:
    static int findResource(const char *path)
    {
      for (int i = 0; i < resourceCount; i++)
      {
        if (resources[i].loaded && resources[i].path && strcmp(resources[i].path, path) == 0)
        {
          return i;
        }
      }
      return -1;
    }

    static void cleanup()
    {
      int writeIdx = 0;
      for (int readIdx = 0; readIdx < resourceCount; readIdx++)
      {
        if (resources[readIdx].loaded && resources[readIdx].refCount > 0)
        {
          if (writeIdx != readIdx)
          {
            resources[writeIdx] = resources[readIdx];
            resources[readIdx] = {nullptr, nullptr, 0, RES_DATA, 0, 0, false};
          }
          writeIdx++;
        }
        else
        {
          releaseResource(resources[readIdx]);
        }
      }
      resourceCount = writeIdx;
    }

    static void freeOldest()
    {
      uint32_t oldestTime = UINT32_MAX;
      int oldestIdx = -1;

      for (int i = 0; i < resourceCount; i++)
      {
        if (resources[i].loaded && resources[i].refCount == 0 &&
            resources[i].lastAccess < oldestTime)
        {
          oldestTime = resources[i].lastAccess;
          oldestIdx = i;
        }
      }

      if (oldestIdx != -1)
      {
        removeResourceAt(oldestIdx);
      }
    }

    static char *duplicatePath(const char *path)
    {
      if (!path)
      {
        return nullptr;
      }

      const size_t len = strlen(path);
      char *copy = static_cast<char *>(malloc(len + 1u));
      if (!copy)
      {
        return nullptr;
      }

      memcpy(copy, path, len + 1u);
      return copy;
    }

    static void releaseResource(Resource &res)
    {
      if (res.data)
      {
        MemUtils::freeAligned(res.data);
        if (totalMemory >= res.size)
        {
          totalMemory -= res.size;
        }
        else
        {
          totalMemory = 0;
        }
      }

      if (res.path)
      {
        free(res.path);
      }

      res = {nullptr, nullptr, 0, RES_DATA, 0, 0, false};
    }

    static void removeResourceAt(int idx)
    {
      if (idx < 0 || idx >= resourceCount)
      {
        return;
      }

      releaseResource(resources[idx]);

      for (int i = idx + 1; i < resourceCount; ++i)
      {
        resources[i - 1] = resources[i];
      }

      resourceCount--;
      resources[resourceCount] = {nullptr, nullptr, 0, RES_DATA, 0, 0, false};
    }
  };
}