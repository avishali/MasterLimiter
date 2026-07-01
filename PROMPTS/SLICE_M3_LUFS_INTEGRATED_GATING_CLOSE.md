# SLICE_M3_LUFS_INTEGRATED_GATING — Close record

**Slice:** Integrated LUFS BS.1770-4 §4 gating (fix systematically-low "I")
**Date:** 2026-07-02
**Repos touched:** melechdsp-hq (SDK) + MasterLimiter (docs; submodule bump pending)

## What shipped

1. **Gating blocks** — 400 ms K-weighted mean-square blocks, 100 ms hop (75% overlap).
2. **Absolute gate** — keep blocks with `l_j ≥ −70 LUFS`.
3. **Relative gate** — threshold = abs-gated loudness − 10 LU; integrated = mean of blocks passing both.
4. **Silence** — no blocks pass absolute gate → floor −100 LUFS.
5. **RT safety** — preallocated ring (`kMaxGatingBlocks = 72000`, ~2 h); no heap after `prepare()`.
6. **Unchanged** — Momentary (0.4 s) and Short-term (3 s) window paths.

## Verification checklist

- [x] SDK build clean
- [x] `mdsp_dsp_tests "Loudness"` — all 5 cases pass (loud+silence, silence floor, abs gate, −23 LUFS calibration ±0.1, M/S regression)
- [ ] SDK committed/pushed; MasterLimiter submodule bumped
- [x] Plugin build clean; AU validates; installed
- [ ] avishali audition: "I" agrees with youlean/Insight on a real master (~0.1–0.3 LU)

## Files changed

**SDK (melechdsp-hq)**
- `shared/mdsp_dsp/include/mdsp_dsp/loudness/LoudnessAnalyzer.h`
- `shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp`
- `shared/mdsp_dsp/tests/Test_LoudnessIntegratedGating.cpp`
- `shared/mdsp_dsp/tests/CMakeLists.txt`

**Plugin (MasterLimiter)**
- `docs/METERING_ACCURACY_AUDIT.md`
- `docs/PROGRESS.md`
- `docs/SIGNAL_FLOW.md`
- `PROMPTS/PLAN.md`

## REUSE CHECK

No existing gating implementation in SharedCode; custom BS.1770-4 §4 algorithm added to `LoudnessAnalyzer`.
