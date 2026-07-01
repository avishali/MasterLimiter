# SLICE_BAND_GR_VIZ — Close record

**Slice:** Per-band GR visualization (Slice A of 3-band arc)
**Date:** 2026-07-01
**Repos touched:** MasterLimiter only (no SDK / submodule bump)

## What shipped

1. **Processor taps** — `currentGrLowDb_`, `currentGrMidDb_`, `currentGrHighDb_`
   plus per-band max holds, computed from post-Color `gLowOut`/`gHighOut` minima
   in the existing GR loop. Mid stays 0 until `SLICE_3BAND_DSP.md`.
2. **History ring** — `HistoryFrame` extended with `grLowDb`, `grMidDb`,
   `grHighDb`; block-level band maxima aggregated into frames.
3. **GR meter** — LO / MID / HI columns (88×354 footprint unchanged); MID
   reserved with faint `–`; total current/max readout preserved; click resets
   band peak holds + `resetMaxGr()`.
4. **History graph** — band GR stroked traces (amber LO, teal MID, violet HI)
   with legend; faint total-GR fill retained behind band lines.

## Deliberate UX change

GR meter primary view changed from L/R channel bars to per-band bars. L/R
atomics (`currentGrLDb_` / `currentGrRDb_`) remain published for other use.

## Verification checklist

- [ ] `cmake --build build` clean
- [ ] `auval -v aufx MaLm Melc` pass; param count unchanged
- [ ] Reported PDC identical to pre-slice build
- [ ] Color 100 %: LO and HI bars move independently under push
- [ ] Color 0 %: LO and HI bars move together
- [ ] History graph: LO/HI traces + legend; MID flat at 0
- [ ] avishali audition approved

## Files changed

- `Source/PluginProcessor.h`
- `Source/PluginProcessor.cpp`
- `Source/ui/meters/GainReductionMeter.h`
- `Source/ui/meters/GainReductionMeter.cpp`
- `Source/ui/HistoryGraphComponent.h`
- `Source/ui/HistoryGraphComponent.cpp`
- `docs/SIGNAL_FLOW.md`
- `docs/PROGRESS.md`
- `PROMPTS/PLAN.md`
