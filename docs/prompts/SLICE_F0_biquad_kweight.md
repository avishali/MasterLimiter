# Cursor Build Prompt — Slice F-0: `Biquad` + frozen BS.1770 K-weight port

**Context:** See `docs/CUSTOM_FILTERS.md` §3.1–3.2 and §5. This is the first slice of the custom filter library. It is a **pure regression port** — remove the JUCE IIR dependency from `LoudnessAnalyzer`, change *nothing* audible. All work is in the SDK submodule `melechdsp-hq/shared/mdsp_dsp/` (source of truth — NOT the generated `build*/_deps/mdsp_sdk/` copies).

## Goal
Build a reusable `mdsp_dsp::Biquad` (Transposed Direct Form II, double-precision state/coeffs) plus RBJ-cookbook designers, then re-point `LoudnessAnalyzer`'s two K-weighting filters at it. LUFS readings must match the current JUCE build to ≤0.01 LU.

## Files to create
1. `include/mdsp_dsp/filters/Biquad.h` (header-only)
   - `class Biquad` with `double` state `s1,s2` and `double` coeffs `b0,b1,b2,a1,a2` (a0 normalized to 1).
   - `void setCoefficients(double b0,double b1,double b2,double a1,double a2)`
   - `void reset()` (zero state)
   - `float processSample(float x)` — TDF-II-T:
     ```
     double y = b0*x + s1;
     s1 = b1*x - a1*y + s2;
     s2 = b2*x - a2*y;
     return (float) y;
     ```
   - No allocation, no locking, header-only, RT-safe.

2. `include/mdsp_dsp/filters/FilterDesign.h` (header-only, `namespace mdsp_dsp::filterdesign`)
   - Free functions returning a small `BiquadCoeffs { double b0,b1,b2,a1,a2; }` struct, computed with the **RBJ Audio EQ Cookbook** formulas (same family JUCE uses, so results match):
     - `BiquadCoeffs highPass(double fs, double freq, double Q)`
     - `BiquadCoeffs highShelf(double fs, double freq, double Q, double gainLinear)`
   - Normalize all coeffs by a0. These two are all F-0 needs; add LP/peak later slices.

## Files to modify
3. `include/mdsp_dsp/loudness/LoudnessAnalyzer.h`
   - Remove `#include <juce_dsp/juce_dsp.h>` if K-weighting was its only use (keep `juce_audio_basics` for `AudioBuffer`). Add `#include <mdsp_dsp/filters/Biquad.h>`.
   - Replace `using Filter = juce::dsp::IIR::Filter<float>;` and the `std::unique_ptr<Filter> preFilter[2]/rlbFilter[2];` members with `Biquad preFilter[2]; Biquad rlbFilter[2];` (value members, no heap).

4. `src/loudness/LoudnessAnalyzer.cpp`
   - `updateFilters()`: build coeffs via the new designers with the **exact frozen values** and push into the Biquads:
     - pre / high-shelf: `highShelf(fs, 1681.974, 1.0/sqrt(2.0), Decibels::decibelsToGain(4.0))`
     - rlb / high-pass: `highPass(fs, 38.135, 0.5003)`
   - `reset()`: call `.reset()` on the four Biquads (drop the null checks — they're values now).
   - `process()`: replace `preFilter[ch]->processSample(x)` / `rlbFilter[ch]->processSample(x)` with `preFilter[ch].processSample(x)` / `rlbFilter[ch].processSample(x)`. Keep everything else (peak, sumSq accumulation, windowing, `unitsToLufs`) byte-identical.

## Constraints
- Do NOT change LUFS math, windowing, atomics, or the metering taps.
- Do NOT touch the crossover, oversampling, or any other filter site — those are later slices.
- Keep `float` audio I/O; only internal Biquad state/coeffs are `double`.
- No allocation in `process`/`processSample`.

## Acceptance gate (must pass before close)
1. **Builds** clean in both the SDK and the MasterLimiter plugin (remember: bump the submodule, both HQ copies).
2. **LUFS regression:** render a reference file through the old build and the new build; integrated + short-term LUFS match to **≤0.01 LU**. (Use the offline rig `tools/analysis/sweep_render.py` or any stable program file.)
3. **No JUCE IIR** remains in `LoudnessAnalyzer` (grep `juce::dsp::IIR` in the loudness dir → empty).
4. Report build + LUFS-match numbers back; do not commit until avishali confirms the A/B match by ear/meter.

## Out of scope (next slices)
`LinearPhaseFIR`, `LinearPhaseCrossover` (F-1/F-2), oversampler port (F-3). Don't start them here.
