# Vox Warp

Hyperpop vocal mixer effect (VST3) by Tell Your Friends.

Loads as a plain hard-tune. Everything else starts at zero:

- **TUNE** — snap (retune speed), shift, formant, throat, width, key + scale
- **HARMONY** — two extra tuned voices locked to the key/scale, blend, spread, human
- **SHATTER** — harmony cloud (spray, scatter, reverse) and stutter
- **ROT** — crush, decimate, fold
- **MOD A/B/C** — draw a shape, LINK it to any knob. TRIG fires on input hits, SYNC follows the host tempo, FREE runs in Hz. SENS sets how much the input drives it.
- Liquid-chrome visualizer that reacts to the output; REACT sets its sensitivity.

## Build

GitHub Actions builds the Windows VST3 on every push to `main` and publishes it under Releases.

Local build: `cmake -B build && cmake --build build --config Release --target VoxWarp_VST3`
(JUCE 8 is fetched automatically.)
