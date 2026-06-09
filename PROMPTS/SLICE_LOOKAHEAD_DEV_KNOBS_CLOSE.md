# CLOSE — Lookahead time DEV knobs

**Date:** 2026-06-10  
**Repos:** SDK `melechdsp-hq` + product `MasterLimiter`  
**Status:** implemented; SDK committed/pushed first; product build/install/AU validation clean

## Summary
- Added runtime active-lookahead support to SDK `mdsp_dsp::LimiterEnvelope`.
- Added `dev_lookahead_band_ms` and `dev_lookahead_wide_ms` DEV params.
- Removed the dead `lookahead_ms` parameter from the product APVTS layout.
- Kept plugin reported latency constant at the 12 ms max-window path.
- Added `lookaheadPad_` to absorb `(max - active)` slack for the per-band and wideband stages.
- Exposed LA Band, LA Wide, and Sustain Ratio in the orange DEV strip.

## Files changed
- SDK:
  - `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h`
  - `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`
- Product:
  - `Source/parameters/ParameterIDs.h`
  - `Source/parameters/Parameters.cpp`
  - `Source/PluginProcessor.h`
  - `Source/PluginProcessor.cpp`
  - `Source/ui/MainView.h`
  - `Source/ui/MainView.cpp`
  - `docs/SIGNAL_FLOW.md`
  - `docs/PROGRESS.md`
  - `PROMPTS/PLAN.md`

## Verification
- SDK `cmake --build build`: pass.
- SDK commit/push: `3bda543 mdsp_dsp: allow runtime limiter lookahead window`.
- Product `cmake --build build`: pass.
- Product AU/VST3 install: pass.
- Product `auval -v aufx MaLm Melc`: pass.

## Audition notes
- Defaults remain 7 ms for band and wide lookahead; this should preserve the current voicing.
- Reported latency is fixed to the 12 ms max-window path; sweeping either DEV lookahead knob should not cause host PDC changes.
- Small clicks on knob moves are expected in DEV because delay offset and active envelope window reset immediately.
- FinalCeiling remains fixed and untouched.
