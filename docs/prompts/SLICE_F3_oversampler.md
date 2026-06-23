# Cursor Build Prompt — Slice F-3: Custom Linear-Phase Oversampler (replace `juce::dsp::Oversampling`)

**Context:** `docs/CUSTOM_FILTERS.md` §3.5, `FILTER_QUALITY_ROADMAP.md` item F. The crossover arc (F-0…F-2c)
is shipped; the **last** JUCE filter family left is `juce::dsp::Oversampling`. It owns the residual
**~65° HF phase tilt @20 kHz** that remains after the crossover was fixed — caused by JUCE's
`setUsingIntegerLatency(true)` rounding the FIR latency, which leaves a sub-sample delay that grows toward
Nyquist (the *same class of bug* the F-2c crossover alignment fixed). This slice replaces it with our own
**linear-phase polyphase half-band oversampler** built on the already-proven `LinearPhaseFIR`.

SDK work in `melechdsp-hq/shared/mdsp_dsp/`. The SDK already links `juce::juce_dsp` (use `juce::dsp::AudioBlock`
for the up/down interface so the swaps are mechanical).

> Split this slice. **F-3a** (engine + tests, SDK only) is one-shot safe — STOP for review. **F-3b** (swap the
> main limiter oversampler) is the measured integration that fixes the HF phase. F-3c/d are follow-ups.

---

## What's being replaced (exact sites)

JUCE config today (all `filterHalfBandFIREquiripple`, 4× = 2 stages, `setUsingIntegerLatency(true)`):
- **Main limiter** — `PluginProcessor.cpp:161-174`: stage 1 transition **0.03 / −110 dB**, stage 2 **0.10 / −100 dB**.
  Used at `:977` `processSamplesUp(block)→osBlock`, `:1354` `processSamplesDown(block)`. Latency `:176`.
- **TruePeakDetector** (`include/.../TruePeakDetector.h:34`, `src/.../TruePeakDetector.cpp:37/50`): 4× up→peak→down. ×4 instances (IN/OUT L/R). Metering only.
- **FinalCeilingLimiter** (`include/.../FinalCeilingLimiter.h:63`, `src/.../FinalCeilingLimiter.cpp:118/140`): 4× TP detection.

The HF phase the user sees on the OUTPUT comes from the **main limiter oversampler** — so F-3b fixes it.
TruePeak/FinalCeiling are metering/residual-mop-up (F-3c, no output-phase impact).

---

## F-3a — `HalfbandPolyphaseOS` engine + tests (SDK only, one-shot safe)

### Create `include/mdsp_dsp/filters/HalfbandPolyphaseOS.h` (+ `.cpp` if needed)

A 2-stage, 4× oversampler from cascaded **linear-phase half-band FIR** stages. Mirror the JUCE API so the
swaps are drop-in:
- `void prepare(int numChannels, int maxBlockSize)` / `initProcessing` equivalent — allocate here.
- `juce::dsp::AudioBlock<float> processSamplesUp(juce::dsp::AudioBlock<float> input)` — returns the 4× block.
- `void processSamplesDown(juce::dsp::AudioBlock<float> output)` — decimates the (processed) OS block back.
- `int getLatencyInSamples() const` — **exact** host-rate group delay (see latency rule below).
- `int getOversamplingFactor() const { return 4; }`, `void reset()`.

**Half-band design (cheap, reuse F-1):** a windowed-sinc low-pass at normalised cutoff **fc = 0.25**
(`kaiserLowpass` with `cutoffHz = sampleRate/4`) is *already* a half-band kernel — every even-index tap is
≈0 except the center. Exploit that:
- **2× upsample:** the even polyphase branch is a pure delay (center tap); the odd branch is a short FIR
  (`LinearPhaseFIR`, direct fold-and-add — these kernels are tens of taps, NOT 14k, so **no FFT needed**).
- **2× decimate:** the polyphase transpose.
- Cascade two 2× stages for 4× (stage 1 tighter, stage 2 wider — match the JUCE 0.03/−110 and 0.10/−100
  baselines so audio quality ≥ today: passband flat to 20 kHz, stopband ≥ −100 dB).
- Kaiser may need a few more taps than JUCE's equiripple for the same attenuation — acceptable for v1; a
  half-band equiripple (Remez) designer is a later refinement. Log the resulting tap counts.

**LATENCY RULE (the whole reason F-3 fixes the HF phase):** report the *exact* group delay, and ensure the
**total up+down latency is an exact integer number of host samples** — pad if necessary (add an integer
host-rate delay) so the host PDC has **zero sub-sample residual**. Do NOT round like JUCE's
`setUsingIntegerLatency`. This is what removes the HF tilt.

### Create `tests/Test_HalfbandPolyphaseOS.cpp`
Gate:
1. **Round-trip is a pure delay:** up→(no processing)→down of broadband noise == `delay(x, getLatencyInSamples())` to < 1e-4 (flat magnitude AND linear phase — i.e. no HF tilt by construction).
2. **Half-band / anti-alias:** a tone above fs/2 injected into the upsampled domain is rejected ≥ −100 dB; passband (≤ 20 kHz) flat within ~0.1 dB.
3. **Symmetric kernels** (linear phase); even taps ≈ 0 (half-band property) to < 1e-6.
4. **Latency** is an exact integer host samples and a multiple of the OS factor where the consumer needs it.
Add to `tests/CMakeLists.txt`. Run `mdsp_dsp_tests "Halfband"` + full suite (exit 0). STOP for review.

---

## F-3b — swap the main limiter oversampler (measured; fixes the HF phase)

Replace `limiterOversampler_` (`juce::dsp::Oversampling<float>`) with `mdsp_dsp::HalfbandPolyphaseOS` in
`PluginProcessor`. `processSamplesUp`/`processSamplesDown` keep the same shape (`:977`/`:1354`). Update the
latency wiring (`:176`, `limiterOsLatencySamples_`, `baseLatencySamples_`) to the new
`getLatencyInSamples()`, and keep `dryDelay_`/`lookaheadPad_`/PDC consistent.

**Gate (offline rig):**
- **HF phase flat:** re-run the pure-tone phase probe (`tools/analysis/transient_lookahead.py` style / the
  inline probe used in this arc) — the ~65° @20 kHz residual should drop to ≈0.
- **Aliasing/THD ≥ baseline:** `tools/analysis/hammerstein.py` + `sweep_render.py` — passthrough ≈ −119 dB,
  hard-limiting worst component ≈ −81 dB or better; no new aliasing.
- Full `mdsp_dsp_tests` exit 0; MasterLimiter Standalone/AU/VST3 build clean; latency/PDC correct (assert the
  new OS latency divides cleanly where the crossover alignment expects it).

---

## F-3c / F-3d (follow-ups, out of scope here)
- **F-3c:** swap the `TruePeakDetector` (×4) and `FinalCeilingLimiter` oversamplers to the shared engine
  (consistency + anti-alias; no output-phase impact).
- **F-3d:** the roadmap-F wins now unlocked — **move the anti-alias cut up** (flat top octave) and optional
  **4×→8×** for the nonlinear stages. Measure each on the rig.

Close F-3a/F-3b by pasting raw `git status`, test output, and the before/after HF-phase + THD numbers.
