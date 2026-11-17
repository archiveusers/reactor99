# REACTOR 99

Nuclear-meltdown tube saturation / distortion / filter plugin (VST3, Windows, built with JUCE 8).

## Get the VST3 (no installs, ~5 minutes)

1. Create a free GitHub account and a new repository.
2. Upload everything in this folder to it (including the hidden `.github` folder).
   Easiest: `Add file -> Upload files`, drag the *contents* of the unzipped folder in, commit.
3. Open the repo's **Actions** tab -> **Build REACTOR 99 (Windows VST3)**. Wait for the green check
   (first run takes a few minutes while it downloads JUCE).
4. Open the finished run, scroll to **Artifacts**, download **REACTOR99-VST3-Windows**, unzip.
5. Copy the `REACTOR 99.vst3` **folder** to `C:\Program Files\Common Files\VST3\`.
6. FL Studio: `Options -> Manage plugins -> Find installed plugins`, then add **REACTOR 99** from the plugin database.

## Build locally instead

Needs Visual Studio 2022 (Desktop C++) and CMake:

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Reactor99_VST3
```

Output: `build/Reactor99_artefacts/Release/VST3/REACTOR 99.vst3`
(a `Reactor99_Standalone` target also exists for testing without a DAW).

## Controls

- **CORE DRIVE / BIAS / SAG** - tube stages (triode -> pentode -> sagging power stage, 4x oversampled)
- **INPUT** - drive into the tubes; the **CORE TEMP** gauge follows signal level x drive
- **CONTROL RODS 1-4** - drag the levers down to insert resonant filters (120 Hz / 600 Hz / 2.5 kHz / 8 kHz). Double-click resets. ROUTE toggles pre/post tubes
- **GEIGER (Radiation Leak)** - pink noise floor, bitcrush and geiger clicks; DYNAMIC mode scales it with heat
- **MELTDOWN** - if heat stays above MELT THR the plant goes critical (flashing lamps, screen shake, extra grit). Press **SCRAM** (or wait 6 s) to drop drive and leak to zero, then it ramps back. SCRAM never moves your knobs or automation
- **REDUCE MOTION** - disables shake, flicker and flashing
- Click the scope to switch between oscilloscope and seismograph mode
- A soft limiter always sits at the output, so Meltdown can't blast your speakers

## Notes

- This is v1 source. It has not been compiled or tested on my side, so the first CI run may need small fixes.
- JUCE is AGPLv3 / commercial licensed. Fine for personal use; check the JUCE license before selling or distributing.
