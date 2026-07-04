# SLICE_PERBAND_GR_READOUTS — Close record

**Slice:** Per-band numeric GR readouts (LO / MID / HI)
**Date:** 2026-07-05
**Repos touched:** MasterLimiter only (meter UI)

## What shipped

1. **Per-band readout strip** — 18 px row under bars, above solo row; 3 columns aligned with bandW/bandGap layout.
2. **Values** — `cur / max` per band = max(L, R) of per-band current/max atomics (same convention as history traces).
3. **Smoothing** — independent `tickGrReadoutSmoother` state per band for current; max displayed directly.
4. **Reset** — reset-peaks click clears band smoother state; processor `resetMaxGr()` unchanged path.

## Verification checklist

- [x] Build clean; AU validates; no DSP/latency change
- [x] Per-band cur/max under each bar; total readout unchanged
- [x] Reset peaks clears band max + smoother state
- [ ] avishali audition: legibility at ~55 px column width; voicing usefulness

## Files changed

- `Source/ui/meters/GainReductionMeter.{h,cpp}`
- `docs/SIGNAL_FLOW.md`
- `docs/PROGRESS.md`
- `PROMPTS/PLAN.md`

## Open questions

- Strip height H = 18 px (matches total readout); font 10 px for band line vs 11 px total.
- Full `cur / max` on one line fits at 198/3 ≈ 55 px columns in testing; stacked fallback not implemented.
