#include "PcDisplayDriver.h"

namespace pip3D
{

    namespace
    {
        PcBlitCallback g_pcBlitCallback = nullptr;

        class PcDisplayDriver : public DisplayDriverBase
        {
        private:
            uint16_t width;
            uint16_t height;

        public:
            PcDisplayDriver() : width(0), height(0) {}

            bool init(const LCD &config) override
            {
                width = config.w;
                height = config.h;
                return g_pcBlitCallback != nullptr;
            }

            void pushImage(int16_t x,
                           int16_t y,
                           int16_t w,
                           int16_t h,
                           uint16_t *buffer) override
            {
                if (!g_pcBlitCallback || !buffer || w <= 0 || h <= 0)
                {
                    return;
                }
                g_pcBlitCallback(x, y, w, h, buffer);
            }

            uint16_t getWidth() const override { return width; }
            uint16_t getHeight() const override { return height; }
        };
    }

    void setPcBlitCallback(PcBlitCallback cb)
    {
        g_pcBlitCallback = cb;
    }

    PcBlitCallback getPcBlitCallback()
    {
        return g_pcBlitCallback;
    }

    DisplayDriverBase *createPcDisplayDriver()
    {
        return new PcDisplayDriver();
    }

}
