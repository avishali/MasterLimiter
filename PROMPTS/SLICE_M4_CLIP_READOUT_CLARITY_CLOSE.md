# SLICE_M4_CLIP_READOUT_CLARITY — Close record

**Slice:** Clip readout clarity + hold; meter hygiene
**Date:** 2026-07-02
**Repos touched:** MasterLimiter only (UI/metering)

## What shipped

1. **Label** — readout text `CLIP GR x.x / y.y` (clipper gain-reduction depth, not output overs).
2. **Tooltip** — explains clipper depth vs ceiling-limited output; click resets max.
3. **Hold** — current depth via `PeakHoldModel` (~850 ms hold, 12 dB/s decay); replaces 50 ms `MeterBallistics` release that ignored `clipHoldMs`.
4. **Max latch** — unchanged (`getMaxClipSinceResetDb()` + click reset).
5. **Hygiene** — comment at `setHoldEnabled(true)` documenting intentional bar max latch; FullRange anchor unification deferred (Ozone scale vs provider perceptual table).

## Verification checklist

- [x] Build clean; AU validates; installed
- [x] No DSP/param/latency change
- [ ] avishali audition: clip figure legible and unambiguous

## Files changed

- `Source/ui/meters/ClipBallistics.cpp`
- `Source/ui/MainView.cpp`
- `Source/ui/meters/MeterGroupComponent.cpp`
- `Source/ui/meters/MeterComponent.cpp`
- `docs/METERING_ACCURACY_AUDIT.md`
- `docs/PROGRESS.md`
- `PROMPTS/PLAN.md`
