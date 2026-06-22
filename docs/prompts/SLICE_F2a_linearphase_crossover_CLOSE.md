# SLICE CLOSE — F-2a: LinearPhaseCrossover engine + unit test (SDK only)

**Closed:** 2026-06-22 · **Repo:** melechdsp-hq (SDK only — no plugin integration)

## What shipped
- `LinearPhaseCrossover` — 2-band complementary split via subtraction trick:
  `low = FIR_lowpass(x)`, `high = delay(x,M) - low`, `low+high == xDelayed`.
- `processSample(ch, x, low, high, xDelayed)` — JUCE-compatible call shape + delayed ref out-param for F-2b.
- `Test_LinearPhaseCrossover.cpp` — 5 gate checks (complementary sum, Color blend flatness, band split, latency, symmetry).

## Files
- `shared/mdsp_dsp/include/mdsp_dsp/filters/LinearPhaseCrossover.h`
- `shared/mdsp_dsp/tests/Test_LinearPhaseCrossover.cpp`
- `shared/mdsp_dsp/tests/CMakeLists.txt`

## Gate
- [x] `mdsp_dsp_tests "Crossover"` — all 5 subtests pass
- [x] Full `mdsp_dsp_tests` — exit 0
- [x] PluginProcessor **not** touched (F-2b pending review)

## Next
**F-2b** — wire into `detectCrossover_`/`applyCrossover_`, fix `xDelayed` Color blend, detection alignment, PDC. Measure on offline rig before shipping.
