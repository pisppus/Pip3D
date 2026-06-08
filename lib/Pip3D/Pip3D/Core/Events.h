#pragma once
#include "Core/Platform.h"

namespace pip3D
{
  enum EventType
  {
    EVENT_FRAME_START = 0,
    EVENT_FRAME_END = 1,
    EVENT_MESH_LOADED = 2,
    EVENT_TEXTURE_LOADED = 3,
    EVENT_CAMERA_CHANGED = 4,
    EVENT_SCENE_CHANGED = 5,
    EVENT_MEMORY_LOW = 6,
    EVENT_FPS_CHANGED = 7,
    EVENT_USER_CUSTOM = 100
  };

  struct EventSystem
  {
  private:
    struct Listener
    {
      EventType type;
      void (*callback)(EventType, void *);
      void *userData;
      bool active;
    };

    static constexpr int MAX_LISTENERS = 32;
    static Listener listeners[MAX_LISTENERS];
    static int listenerCount;

  public:
    static bool subscribe(EventType type, void (*callback)(EventType, void *), void *userData = nullptr)
    {
      if (!callback)
        return false;
      if (listenerCount >= MAX_LISTENERS)
        return false;

      listeners[listenerCount] = {type, callback, userData, true};
      listenerCount++;
      return true;
    }

    static void unsubscribe(void (*callback)(EventType, void *))
    {
      for (int i = 0; i < listenerCount; i++)
      {
        if (listeners[i].callback == callback)
        {
          listeners[i].active = false;
        }
      }
    }

    static void emit(EventType type, void *data = nullptr)
    {
      for (int i = 0; i < listenerCount; i++)
      {
        if (listeners[i].active && listeners[i].callback && listeners[i].type == type)
        {
          listeners[i].callback(type, data ? data : listeners[i].userData);
        }
      }
    }

    static void cleanup()
    {
      int writeIdx = 0;
      for (int readIdx = 0; readIdx < listenerCount; readIdx++)
      {
        if (listeners[readIdx].active)
        {
          if (writeIdx != readIdx)
          {
            listeners[writeIdx] = listeners[readIdx];
          }
          writeIdx++;
        }
      }
      listenerCount = writeIdx;
    }

    static int getListenerCount() { return listenerCount; }
  };
}