# SLICE CLOSE — Lookahead Envelope-Follower Release A/B

**Date:** 2026-06-09  
**Status:** implemented locally; Release build clean; AU/VST3 installed for audition.

## Summary

Added a temporary dev-gated lookahead release engine for `LimiterEnvelope`.
Default remains `Adaptive`, so the current adaptive-sigma behavior is preserved
until the dev toggle is switched to `Lookahead`.

## Retrieval Log

- TOOLS USED: `user-melech_internal`, `user-juce_docs`, `user-melech_dsp`
- QUERIES ISSUED: `LimiterEnvelope`, `PluginProcessor.cpp`,
  `PluginEditor.cpp`, JUCE `AudioProcessorValueTreeState`,
  `AudioParameterChoice`, `AudioParameterFloat`, shared DSP
  `LimiterEnvelope` and lookahead/release reuse searches
- FILES RETRIEVED: `LimiterEnvelope.{h,cpp}`, `ParameterIDs.h`,
  `Parameters.cpp`, `PluginProcessor.{h,cpp}`, `MainView.{h,cpp}`,
  `PROMPTS/PLAN.md`, `docs/PROGRESS.md`,
  `PROMPTS/SLICE_LOOKAHEAD_RELEASE.md`
- SECTIONS CITED: `LimiterEnvelope::process()` release branches,
  `LimiterEnvelope::prepare()` preallocation, `processCore()` envelope
  configuration lambda, APVTS dev parameter block, and `MainView` DEV RELEASE
  strip setup/layout
- REUSE CHECK: reused the existing `mdsp_dsp::LimiterEnvelope` and product
  envelope topology. I checked the local library but found no separate existing
  lookahead release/sliding-window-min implementation, so the new release mode
  was added inside `LimiterEnvelope`.

## Delivered

- `LimiterEnvelope::ReleaseEngine` with `AdaptiveSigma` and `LookaheadFollower`.
- Preallocated sliding-window minimum over the lookahead horizon.
- Fixed-time 2..4 pole lookahead release cascade.
- Dev params:
  - `dev_release_engine`
  - `dev_la_release_ms`
  - `dev_la_release_poles`
- Processor wiring for all four envelopes.
- `DEV RELEASE - TEMP` strip controls: Engine, LA Release, Poles, plus the
  existing legacy sigma/scale controls.
- `docs/PROGRESS.md` and `PROMPTS/PLAN.md` updated.

## Gate

- [x] `cmake --build build` clean
- [x] AU copied to `~/Library/Audio/Plug-Ins/Components/`
- [x] VST3 copied to `~/Library/Audio/Plug-Ins/VST3/`
- [ ] Audition: Color 100, Auto Transparent, A/B Adaptive vs Lookahead
- [ ] Pick winning LA Release ms + Poles for the bake/removal pass

## Audition Loop

On the acoustic mix at Color 100, Auto Transparent:

1. A/B Engine: `Adaptive` vs `Lookahead`.
2. With Lookahead on, sweep `LA Release` from low values upward.
3. Try `Poles` 2, 3, and 4.
4. Report winning `LA Release` and `Poles` so the constants can be baked and
   temporary dev params removed for `0.4`.
