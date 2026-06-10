# SLICE CLOSE — Limiter attack DEV knob

**Date:** 2026-06-10
**Status:** implemented locally; SDK committed/pushed; plugin build/install/AU validation clean; audition pending

## Summary
- Added `LimiterEnvelope::setAttackOverrideMs()` in the SDK. `ms > 0` overrides the Character-derived attack; `ms <= 0` returns to mode-derived attack.
- Added plugin DEV parameter `dev_attack_ms` (`0.05..10 ms`, default `3 ms`) and applies it to all four envelopes through the existing configure block.
- The override remains capped by the active lookahead window: low/high use LA Band, wide/wideR use LA Wide.
- Added an orange-strip DEV `Attack` slider and temporarily disabled/greyed the Character segmented control while attack tuning is active.
- Updated `docs/SIGNAL_FLOW.md`, `docs/PROGRESS.md`, and `PROMPTS/PLAN.md`.

## Retrieval
- TOOLS USED: `user-melech_internal`, `user-juce_docs`, `user-melech_dsp`, local file reads/search.
- QUERIES ISSUED: MasterLimiter/SDK path lookup; JUCE `AudioProcessorValueTreeState` / `SliderAttachment`; JUCE `Slider`; shared DSP `LimiterEnvelope attack lookahead release`.
- FILES RETRIEVED: SDK `LimiterEnvelope.{h,cpp}`; plugin `ParameterIDs.h`, `Parameters.cpp`, `PluginProcessor.{h,cpp}`, `MainView.{h,cpp}`; docs and plan files.
- SECTIONS CITED: `LimiterEnvelope::recomputeAttackSamples()`, active lookahead clamp, `processCore()` envelope configuration, APVTS DEV parameter block, and `MainView` DEV strip.
- REUSE CHECK: reused `mdsp_dsp::LimiterEnvelope`; no separate local attack-override component exists.

## Verification Gate
- [x] SDK Release build clean.
- [x] SDK committed and pushed first: `0434a17`.
- [x] Plugin Release build clean.
- [x] AU/VST3 copied to user plug-in folders.
- [x] AU validation clean.
- [ ] Audition confirms Attack changes onset, clamps to lookahead, and Character remains inert.

## Audition Notes
- Fast attack (`0.05..0.3 ms`) should feel snappier/more aggressive.
- Around `3 ms` matches the current Clean default unless clamped by a shorter active lookahead.
- Slower values only take effect when LA Band/Wide are long enough to allow them.
