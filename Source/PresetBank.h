#pragma once
#include <vector>

struct Preset
{
    const char* name;

    // 0..1 macros
    float clock;
    float wordSize;
    float opMorph;
    float mask;
    float jitter;
    float stutter;
    float feedback;
    float density;

    // toggles
    bool limiterOn;

    // extra
    float outputDb; // -24..+12
    float seedNorm; // 0..1 maps to 0..2^31-1
};

static inline std::vector<Preset> makePresetBank()
{
    return {
        { "p[k] fracture",  0.72f, 0.28f, 0.22f, 0.68f, 0.35f, 0.62f, 0.70f, 0.45f, true,  -6.0f, 0.15f },
        { "nato clockbox",  0.58f, 0.40f, 0.08f, 0.55f, 0.22f, 0.28f, 0.55f, 0.35f, true,  -8.0f, 0.35f },
        { "bit-saw wall",   0.80f, 0.18f, 0.55f, 0.30f, 0.40f, 0.35f, 0.85f, 0.30f, true, -10.0f, 0.80f },
        { "edge grinder",   0.40f, 0.60f, 0.92f, 0.52f, 0.15f, 0.10f, 0.45f, 0.18f, true,  -6.0f, 0.55f },
        { "data rain",      0.95f, 0.22f, 0.35f, 0.75f, 0.55f, 0.50f, 0.40f, 0.70f, true, -12.0f, 0.05f },
        { "mute-clicks",    0.15f, 0.10f, 0.05f, 0.20f, 0.10f, 0.05f, 0.25f, 0.08f, true, -12.0f, 0.25f },
        { "no limiter A/B", 0.65f, 0.25f, 0.30f, 0.60f, 0.30f, 0.45f, 0.75f, 0.40f, false, -18.0f, 0.12f },
    };
}
