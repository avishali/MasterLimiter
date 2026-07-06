# SLICE S-A — `SingleBandLimiter` SDK module (build + null-test in isolation)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (null-test numbers + diff) · **Audition:** n/a (no plugin change this slice)
**Repo/scope:** SDK **`melechdsp-hq`** ONLY — new files under `shared/mdsp_dsp/`. **No plugin change, no edits to existing SDK classes.** Purely additive.
**Why:** ADR-0013 — reusable limiter *topology* must be a composable `mdsp_dsp` module. Today the wideband limiter is hand-composed inline in `MasterLimiter/Source/PluginProcessor.cpp` (LookaheadDelay + inline peak detect + `LimiterEnvelope` + inline gain apply). This slice creates the reusable module and PROVES it equals that composition — **without touching the shipping hot path** (that migration is a later slice). Unblocks the clean K=1 baseline for the spectral-engine bench.

> ⚠️ **Retrieval log first.** Read and output actual signatures/lines for: `LimiterEnvelope.h` (the `process(peakIn, gainOut, n)` gain-computer API + its setters), `PeakDetector.h`, `LookaheadDelay.h`, and the plugin's inline wideband composition in `PluginProcessor.cpp` (~1800–1888: peak build, `envelope_.process`, and the apply `d[i] = delayed * wideGain * ceilingLin`). We are mirroring that recipe exactly.

---

## New SDK module — `mdsp_dsp::SingleBandLimiter`

New files (additive): `shared/mdsp_dsp/include/mdsp_dsp/dynamics/SingleBandLimiter.h` + `shared/mdsp_dsp/src/dynamics/SingleBandLimiter.cpp`.

A **complete single-band limiter** that bundles the exact primitives the plugin composes inline, so a "wideband limiter" is one `SingleBandLimiter` on the full signal:

- **Owns:** a `LookaheadDelay<float>` (per channel), peak detection (max-abs across the linked channels, matching the plugin), a `LimiterEnvelope` (per channel), and the gain application.
- **API (mirror the existing SDK module style — cf. `FinalCeilingLimiter`):**
  - `struct Spec { double sampleRate; int numChannels; int maxBlockSize; int lookaheadSamples; };`
  - `void prepare(const Spec&);`  — ALL allocation here (RT rule §3).
  - `void process(juce::AudioBuffer<float>&) noexcept;`  — in-place; stereo-linked by default.
  - `void reset() noexcept;`
  - Setters forwarding to the internal `LimiterEnvelope`: `setThresholdLinear`, `setReleaseMs`/release-engine selection, attack setters, `setMode` — expose the SAME knobs `LimiterEnvelope` already has (do not invent new behaviour).
  - `void setCeilingLinear(float)` — the output ceiling multiply (matches the plugin's `ceilingLin`).
  - `int getLatencySamples() const noexcept;` — = lookahead (+ any detector latency; here detector is 0).
  - `float getLastBlockMaxGrDb() const noexcept;` — forward from the envelope.
- **Behaviour = the plugin's wideband recipe, exactly:** delay the signal by lookahead; compute the lookahead peak; `envelope_.process` → gain; output = `delayedSignal * gain * ceilingLinear`. Stereo-link = shared peak/gain across channels (with an option to run per-channel later; default linked).
- **RT-safe, no locks, no allocation in `process()`.** Report exact integer latency.

## Unit test (the null-test — this is the deliverable's proof)

Add a test (follow the SDK's existing test setup / framework — check `shared/mdsp_dsp/tests/` or the CMake test target and mirror it): **`SingleBandLimiterTest`**.

- Build a **reference inline composition** in the test — a `LookaheadDelay` + `PeakDetector` + `LimiterEnvelope` + the same `delayed*gain*ceiling` apply, configured identically.
- Feed BOTH the reference and `SingleBandLimiter` the same signals: (a) a −1 dBFS-exceeding transient burst, (b) a sustained tone at 0 dB, (c) pink-ish noise, (d) silence.
- **GATE:** `SingleBandLimiter` output == reference output to **≤ −120 dB** (ideally bit-identical). Also assert: latency matches, `reset()` returns to a clean state, and no allocation in `process()` (if the test harness supports an allocation check, use it; else note it).

## Build & verify
```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
cmake --build build --config Release 2>&1 | tail -6   # or the SDK's test build target
ctest --output-on-failure -R SingleBandLimiter 2>&1 | tail -20
```
**Acceptance (Claude verifies):**
1. SDK builds clean, no new warnings; only NEW files added (no edits to existing SDK classes).
2. `SingleBandLimiterTest` passes: null-test ≤ −120 dB vs the reference composition on all four signals; latency correct; RT-safe.
3. Header documents the module (what it owns, latency, RT contract) in the style of `FinalCeilingLimiter.h`.

## Non-goals (do NOT do)
- **No plugin change.** Wiring the plugin's wideband stage to use `SingleBandLimiter` is a SEPARATE later slice (behaviour-preserving, plugin-output null-test).
- No `MultibandLimiter` yet (slice S-B).
- No new DSP behaviour — this is a faithful bundling of existing primitives.
- Do not edit `LimiterEnvelope`, `FinalCeilingLimiter`, or any Quell/StftEngine files.

## Output requirements
1. Retrieval log (signatures + the plugin's inline recipe lines). 2. New header + cpp. 3. Test file + the null-test dB numbers on all four signals. 4. Build + ctest output. 5. Reported latency. 6. Confirm no existing files edited. 7. Open questions.

## Notes for the architect (not for Cursor)
- Cross-product: additive only; Quell/DeNoiser unaffected (no shared edits). Commit SDK-local; **do NOT push** (Quell hold).
- Next after S-A passes: **S-B** `MultibandLimiter` (composes N `SingleBandLimiter` + LR/crossover split; safety stage optional), then **S-C** the wav-in/wav-out bench that runs K=1 vs multiband on jazz/EDM for the §4a measurement. Plugin migration to these modules is its own later behaviour-preserving arc.
