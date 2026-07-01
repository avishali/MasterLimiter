# SLICE_M2_GR_MASTERING_SCALE — Close record

**Slice:** GR meter mastering scale (make 0.1–3 dB reduction visible)
**Date:** 2026-07-02
**Repos touched:** MasterLimiter only (UI)

## What shipped

1. **Sqrt scale** — `normaliseGr` uses sqrt on 0–12 dB (3 dB → 50% bar height).
2. **Ticks** — −0.5 / −1 / −2 / −3 / −6 / −12 dB reference lines.
3. **Fill** — `fillRect` body (no rounded-corner dead zone); no height threshold.
4. **Ballistics** — release 50 → 180 ms (attack 1 ms, hold 1000 ms unchanged).
5. **Readout** — "total" caption (deepest band × wideband vs per-band bars).

## Verification checklist

- [x] Build clean; AU validates; installed
- [ ] ~0.5–1 dB GR clearly visible on bars
- [ ] Total readout values unchanged; "total" label present
- [ ] avishali audition: mastering-range GR readable at 0.1 dB

## Files changed

- `Source/ui/meters/GainReductionMeter.cpp`
- `docs/SIGNAL_FLOW.md`
- `docs/METERING_ACCURACY_AUDIT.md`
- `docs/PROGRESS.md`
- `PROMPTS/PLAN.md`
