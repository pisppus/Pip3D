#include <Arduino.h>
#include <PipCore/Platforms/Desktop/Runtime.hpp>
#include <windows.h>

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

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return runSimulator();
}

int main()
{
    return runSimulator();
}
