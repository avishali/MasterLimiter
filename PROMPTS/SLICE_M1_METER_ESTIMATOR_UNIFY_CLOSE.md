# SLICE_M1_METER_ESTIMATOR_UNIFY — Close record

**Slice:** I/O meter estimator unification (number = bar = truth)
**Date:** 2026-07-02
**Repos touched:** MasterLimiter only (product UI + meter floor constant)

## What shipped

1. **SP** — number and bar peak both read `DisplayLevelSmoother.peakDb` (instant rise, 60 dB/s release); removed 0.85 s / 0.95 s-τ `PeakNumericSmoother`.
2. **MAX** — bar yellow max line uses `renderState.maxPeakDb` overridden from `getMax*PeakLDb()` (same as MAX number).
3. **RMS** — number reads direct processor RMS tap; removed `rmsSmooth` second hold; bar stays on `displaySmooth.rmsDb`.
4. **Floor** — `MasterLimiterAudioProcessor::kMeterFloorDb = −120` for atomics, resets, formatters.

## Verification checklist

- [x] Build clean; AU validates; installed
- [ ] Transient: SP returns to bed level within ~250 ms; bar = SP number
- [ ] MAX line = MAX number until Reset peaks
- [ ] RMS number/bar track; −110 dBFS shows −110
- [ ] avishali audition: trustworthy to 0.1 dB

## Files changed

- `Source/PluginProcessor.h` / `.cpp`
- `Source/ui/meters/MeterGroupComponent.h` / `.cpp`
- `docs/SIGNAL_FLOW.md`
- `docs/METERING_ACCURACY_AUDIT.md`
- `docs/PROGRESS.md`
- `PROMPTS/PLAN.md`
