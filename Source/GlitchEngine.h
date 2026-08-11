#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

class GlitchEngine
{
public:
    void prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
    {
        sr = sampleRate;
        channels = juce::jmax (1, numChannels);

        // micro buffer: 5ms .. 200ms
        maxMicroSamples = (int) std::ceil (0.2 * sr);
        microBuf.resize ((size_t) channels);
        microWrite.assign ((size_t) channels, 0);
        microRead.assign  ((size_t) channels, 0);

        for (int ch = 0; ch < channels; ++ch)
            microBuf[(size_t) ch].assign ((size_t) maxMicroSamples, 0);

        // short FB delay (8-sample ring)
        for (int ch = 0; ch < channels; ++ch)
        {
            fbRing[(size_t) ch].fill (0);
            fbIdx[(size_t) ch] = 0;
            prevY[(size_t) ch] = 0;
        }

        // DC blocker state
        dcX1.assign ((size_t) channels, 0.0f);
        dcY1.assign ((size_t) channels, 0.0f);

        // limiter state
        limEnv.assign ((size_t) channels, 1.0f);

        reset();
    }

    void reset()
    {
        clockPhase = 0.0f;
        heldA = 0;
        heldB = 0;
        opState = 0;
        blockCounter = 0;

        // deterministic seed default
        rng = 0x12345678u;

        for (int ch = 0; ch < channels; ++ch)
        {
            std::fill (microBuf[(size_t) ch].begin(), microBuf[(size_t) ch].end(), 0);
            microWrite[(size_t) ch] = 0;
            microRead[(size_t) ch]  = 0;
            fbRing[(size_t) ch].fill (0);
            fbIdx[(size_t) ch] = 0;
            prevY[(size_t) ch] = 0;
            dcX1[(size_t) ch] = dcY1[(size_t) ch] = 0.0f;
            limEnv[(size_t) ch] = 1.0f;
        }

        uiOpState.store (0, std::memory_order_release);
        uiClockIndex.store (0, std::memory_order_release);
        uiMicroLenMs.store (0, std::memory_order_release);
    }

    void triggerPanic()
    {
        panicFlag.store (true, std::memory_order_release);
    }

    // UI status (safe snapshots)
    int  getUiOpState()   const { return uiOpState.load (std::memory_order_acquire); }
    int  getUiClockIndex()const { return uiClockIndex.load (std::memory_order_acquire); }
    int  getUiMicroLenMs()const { return uiMicroLenMs.load (std::memory_order_acquire); }

    static inline uint32_t rotateLeftForTest (uint32_t x, int k) noexcept
    {
        return rotl32u (x, k);
    }

    // === parameter setter ===
    void setParams (float clock, float wordSize, float opMorph, float mask, float jitter,
                    float stutter, float feedback, float density, bool limiterOn, float outGainLin)
    {
        pClock    = juce::jlimit (0.0f, 1.0f, clock);
        pWordSize = juce::jlimit (0.0f, 1.0f, wordSize);
        pOpMorph  = juce::jlimit (0.0f, 1.0f, opMorph);
        pMask     = juce::jlimit (0.0f, 1.0f, mask);
        pJitter   = juce::jlimit (0.0f, 1.0f, jitter);
        pStutter  = juce::jlimit (0.0f, 1.0f, stutter);
        pFeedback = juce::jlimit (0.0f, 1.0f, feedback);
        pDensity  = juce::jlimit (0.0f, 1.0f, density);

        pLimiterOn = limiterOn;
        pOutGainLin = juce::jmax (0.0f, outGainLin);
    }

    void setSeed (uint32_t seed)
    {
        rng = (seed == 0 ? 0x1u : seed);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = juce::jmin (buffer.getNumChannels(), channels);

        if (panicFlag.exchange (false))
        {
            reset();
            buffer.clear();
            return;
        }

        // map macros
        const int wordBits = mapWordBits (pWordSize);
        const float fbGain = mapFeedbackGain (pFeedback);
        const float eventP = mapDensityProb (pDensity);
        const float clockHz = steppedClockHz (pClock, pJitter);
        const int microLen  = mapMicroLenSamples (pStutter);
        const int stutJumpRate = mapStutterJumpRate (pStutter, pJitter);

        // publish to UI (lightweight)
        uiOpState.store (opState, std::memory_order_release);
        uiClockIndex.store (stepClockIndex, std::memory_order_release);
        uiMicroLenMs.store ((int) std::lround (1000.0 * (double) microLen / sr), std::memory_order_release);

        // op morph
        const float opPos = pOpMorph * (float) (numOps - 1);
        const int op0 = (int) std::floor (opPos);
        const int op1 = juce::jmin (op0 + 1, numOps - 1);
        const float opMix = opPos - (float) op0;

        for (int i = 0; i < numSamples; ++i)
        {
            // block-ish event: every 64 samples force harsh state change
            blockCounter++;
            if ((blockCounter & 63) == 0)
            {
                opState = (opState + (int) (xorshift32() & 31u) + 7) & 63;
                heldA ^= (int32_t) xorshift32();
                heldB = (int32_t) rotl32u ((uint32_t) heldB ^ xorshift32(), (opState & 31));
            }

            advanceClockAndHold (clockHz, eventP);

            // event-driven state flips
            if (rand01() < eventP)
                opState = (opState + (int) (xorshift32() & 0x3u) + 1) & 63;

            const uint32_t mask = buildMask (pMask, (uint32_t) opState);

            for (int ch = 0; ch < numCh; ++ch)
            {
                float in = buffer.getReadPointer(ch)[i];
                if (! std::isfinite(in)) in = 0.0f;

                int32_t a = floatToQ23 (in);

                // very short feedback (1..8 samples)
                const int fbTap = mapFbTap (pFeedback);
                int32_t fb = fbRing[(size_t) ch][(size_t) ((fbIdx[(size_t) ch] - fbTap) & 7)];

                // wrap add with scaled FB
                a = wrapAdd (a, (int32_t) ((int64_t) fb * (int64_t) (fbGain * 32768.0f) / 32768));

                // secondary signal b: held + rng
                int32_t b = heldB ^ (int32_t) xorshift32();

                // word-size reduction
                a = reduceWord (a, wordBits);
                b = reduceWord (b, wordBits);

                // bitwise bank (morph)
                const int shift = mapShift (pJitter, opState);
                const int32_t y0 = bitOp (a, b, op0, shift, mask, ch);
                const int32_t y1 = bitOp (a, b, op1, shift, mask, ch);
                int32_t y = lerpInt (y0, y1, opMix);

                // micro-buffer abuse
                y = microBufferProcess (y, ch, microLen, stutJumpRate, eventP);

                // stereo coupling (if stereo)
                if (numCh >= 2 && (xorshift32() & 255u) == 0u)
                {
                    const int other = (ch ^ 1);
                    int32_t otherPrev = prevY[(size_t) other];
                    y = (y ^ otherPrev);
                    y = wrapAdd (y, (int32_t) (otherPrev >> 3));
                }

                prevY[(size_t) ch] = y;

                // store into FB ring
                fbRing[(size_t) ch][(size_t) fbIdx[(size_t) ch]] = y;
                fbIdx[(size_t) ch] = (fbIdx[(size_t) ch] + 1) & 7;

                float out = q23ToFloat (y);
                out = dcBlock (out, ch);
                out = softClip (out);

                if (pLimiterOn)
                    out = fastLimiter (out, ch);

                out *= pOutGainLin;
                buffer.getWritePointer(ch)[i] = out;
            }
        }

        // update UI states at end of block too (more responsive)
        uiOpState.store (opState, std::memory_order_release);
        uiClockIndex.store (stepClockIndex, std::memory_order_release);
        uiMicroLenMs.store ((int) std::lround (1000.0 * (double) mapMicroLenSamples (pStutter) / sr), std::memory_order_release);
    }

private:
    double sr = 44100.0;
    int channels = 2;

    float pClock = 0.5f, pWordSize = 0.5f, pOpMorph = 0.0f, pMask = 0.5f, pJitter = 0.0f;
    float pStutter = 0.0f, pFeedback = 0.0f, pDensity = 0.2f;
    bool  pLimiterOn = true;
    float pOutGainLin = 1.0f;

    // S/H and state
    float clockPhase = 0.0f;
    int32_t heldA = 0;
    int32_t heldB = 0;
    int opState = 0;
    int stepClockIndex = 0;

    std::atomic<bool> panicFlag { false };
    int blockCounter = 0;

    // PRNG
    uint32_t rng = 0x12345678u;

    // micro buffer
    int maxMicroSamples = 0;
    std::vector<std::vector<int32_t>> microBuf;
    std::vector<int> microWrite;
    std::vector<int> microRead;

    // short FB ring (8 samples) up to 8 channels
    std::array<std::array<int32_t, 8>, 8> fbRing {};
    std::array<int, 8> fbIdx {};
    std::array<int32_t, 8> prevY {};

    // DC blocker
    std::vector<float> dcX1, dcY1;

    // limiter
    std::vector<float> limEnv;

    // UI atomic meters
    std::atomic<int> uiOpState {0};
    std::atomic<int> uiClockIndex {0};
    std::atomic<int> uiMicroLenMs {0};

    // ===== Utility =====
    static inline int32_t floatToQ23 (float x)
    {
        x = juce::jlimit (-1.0f, 1.0f, x);
        return (int32_t) std::lrintf (x * 8388607.0f);
    }
    static inline float q23ToFloat (int32_t v) { return (float) v / 8388607.0f; }

    static inline uint32_t rotl32u (uint32_t x, int k) noexcept
    {
        k &= 31;
        if (k == 0)
            return x;
        return (x << k) | (x >> (32 - k));
    }

    inline uint32_t xorshift32()
    {
        uint32_t x = rng;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rng = (x == 0 ? 0x1u : x);
        return rng;
    }

    inline float rand01()
    {
        return (float) (xorshift32() & 0x00FFFFFFu) / (float) 0x01000000u;
    }

    static inline int32_t wrapAdd (int32_t a, int32_t b)
    {
        int64_t s = (int64_t) a + (int64_t) b;
        return (int32_t) s;
    }

    static inline int32_t reduceWord (int32_t x, int bits)
    {
        const int drop = juce::jlimit (0, 19, 23 - bits);
        return (x >> drop) << drop;
    }

    static inline int32_t lerpInt (int32_t a, int32_t b, float t)
    {
        return (int32_t) std::lrintf ((float) a + ((float) b - (float) a) * t);
    }

    // ===== Mapping =====
    static inline int mapWordBits (float p)
    {
        const float x = juce::jlimit (0.0f, 1.0f, p);
        return 4 + (int) std::floor (x * 12.999f);
    }

    static inline float mapFeedbackGain (float p)
    {
        const float x = juce::jlimit (0.0f, 1.0f, p);
        return 0.05f + 0.90f * (x * x);
    }

    static inline float mapDensityProb (float p)
    {
        const float x = juce::jlimit (0.0f, 1.0f, p);
        return 0.00002f + 0.02f * (x * x);
    }

    static inline int mapFbTap (float p)
    {
        const float x = juce::jlimit (0.0f, 1.0f, p);
        return 1 + (int) std::floor (x * 7.999f);
    }

    static inline int mapShift (float jitter, int state)
    {
        int k = (state & 15);
        k = (k + (int) std::floor (jitter * 15.0f)) & 31;
        return k;
    }

    float steppedClockHz (float clock, float jitter)
    {
        static constexpr float table[] = {
            2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f, 24.0f,
            32.0f, 48.0f, 64.0f, 96.0f, 128.0f, 192.0f, 256.0f, 384.0f,
            512.0f, 768.0f, 1024.0f, 1536.0f, 2048.0f, 3072.0f, 4096.0f
        };
        constexpr int N = (int) (sizeof(table)/sizeof(table[0]));
        const int idx = juce::jlimit (0, N - 1, (int) std::floor (clock * (float) (N - 1) + 0.0001f));
        stepClockIndex = idx;

        float hz = table[idx];
        const float j = (rand01() * 2.0f - 1.0f) * 0.30f * jitter;
        hz *= (1.0f + j);
        return juce::jmax (0.1f, hz);
    }

    int mapMicroLenSamples (float stutter)
    {
        const float x = juce::jlimit (0.0f, 1.0f, stutter);
        const float ms = 5.0f + 195.0f * (x * x);
        const int n = (int) std::floor (ms * 0.001f * (float) sr);
        return juce::jlimit (8, juce::jmax (8, maxMicroSamples), n);
    }

    int mapStutterJumpRate (float stutter, float jitter)
    {
        const float x = juce::jlimit (0.0f, 1.0f, stutter);
        const float y = juce::jlimit (0.0f, 1.0f, jitter);
        return 2 + (int) std::floor (62.0f * (1.0f - x) * (1.0f - 0.5f*y));
    }

    uint32_t buildMask (float maskParam, uint32_t state)
    {
        const float x = juce::jlimit (0.0f, 1.0f, maskParam);
        const int k = (int) std::floor (x * 31.999f);

        uint32_t m = 0;
        if (x < 0.33f)
        {
            m = (k == 31 ? 0xFFFFFFFFu : ((1u << (k + 1)) - 1u));
        }
        else if (x > 0.66f)
        {
            uint32_t lo = (k == 31 ? 0u : ((1u << (k + 1)) - 1u));
            m = ~lo;
        }
        else
        {
            uint32_t base = 0xAAAAAAAAu;
            m = rotl32u (base, (int) (state & 31));
        }
        return m;
    }

    // ===== S/H and events =====
    void advanceClockAndHold (float clockHz, float eventP)
    {
        const float inc = clockHz / (float) sr;
        clockPhase += inc;

        const bool forced = (rand01() < eventP * 0.25f);

        if (clockPhase >= 1.0f || forced)
        {
            clockPhase -= std::floor (clockPhase);
            heldA = (int32_t) xorshift32();
            heldB = (int32_t) rotl32u (xorshift32(), (opState & 31));
        }

        if (pJitter > 0.0f && (xorshift32() & 31u) == 0u)
            heldB ^= (int32_t) (xorshift32() & 0x00FFFFFFu);
    }

    // ===== bitwise bank =====
    static constexpr int numOps = 12;

    int32_t bitOp (int32_t a, int32_t b, int op, int shift, uint32_t mask, int ch)
    {
        const uint32_t ua = (uint32_t) a;
        const uint32_t ub = (uint32_t) b;
        const uint32_t um = mask;
        const int32_t py = prevY[(size_t) ch];

        switch (op)
        {
            case 0:  return (int32_t) (ua ^ (ub >> shift));
            case 1:  return (int32_t) (ua & um);
            case 2:  return (int32_t) (ua | um);
            case 3:  return (int32_t) (rotl32u (ua, shift) ^ um);
            case 4:  return (int32_t) (ua + (ub << shift));
            case 5:  return (int32_t) ((ua ^ um) + (ub >> shift));
            case 6:  return (int32_t) ((ua & (ub >> shift)) ^ um);
            case 7:  return (int32_t) (rotl32u (ua ^ ub, shift) | um);

            // Added “Max-ish” discontinuous ops:
            case 8:  // delta
            {
                int32_t y = (int32_t) (a - py);
                return y ^ (int32_t) um;
            }
            case 9:  // edge gate
            {
                int32_t d = (int32_t) (a - py);
                uint32_t ad = (uint32_t) (d < 0 ? -d : d);
                if (ad > 0x00200000u)
                    return (int32_t) (ua ^ 0x7FFFFFu) | (int32_t) (um & 0x00FFFFFFu);
                return (int32_t) ua;
            }
            case 10: // change
            {
                return (a != py) ? (int32_t) (ub ^ um) : (int32_t) ua;
            }
            case 11: // xor-fold
            {
                uint32_t t = ua ^ (ua >> (shift & 15));
                t = rotl32u (t, (shift + 5) & 31);
                return (int32_t) (t ^ um);
            }
            default: return a;
        }
    }

    // ===== micro-buffer abuse =====
    int32_t microBufferProcess (int32_t x, int ch, int microLen, int jumpRate, float eventP)
    {
        auto& buf = microBuf[(size_t) ch];
        int& w = microWrite[(size_t) ch];
        int& r = microRead[(size_t) ch];

        buf[(size_t) w] = x;
        w++;
        if (w >= microLen) w = 0;

        const int baseOff = 1 + (int) (xorshift32() % (uint32_t) juce::jmax (2, microLen - 1));
        int target = w - baseOff;
        if (target < 0) target += microLen;

        const bool jump = ((int) (xorshift32() & 0x3Fu) == 0) || (rand01() < eventP);
        if (jump)
        {
            uint32_t idx = (uint32_t) (r < 0 ? 0 : r);
            idx = (idx * 1103515245u + 12345u) % (uint32_t) microLen;
            r = (int) idx;

            if (pStutter > 0.5f && (xorshift32() & 1u))
            {
                int back = 1 + (int) (xorshift32() % 64u);
                r -= back;
                while (r < 0) r += microLen;
            }
        }
        else
        {
            if ((iabs(r - target) > 8) && ((xorshift32() % (uint32_t) jumpRate) == 0u))
                r = target;
            else
                r = (r + 1) % microLen;
        }

        int32_t y = buf[(size_t) r];

        if (pStutter > 0.2f && (xorshift32() & 127u) == 0u)
            y ^= (int32_t) (xorshift32() & 0x0007FFFFu);

        return y;
    }

    static inline int iabs(int x) { return x < 0 ? -x : x; }

    // ===== safety DSP =====
    float dcBlock (float x, int ch)
    {
        const float R = 0.995f;
        const float y = x - dcX1[(size_t) ch] + R * dcY1[(size_t) ch];
        dcX1[(size_t) ch] = x;
        dcY1[(size_t) ch] = y;
        return y;
    }

    static float softClip (float x)
    {
        return (2.0f / juce::MathConstants<float>::pi) * std::atan (x * 3.0f);
    }

    float fastLimiter (float x, int ch)
    {
        const float ceiling = 0.98f;

        float ax = std::fabs (x);
        float env = limEnv[(size_t) ch];

        const float atk = 0.02f;
        const float rel = 0.9995f;
        const float target = (ax > ceiling ? ceiling / juce::jmax (ax, 1e-6f) : 1.0f);

        if (target < env) env = env + atk * (target - env);
        else             env = env * rel + (1.0f - rel) * target;

        limEnv[(size_t) ch] = juce::jlimit (0.0f, 1.0f, env);
        return x * limEnv[(size_t) ch];
    }
};
