# GlitchNoiseJUCE

Integer and bitwise digital glitch/noise audio effect built as a JUCE plug-in.

## Features
- Integer / bitwise-centric glitch engine (Q23-ish fixed-point)
- Clocked sample/hold + stepped clock table (OS9/Max-ish discontinuity)
- Micro-buffer abuse (stutter/scrub/jump) driven by algebraic-ish pointer updates
- 1–8 sample feedback ring (aggressive)
- Safety stage: DC blocker + soft-clip + optional limiter (toggle for A/B)
- WebView (WebBrowserComponent) “OS9-ish” minimal GUI
- Preset bank + Randomize (with per-parameter lock toggles) + Panic

## Safety notes
- Keep **Limiter ON** by default.
- Start with monitoring level low.
- Use **Panic** if anything runs away.

## Build

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DGLITCHNOISEJUCE_BUILD_PLUGIN=ON -DGLITCHNOISEJUCE_BUILD_TESTS=ON
cmake --build build/release --target GlitchNoiseJUCE_Artifacts GlitchNoiseJUCEIntegrationTests --parallel 2
ctest --test-dir build/release --output-on-failure
```

Artifacts are staged under `artifacts/Release/`.

## Identity

- Company: `EsionHsrahLatigid`
- Manufacturer code: `EHL_`
- Plugin code: `GlNJ`
- Bundle ID: `jp.ehl.glitchnoisejuce`
