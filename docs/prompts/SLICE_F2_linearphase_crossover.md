# Cursor Build Prompt — Slice F-2: Linear-Phase Complementary Crossover

**Context:** `docs/CUSTOM_FILTERS.md` §3.4 (keystone). Builds on committed F-0 (`Biquad`) and
F-1 (`LinearPhaseFIR` + `FirDesign`, `bbd6d91`/`8c4bd7e`). Goal: replace the IIR
`juce::dsp::LinkwitzRileyFilter` 120 Hz split with a linear-phase complementary crossover so the
**Color knob's intermediate low-end dip is gone** (FILTER_QUALITY_ROADMAP.md item D, backlog #2).

All SDK work is in `melechdsp-hq/shared/mdsp_dsp/` (source of truth — sibling repo, NOT a fetched
copy; MasterLimiter builds against it directly via `MELECHDSP_HQ_ROOT`).

> ⚠️ **Split this slice. Do F-2a first and STOP for review/measurement before F-2b.** F-2b reworks
> the shipping signal path and plugin latency; it must be measured on the offline rig, not one-shot.

---

## F-2a — `LinearPhaseCrossover` engine + unit test (SDK only, safe to one-shot)

### Create `include/mdsp_dsp/filters/LinearPhaseCrossover.h`
A 2-band complementary split built on `LinearPhaseFIR`, using the **subtraction trick**:

```
low  = FIR_lowpass(x)          // group delay M = (N-1)/2
high = delay(x, M) - low       // complement BY SUBTRACTION
=> low + high == delay(x, M)   exact, flat magnitude AND phase
```

Interface (per channel; support 2 channels like the JUCE one):
- `struct Spec { double sampleRate; double cutoffHz; double transitionHz; double stopAttenDb; int numChannels; };`
- `void prepare(const Spec&)` — design the lowpass via `firdesign::kaiserLowpass`, install into one
  `LinearPhaseFIR` per channel, allocate an integer delay line of length M per channel for the
  `delay(x, M)` reference. No allocation after prepare.
- `void reset()`
- `int getLatencySamples() const` — returns M.
- `void processSample(int ch, float x, float& low, float& high)` — `low = fir.processSample(x)`;
  `xDelayed = delayLine.pushPop(x)`; `high = xDelayed - low`. (Same call shape as
  `juce::dsp::LinkwitzRileyFilter::processSample` so the plugin swap in F-2b is mechanical.)
- Also expose `float delayRef(int ch, float x)` OR make the M-delayed input retrievable, because
  F-2b needs `delay(x, M)` for the linked/wideband reference term (see F-2b contract). Simplest:
  `processSample` also writes an out-param `float& xDelayed` (the `delay(x,M)` value), so the caller
  gets low, high, AND the delayed full-band reference in one call.

Use a small fixed integer delay (reuse `mdsp_dsp::LookaheadDelay` or a plain ring buffer); the FIR
and the reference delay MUST share identical M so `low+high == xDelayed` to within 1e-6.

### Create `tests/Test_LinearPhaseCrossover.cpp` (mirror Test_LinearPhaseFIR style)
Gate:
1. **Exact complementary sum:** for white-noise input, `low[n] + high[n] == xDelayed[n]` to < 1e-6 RMS.
2. **Flat reconstruction at every blend:** for `bl in {0, 0.25, 0.5, 0.75, 1}` and unity band gains,
   `bl*xDelayed + (1-bl)*(low+high) == xDelayed` to < 1e-6 (proves no blend-dependent cancellation —
   the core fix).
3. **Band split sanity:** a 40 Hz tone is ~all in `low`; a 1 kHz tone is ~all in `high` (cutoff 120 Hz).
4. **Latency:** `getLatencySamples() == M` and a sub-cutoff tone in `low` is delayed by exactly M.
5. **Linear phase:** lowpass kernel symmetric (inherited from F-1, re-assert via the engine IR).

Add `Test_LinearPhaseCrossover.cpp` to `tests/CMakeLists.txt`.

### F-2a gate (paste raw output when done)
- `git status --short` and `ls include/mdsp_dsp/filters/`
- Build `mdsp_dsp_tests`, run `mdsp_dsp_tests "Crossover"` AND the full `mdsp_dsp_tests` (exit 0).
- Do NOT touch PluginProcessor in F-2a. STOP here for review.

---

## F-2b — plugin integration (DESIGN CONTRACT, do after F-2a is reviewed)

Replace `detectCrossover_` / `applyCrossover_` (`juce::dsp::LinkwitzRileyFilter`,
`PluginProcessor.h:165-166`, prepared at `PluginProcessor.cpp:185-189` with `xoSpec` at
**`osSampleRate`** — note the 4× rate). Usage sites: detect loop `PluginProcessor.cpp:1047-1060`,
apply loop `1078-1090`.

**Critical correctness requirements (these are the slice, not the filter swap):**

1. **Phase-coherent reconstruction.** Today (`:1086`) mixes the *undelayed* `delayed` (linked term)
   with the split bands. With FIR, `low+high == delay(delayed, M)`. So the linked/wideband reference
   MUST become `delay(delayed, M)` (the `xDelayed` out-param from F-2a), NOT `delayed`:
   ```
   bandLimited = linkedGain * xDelayed                          // was: linkedGain * delayed
               + (1-bl) * (low * gainLow[i] + high * gainHigh[i]);
   ```
   Otherwise intermediate Color re-introduces a cancellation — verify this is gone, don't assume.

2. **Detection alignment.** `detectCrossover_` now delays the band-peak streams by M, so the
   envelope gain curve lands M late relative to the audio. Compensate so gain-to-audio alignment is
   preserved (reduce the applied audio delay by M, or advance detection). Measure GR timing before/after.

3. **Latency / PDC.** Add the crossover latency to `baseLatencySamples_` (`:312`), converted from OS
   samples to host samples (`M / osFactor`), and keep `dryDelay_`/`lookaheadPad_` consistent so
   reported latency stays correct and the bypass cross-fade stays sample-aligned. `setLatencySamples`
   at `:333`/`:868`.

4. **CPU reality.** At `osSampleRate` (192 kHz) a 120 Hz linear-phase split is ~6–8k taps; direct
   convolution is ~16× a host-rate split and likely too heavy for RT. **F-2b proves correctness with
   direct-conv on the offline rig first.** If RT CPU is unacceptable, the shippable path is
   FFT-partitioned convolution in `LinearPhaseFIR` (separate slice F-2c) or the multirate low-band
   split — latency is identical either way, so the fix is unaffected.

**F-2b gate (offline rig):**
- `tools/analysis/hammerstein.py` + sweep: Color sweep 0→25→50→75→100 shows the **low-end dip gone**
  at all values (compare against the current build's intermediate-Color dip).
- Band sum flat ±0.01 dB; THD/phase no worse than baseline (−119 dB passthrough, −81 dB hard-limit).
- Full `mdsp_dsp_tests` exit 0; MasterLimiter plugin builds clean (Standalone/AU/VST3).
- Report CPU (% in host) at Color 50 so we can decide if direct-conv ships or we need F-2c.

---

## Out of scope
Oversampler port (F-3), movable freq / 3-band (later), FFT-partition (F-2c if CPU demands it).
