# SLICE_CLIPPER_PREPOST — Close record

**Slice:** Clipper Pre/Post position select
**Date:** 2026-07-05
**Repos touched:** MasterLimiter only (no SDK)

## What shipped

1. **Param** — `clipper_position` { Pre, Post }, default Pre (frozen ID).
2. **DSP** — `runClipperStage(osBlock)` lambda; called once per block at Pre or Post.
3. **Post site** — after wideband+ceiling writes, before `lookaheadPad_` + downsample.
4. **Metering** — clip GR moves with active site (same atomics).

## Verification checklist

- [x] Build clean; AU validates
- [x] Pre deterministic null (two identical renders, max diff 0)
- [x] Latency Pre=Post=2995 samples (active on/off unchanged)
- [x] Rig: Post crest 3.1 dB vs no-clip 4.4 dB; SP −1.00 dBFS with FC TruePeak
- [ ] avishali audition: Pre vs Post character on program material

## Rig output (2026-07-05)

```
Pre-path null: max |y1-y2| = 0.000e+00 PASS
Latency: Pre=2995 Post=2995 PASS
no clip  SP=-1.00 crest=4.4 dB
Pre clip SP=-1.00 crest=3.1 dB
Post clip SP=-1.00 crest=3.1 dB
```

## Files changed

- `Source/parameters/ParameterIDs.h`
- `Source/parameters/Parameters.cpp`
- `Source/PluginProcessor.{h,cpp}`
- `docs/SIGNAL_FLOW.md`
- `docs/PROGRESS.md`
- `PROMPTS/PLAN.md`
- `tools/analysis/clipper_prepost_verify.py`

## Open questions

- Live Pre↔Post switch may click (single clipper OS state); smooth/duck deferred.
