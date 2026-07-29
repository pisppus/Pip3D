#pragma once

#include <cstdint>
#include <cstddef>

namespace pipcore::audio
{

    enum PACFlags : uint16_t
    {
        PAC_FLAG_LOOP = (1u << 0),
    };

    enum PACBlockMode : uint8_t
    {
        PAC_MODE_SILENCE = 0,
        PAC_MODE_ADPCM2 = 1,
        PAC_MODE_ADPCM4 = 2,
        PAC_MODE_ADPCM6 = 3,
        PAC_MODE_HOLD = 4,
    };

    struct alignas(4) PACHeader
    {
        char magic[4];
        uint16_t version;
        uint16_t flags;
        uint32_t sourceRate;
        uint32_t nativeRate;
        uint32_t frameCount;
        uint32_t loopStart;
        uint32_t loopEnd;
        uint32_t blockCount;
        uint32_t dataOffset;
        uint32_t dataSize;
        uint32_t reserved1;
        uint32_t reserved2;

        [[nodiscard]] inline bool isValid() const noexcept
        {
            return magic[0] == 'P' && magic[1] == 'A' &&
                   magic[2] == 'C' && magic[3] == '!';
        }
        [[nodiscard]] inline bool isLoop() const noexcept
        {
            return (flags & PAC_FLAG_LOOP) != 0;
        }
    };
    static_assert(sizeof(PACHeader) == 48, "PACHeader must be exactly 48 bytes");

    static constexpr uint32_t PAC_BLOCK_FRAMES = 256;

    struct PacVoiceState
    {

        const uint8_t *data = nullptr;
        const uint8_t *dataEnd = nullptr;
        uint32_t frameCount = 0;
        uint32_t loopStart = 0;
        uint32_t loopEnd = 0;
        bool isLoop = false;

        const uint8_t *blockPtr = nullptr;
        uint32_t currentFrame = 0;
        uint16_t sampleInBlock = 0;

        uint8_t pacMode = PAC_MODE_SILENCE;
        uint16_t blockPayloadBytes = 0;
        int16_t pacPredictor = 0;
        int8_t pacStepIndex = 0;

        uint32_t srcStep = 65536;
        uint32_t srcPhase = 0;
        int16_t prevSrcSample = 0;
        int16_t curSrcSample = 0;
        bool firstSample = true;
        bool isPassthrough = true;
        bool active = false;

        void resetToLoopStart() noexcept;

        [[nodiscard]] int16_t decodeSourceSample() noexcept;

        void decodeMixFrame(int16_t &outL, int16_t &outR) noexcept;

    private:
        void loadNextBlockHeader() noexcept;
    };
}
