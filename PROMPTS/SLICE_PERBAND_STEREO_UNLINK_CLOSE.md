# SLICE_PERBAND_STEREO_UNLINK — Close record

**Slice:** Per-band stereo (L/R) unlink + L/R-per-band GR meter (Slice A2)
**Date:** 2026-07-01
**Repos touched:** MasterLimiter only (no SDK / submodule bump)

## What shipped

1. **DSP (Stereo mode)** — Per-channel band detection, envelopes (`envelopeLowR_` /
   `envelopeHighR_`), Color blend, and apply when `dev_band_stereo_link_pct` &lt; 100%.
   At 100% (default) uses `max(|L|,|R|)` fast path → bit-identical to Slice A.
   **M/S mode** band path untouched.
2. **DEV param** — `dev_band_stereo_link_pct` (0..100 %, default 100) + dock slider.
3. **GR taps** — Per-band × per-channel atomics; history still uses band max of L/R.
4. **GR meter** — LO/MID/HI groups with L/R sub-bars in existing 88×354 footprint.

## Verification checklist

- [ ] `cmake --build build` clean
- [ ] `auval -v aufx MaLm Melc` pass; latency unchanged vs `a2e6f84`
- [ ] Stereo Band Link 100% null vs Slice A ≤ −100 dBFS
- [ ] M/S bit-identical at all Band Link values
- [ ] Band Link &lt; 100%: hard-pan independence visible on meter + audible
- [ ] avishali audition approved

## Files changed

- `Source/PluginProcessor.h` / `.cpp`
- `Source/parameters/ParameterIDs.h` / `Parameters.cpp`
- `Source/ui/DevControlsComponent.h` / `.cpp`
- `Source/ui/meters/GainReductionMeter.h` / `.cpp`
- `docs/SIGNAL_FLOW.md`
- `docs/PROGRESS.md`
- `PROMPTS/PLAN.md`
