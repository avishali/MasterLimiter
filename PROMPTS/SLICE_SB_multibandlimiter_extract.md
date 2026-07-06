# SLICE S-B — `MultibandLimiter` SDK module (build + null-test in isolation)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (null-test numbers + diff) · **Audition:** n/a (no plugin change this slice)
**Repo/scope:** SDK **`melechdsp-hq`** ONLY — new files under `shared/mdsp_dsp/`. **No plugin change, no edits to existing SDK classes.** Purely additive.
**Why:** ADR-0013 — the composite multiband topology must be a reusable `mdsp_dsp` module. This composes the just-shipped `SingleBandLimiter` (commit `6b17a27`) with a reconstruction-correct band split, with the **safety stage as an OPTIONAL appended module (not baked in)** — the thing that finally lets us run a clean per-band-vs-wideband test (S-C) on real DSP. Uses `LinkwitzRileyBandSplitter` (0-latency IIR, sums to unity) — the honest, simplest test vehicle; the LinearPhase-crossover tree is a later option.

> ⚠️ **Retrieval log first.** Read and output actual signatures for: `filters/LinkwitzRileyBandSplitter.h` (esp. `prepare`, `setNumBands`, `setCrossoverFrequencies`/`setDefaultCrossovers`, `splitSample(ch, x, bandOut[])`, `sumBands(...)`, `getNumBands`), and `dynamics/SingleBandLimiter.h` (the API you'll compose). Confirm the LR splitter is allpass-complementary (0 latency, bands sum to the input).

---

## New SDK module — `mdsp_dsp::MultibandLimiter`

New files (additive): `shared/mdsp_dsp/include/mdsp_dsp/dynamics/MultibandLimiter.h` + `src/dynamics/MultibandLimiter.cpp`.

Composes: **`LinkwitzRileyBandSplitter` → N × `SingleBandLimiter` (one per band) → sum**, plus an **optional** trailing safety `SingleBandLimiter` on the summed output.

- **Owns:** one `LinkwitzRileyBandSplitter`, `numBands` × `SingleBandLimiter`, N stereo scratch band-buffers (allocated in `prepare()`), and one optional safety `SingleBandLimiter`.
- **Spec / API (mirror `SingleBandLimiter` conventions):**
  - `struct Spec { double sampleRate; int numChannels; int maxBlockSize; int numBands; int lookaheadSamples; };`
  - `void prepare(const Spec&);` — ALL allocation here. `numBands` up to the LR splitter max (8).
  - `void process(juce::AudioBuffer<float>&) noexcept;` — in-place. Per block: split input into N band buffers (via `splitSample` looped, or the splitter's block API if present) → run each band buffer through its `SingleBandLimiter` → **sum** the limited bands → (if safety enabled) run the sum through the safety limiter.
  - `void reset() noexcept;`
  - `void setCrossoverFrequencies(const float* hz, int count) noexcept;` / `void setDefaultCrossovers(float lowHz, float highHz) noexcept;`
  - **Per-band access (the §4a knobs):** `SingleBandLimiter& band(int i) noexcept;` — the composer configures each band's threshold / **release** / engine / attack directly. (Per-band RELEASE is the whole point — expose it via this handle, do not flatten to one shared value.)
  - **Optional safety:** `void setSafetyEnabled(bool) noexcept;` + `SingleBandLimiter& safety() noexcept;` — a trailing wideband limiter the composer may append. **Default: DISABLED** (so the pure multiband path is the default → clean per-band test). NOT baked into the band loop.
  - `int getLatencySamples() const noexcept;` — LR split = 0; = band lookahead, **+ safety lookahead when safety enabled** (account for it exactly).
  - `float getBandGrDb(int i) const noexcept;` (forward each band's `getLastBlockMaxGrDb`).
- **RT-safe:** no allocation/locks in `process()`. Report exact integer latency.

## Unit test — `MultibandLimiterTest` (two null-tests + wiring checks)

1. **RECONSTRUCTION null-test (the critical one — this is what the butter bank failed):** set every band + safety to NO limiting (threshold ≥ 0 dBFS so gain ≡ 1). Output MUST equal the input **delayed by `getLatencySamples()`** to **≤ −120 dB** on pink noise + a transient. This proves LR split+sum is lossless. Do it for N ∈ {2, 3, 4, 8}.
2. **COMPOSITION null-test:** build a reference inline composition (`LinkwitzRileyBandSplitter` + N `SingleBandLimiter` + `sumBands`, same configs) and compare to `MultibandLimiter` on a driven signal (peaks > −1 dBFS), **safety OFF and safety ON**. Gate ≤ −120 dB (bit-identical expected).
3. **Wiring checks:** (a) setting **different per-band release** on two configs yields **different output** (proves per-band release is actually wired, not ignored); (b) latency matches N and safety on/off; (c) `reset()` returns clean.

Follow the SDK test setup used by `Test_SingleBandLimiter.cpp` (same helpers / CMake registration).

## Build & verify
```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
cmake --build build --config Release --target mdsp_dsp_tests 2>&1 | tail -6
./build/shared/mdsp_dsp/tests/mdsp_dsp_tests "MultibandLimiter" 2>&1 | tail -30
```
**Acceptance (Claude verifies):**
1. SDK builds clean, no new warnings; only NEW files + CMake entries (no edits to existing SDK classes).
2. Reconstruction null-test ≤ −120 dB for N ∈ {2,3,4,8}; composition null-test ≤ −120 dB (safety off & on); per-band-release wiring check shows a real delta; latency correct; reset clean.
3. Header documents the module (what it owns, latency incl. safety, RT contract) in the `SingleBandLimiter.h` style.

## Non-goals (do NOT do)
- **No plugin change** (plugin migration is a later behaviour-preserving arc).
- No LinearPhase-crossover variant yet (later option).
- No new limiting behaviour — faithful composition of `SingleBandLimiter` + `LinkwitzRileyBandSplitter` only.
- Do not edit `SingleBandLimiter`, `LimiterEnvelope`, `LinkwitzRileyBandSplitter`, or any Quell/StftEngine files.

## Output requirements
1. Retrieval log (LR splitter + SingleBandLimiter signatures). 2. New header + cpp. 3. Test file + all null-test dB numbers (reconstruction per N, composition safety off/on, per-band delta). 4. Build + test output. 5. Reported latency (safety off & on). 6. Confirm no existing files edited. 7. Open questions.

---

## REVISION 1 / S-B.1 (2026-07-06, after run 1 — architect, VERIFIED) — fix the reconstruction test

Run 1 module is CORRECT and accepted (composition null-tests −200 dB, per-band release wired, latency 240/480, additive scope — all independently re-verified). **But the reconstruction test is wrong on both sides and must be fixed before S-C:**
- My original gate ("unity split+sum == delayed input ≤ −120 dB") is **unachievable for this splitter by design** — the LR tree sum is **allpass (flat magnitude, shifted phase)**, per `LinkwitzRileyBandSplitter.h:14-19`. Comparing an allpass reconstruction to a pure delay is meaningless (my spec error).
- Cursor's substitute ("DUT == inline reference ≤ −120 dB") is **trivially satisfiable** — both use the same splitter, so it proves the module composes correctly but says NOTHING about whether the splitter reconstructs. The "vs delayed input" −1.9…+5.2 dB is consistent with allpass but does not PROVE flat magnitude (the exact property the butter bank failed).

**Add the correct gate — a flat-MAGNITUDE reconstruction test** in `Test_MultibandLimiter.cpp`:
- Unity gains (no limiting). Feed **white noise** (and/or a log sweep). Split → `sumBands` → output.
- Compute the **magnitude response** output-vs-input: FFT both, take `20*log10(|Y[k]|/|X[k]|)` averaged over several windows (Welch), OR simplest robust proxy: **per-octave-band RMS ratio** across ~10 log-spaced bands 30 Hz–18 kHz.
- **GATE: |magnitude deviation| ≤ 0.1 dB** across the band (allow a little more, ≤ 0.3 dB, only at the very edges) for N ∈ {2,3,4,8}. This is what distinguishes benign-allpass (passes) from butter-style mangling (fails hard).
- **Keep** the composition null-test (it's a valid module-correctness check). **Replace** the "vs delayed input <6 dB anti-catastrophe guard" with this magnitude test (or keep it only as an informational log, not a gate).

Keep everything else as-is. Additive, SDK-only, no plugin change. Report the per-N magnitude-deviation numbers.

## Notes for the architect (not for Cursor)
- Cross-product: additive only; commit SDK-local, **do NOT push** (Quell hold).
- After S-B passes → **S-C**: wav-in/wav-out bench running `SingleBandLimiter` (K=1) vs `MultibandLimiter` (N∈{2,4,8}, per-band release, **safety OFF**) on jazz/EDM at matched loudness/TP (SP −1, no FC, overs ≤ 1–2 dB) → I measure 300 ms range vs Ozone 4.68/5.11. That is the real §4a test on production DSP. (IIR path caps at 8 bands; if per-band helps, the many-band/spectral regime is the follow-on.)
