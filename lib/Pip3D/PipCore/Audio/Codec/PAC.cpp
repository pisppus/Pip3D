#include <PipCore/Audio/Codec/PAC.hpp>

namespace pipcore::audio
{
    namespace
    {
        constexpr int8_t kIndexTable[16] = {
            -1, -1, -1, -1, 2, 4, 6, 8,
            -1, -1, -1, -1, 2, 4, 6, 8};

        constexpr int8_t kIndexTable2[4] = {-1, 1, -1, 2};

        constexpr int16_t kStepTable[89] = {
            7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
            19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
            50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
            130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
            337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
            876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
            2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
            5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
            15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

        [[nodiscard]] inline int16_t clampSat16(int32_t v) noexcept
        {
            if (v > 32767)
                return 32767;
            if (v < -32768)
                return -32768;
            return static_cast<int16_t>(v);
        }

        [[nodiscard]] inline int16_t decodeAdpcm4(uint8_t code, int16_t &predictor, int8_t &stepIndex) noexcept
        {
            int16_t step = kStepTable[stepIndex];
            int32_t diff = step >> 3;
            if (code & 4)
                diff += step;
            if (code & 2)
                diff += (step >> 1);
            if (code & 1)
                diff += (step >> 2);
            if (code & 8)
                predictor = clampSat16(static_cast<int32_t>(predictor) - diff);
            else
                predictor = clampSat16(static_cast<int32_t>(predictor) + diff);
            stepIndex += kIndexTable[code & 7];
            if (stepIndex < 0)
                stepIndex = 0;
            else if (stepIndex > 88)
                stepIndex = 88;
            return predictor;
        }

        [[nodiscard]] inline int16_t decodeAdpcm6(uint8_t code, int16_t &predictor, int8_t &stepIndex) noexcept
        {
            int16_t step = kStepTable[stepIndex];
            int32_t diff = step >> 5;
            if (code & 16)
                diff += step;
            if (code & 8)
                diff += (step >> 1);
            if (code & 4)
                diff += (step >> 2);
            if (code & 2)
                diff += (step >> 3);
            if (code & 1)
                diff += (step >> 4);
            if (code & 32)
                predictor = clampSat16(static_cast<int32_t>(predictor) - diff);
            else
                predictor = clampSat16(static_cast<int32_t>(predictor) + diff);
            uint8_t adaptIdx = (code >> 1) & 0x0F;
            stepIndex += kIndexTable[adaptIdx];
            if (stepIndex < 0)
                stepIndex = 0;
            else if (stepIndex > 88)
                stepIndex = 88;
            return predictor;
        }

        [[nodiscard]] inline int16_t decodeAdpcm2(uint8_t code, int16_t &predictor, int8_t &stepIndex) noexcept
        {
            int16_t step = kStepTable[stepIndex];
            int32_t delta = (code & 1) ? step : (step >> 2);
            if (code & 2)
                predictor = clampSat16(static_cast<int32_t>(predictor) - delta);
            else
                predictor = clampSat16(static_cast<int32_t>(predictor) + delta);
            stepIndex += kIndexTable2[code & 0x03];
            if (stepIndex < 0)
                stepIndex = 0;
            else if (stepIndex > 88)
                stepIndex = 88;
            return predictor;
        }
    }

    void PacVoiceState::resetToLoopStart() noexcept
    {
        const uint32_t targetBlock = loopStart / PAC_BLOCK_FRAMES;
        const uint8_t *p = data;
        const uint8_t *end = dataEnd;
        for (uint32_t b = 0; b < targetBlock && p < end; ++b)
        {
            uint8_t hdrByte = *p++;
            uint8_t mode = hdrByte >> 4;
            switch (mode)
            {
            case PAC_MODE_SILENCE:
                break;
            case PAC_MODE_HOLD:
                p += 2;
                break;
            case PAC_MODE_ADPCM2:
                p += 3 + 64;
                break;
            case PAC_MODE_ADPCM4:
                p += 3 + 128;
                break;
            case PAC_MODE_ADPCM6:
                p += 3 + 192;
                break;
            default:
                p = end;
                break;
            }
        }
        blockPtr = p;
        currentFrame = loopStart;
        sampleInBlock = 0;
        firstSample = true;
        srcPhase = 0;
    }

    void PacVoiceState::loadNextBlockHeader() noexcept
    {
        if (blockPtr >= dataEnd)
        {
            pacMode = PAC_MODE_SILENCE;
            pacPredictor = 0;
            blockPayloadBytes = 0;
            sampleInBlock = 0;
            return;
        }

        uint8_t hdr = *blockPtr++;
        pacMode = hdr >> 4;

        switch (pacMode)
        {
        case PAC_MODE_SILENCE:
            pacPredictor = 0;
            blockPayloadBytes = 0;
            break;
        case PAC_MODE_HOLD:
            if (blockPtr + 2 <= dataEnd)
            {
                pacPredictor = static_cast<int16_t>(blockPtr[0] | (blockPtr[1] << 8));
                blockPtr += 2;
            }
            else
                pacPredictor = 0;
            blockPayloadBytes = 0;
            break;
        case PAC_MODE_ADPCM2:
            if (blockPtr + 3 <= dataEnd)
            {
                pacPredictor = static_cast<int16_t>(blockPtr[0] | (blockPtr[1] << 8));
                pacStepIndex = static_cast<int8_t>(blockPtr[2]);
                blockPtr += 3;
                blockPayloadBytes = 64;
            }
            else
            {
                pacMode = PAC_MODE_SILENCE;
                pacPredictor = 0;
                blockPayloadBytes = 0;
            }
            break;
        case PAC_MODE_ADPCM4:
            if (blockPtr + 3 <= dataEnd)
            {
                pacPredictor = static_cast<int16_t>(blockPtr[0] | (blockPtr[1] << 8));
                pacStepIndex = static_cast<int8_t>(blockPtr[2]);
                blockPtr += 3;
                blockPayloadBytes = 128;
            }
            else
            {
                pacMode = PAC_MODE_SILENCE;
                pacPredictor = 0;
                blockPayloadBytes = 0;
            }
            break;
        case PAC_MODE_ADPCM6:
            if (blockPtr + 3 <= dataEnd)
            {
                pacPredictor = static_cast<int16_t>(blockPtr[0] | (blockPtr[1] << 8));
                pacStepIndex = static_cast<int8_t>(blockPtr[2]);
                blockPtr += 3;
                blockPayloadBytes = 192;
            }
            else
            {
                pacMode = PAC_MODE_SILENCE;
                pacPredictor = 0;
                blockPayloadBytes = 0;
            }
            break;
        default:
            pacMode = PAC_MODE_SILENCE;
            pacPredictor = 0;
            blockPayloadBytes = 0;
            break;
        }

        sampleInBlock = 0;
    }

    int16_t PacVoiceState::decodeSourceSample() noexcept
    {
        if (currentFrame >= frameCount)
        {
            if (isLoop && loopEnd > loopStart)
                resetToLoopStart();
            else
            {
                active = false;
                return 0;
            }
        }

        if (sampleInBlock == 0)
            loadNextBlockHeader();

        int16_t out = 0;
        switch (pacMode)
        {
        case PAC_MODE_SILENCE:
            out = 0;
            break;
        case PAC_MODE_HOLD:
            out = pacPredictor;
            break;
        case PAC_MODE_ADPCM4:
            if (sampleInBlock == 0)
            {
                out = pacPredictor;
            }
            else
            {
                uint16_t codeIdx = sampleInBlock - 1;
                uint16_t byteIdx = codeIdx >> 1;
                if (blockPtr + byteIdx < dataEnd)
                {
                    uint8_t b = blockPtr[byteIdx];
                    uint8_t code = (codeIdx & 1) ? (b >> 4) : (b & 0x0F);
                    out = decodeAdpcm4(code, pacPredictor, pacStepIndex);
                }
                else
                    out = pacPredictor;
            }
            break;
        case PAC_MODE_ADPCM2:
            if (sampleInBlock == 0)
            {
                out = pacPredictor;
            }
            else
            {
                uint16_t codeIdx = sampleInBlock - 1;
                uint16_t byteIdx = codeIdx >> 2;
                if (blockPtr + byteIdx < dataEnd)
                {
                    uint8_t b = blockPtr[byteIdx];
                    uint8_t shift = (codeIdx & 3) << 1;
                    uint8_t code = (b >> shift) & 0x03;
                    out = decodeAdpcm2(code, pacPredictor, pacStepIndex);
                }
                else
                    out = pacPredictor;
            }
            break;
        case PAC_MODE_ADPCM6:
            if (sampleInBlock == 0)
            {
                out = pacPredictor;
            }
            else
            {
                uint16_t codeIdx = sampleInBlock - 1;
                uint16_t groupIdx = codeIdx >> 2;
                uint16_t inGroup = codeIdx & 3;
                uint16_t byteBase = groupIdx * 3;
                if (blockPtr + byteBase + 2 < dataEnd)
                {
                    uint32_t chunk = blockPtr[byteBase] | (static_cast<uint32_t>(blockPtr[byteBase + 1]) << 8) | (static_cast<uint32_t>(blockPtr[byteBase + 2]) << 16);
                    uint8_t code = (chunk >> (inGroup * 6)) & 0x3F;
                    out = decodeAdpcm6(code, pacPredictor, pacStepIndex);
                }
                else
                    out = pacPredictor;
            }
            break;
        default:
            out = 0;
            break;
        }

        sampleInBlock++;
        currentFrame++;

        if (sampleInBlock >= PAC_BLOCK_FRAMES)
        {
            sampleInBlock = 0;
            blockPtr += blockPayloadBytes;
        }
        return out;
    }

    void PacVoiceState::decodeMixFrame(int16_t &outL, int16_t &outR) noexcept
    {
        if (isPassthrough)
        {
            int16_t s = decodeSourceSample();
            outL = outR = active ? s : 0;
            return;
        }

        if (firstSample)
        {
            prevSrcSample = decodeSourceSample();
            curSrcSample = prevSrcSample;
            firstSample = false;
            srcPhase = 0;
        }

        while (srcPhase >= 65536u)
        {
            srcPhase -= 65536u;
            prevSrcSample = curSrcSample;
            curSrcSample = decodeSourceSample();
            if (!active)
            {
                outL = outR = 0;
                return;
            }
        }

        int32_t frac = static_cast<int32_t>(srcPhase);
        int32_t diff = static_cast<int32_t>(curSrcSample) - static_cast<int32_t>(prevSrcSample);
        int32_t out = static_cast<int32_t>(prevSrcSample) + ((diff * frac) >> 16);
        outL = outR = static_cast<int16_t>(
            out > 32767 ? 32767 : (out < -32768 ? -32768 : out));
        srcPhase += srcStep;
    }
}
