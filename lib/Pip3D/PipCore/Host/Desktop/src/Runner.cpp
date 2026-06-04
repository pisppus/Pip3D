#include <Arduino.h>
#include <PipCore/Platforms/Desktop/Runtime.hpp>

#if defined(_WIN32)
#undef INPUT
#include <windows.h>
#endif

void setup();
void loop();

namespace
{
    int runSimulator()
    {
        setup();

        auto &runtime = pipcore::desktop::Runtime::instance();
        while (!runtime.shouldQuit())
        {
            loop();
            runtime.delayMs(1);
        }

        return 0;
    }
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return runSimulator();
}
#endif

int main()
{
    return runSimulator();
}
