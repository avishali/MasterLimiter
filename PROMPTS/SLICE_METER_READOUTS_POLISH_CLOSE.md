# SLICE_METER_READOUTS_POLISH — Close record

**Slice:** I/O meter readouts polish (labels, peak-max hold, ballistics) + GR panel frame
**Date:** 2026-07-02
**Repos touched:** MasterLimiter only (UI + processor max-peak atomics)

## What shipped

1. **Labels** — each I/O readout row captioned TP / SP / MAX / RMS; values right-aligned in a column.
2. **MAX hold** — processor-latched `maxInput/OutputPeak{L,R}Db_` atomics (parity with TP); cleared on Reset peaks.
3. **Ballistics** — SP/RMS numeric readouts: ~850 ms hold, ~950 ms release; meter bars unchanged.
4. **GR frame** — panel fill/border matched to I/O meters (`theme.panel` + `rSmall`).

## Verification checklist

- [x] Build clean; AU validates; installed to user folders
- [ ] All four rows labeled; numbers align; no overflow
- [ ] MAX holds after transient until Reset (matches TP)
- [ ] SP/RMS readable (~700 ms–1 s); GR panel matches In/Out
- [ ] avishali audition: readouts clear and labeled

## Files changed

- `Source/PluginProcessor.h` / `.cpp`
- `Source/ui/meters/MeterComponent.cpp`
- `Source/ui/meters/MeterGroupComponent.h` / `.cpp`
- `Source/ui/meters/GainReductionMeter.cpp`
- `docs/SIGNAL_FLOW.md`
- `docs/PROGRESS.md`
- `PROMPTS/PLAN.md`
