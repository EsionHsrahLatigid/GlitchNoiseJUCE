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
cmake --build build/release --target ehl_stage_products GlitchNoiseJUCEIntegrationTests --parallel 2
ctest --test-dir build/release --output-on-failure
```

Release products are staged by `ehl_stage_products` under:

```text
artifacts/plugin-release/macos-arm64/standalone/glitchnoisejuce_standalone_plugin.app
artifacts/plugin-release/macos-arm64/vst3/glitchnoisejuce_vst3_plugin.vst3
artifacts/plugin-release/macos-arm64/au/glitchnoisejuce_au_plugin.component
artifacts/plugin-release/macos-arm64/ARTIFACTS.txt
artifacts/plugin-release/windows-x64/standalone/glitchnoisejuce_standalone_plugin.exe
artifacts/plugin-release/windows-x64/vst3/glitchnoisejuce_vst3_plugin.vst3
artifacts/plugin-release/windows-x64/ARTIFACTS.txt
```

On local macOS builds outside CI, VST3 and AU formats are also copied to the
current user's standard plug-in folders:

- `~/Library/Audio/Plug-Ins/VST3/GlitchNoiseJUCE.vst3`
- `~/Library/Audio/Plug-Ins/Components/GlitchNoiseJUCE.component`

Standalone remains in the artifact tree. CI and non-macOS builds do not copy by
default. Override with `-DEHL_COPY_PLUGIN_AFTER_BUILD=ON` or `OFF`.

## Identity

- Company: `EsionHsrahLatigid`
- Manufacturer code: `EHL_`
- Plugin code: `GlNJ`
- Bundle ID: `jp.ehl.glitchnoisejuce`
