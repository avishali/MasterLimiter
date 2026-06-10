# SLICE CLOSE — Preset A/B compare + Load-from-file + header polish

**Date:** 2026-06-10
**Status:** implemented locally; build/user install/AU validation clean; system AU copy blocked by stale root-owned bundle; audition pending

## Summary
- Added persistent full-state A/B compare slots backed by APVTS `ValueTree` snapshots.
- Added header **A**, **B**, and copy controls for fast scratch comparison between two complete voicings, including DEV params.
- Added **Load from file...** to the preset menu using async `FileChooser`, rooted at `~/Library/Audio/Presets/MelechDSP/MasterLimiter`.
- Switched combo-box rendering to JUCE V4's stock combo arrow and removed the preset-menu arrow colour override.
- Made Bypass text drawing explicit in `MasterLimiterLookAndFeel` and widened the header button so `Bypassed` is legible.

## Retrieval
- TOOLS USED: `user-melech_internal`, `user-juce_docs`, `user-melech_dsp`, local file reads/search.
- QUERIES ISSUED: MasterLimiter `PluginProcessor`/`MainView`/`PresetManager`/`MasterLimiterLookAndFeel` lookup; JUCE `FileChooser`; JUCE `AudioProcessorValueTreeState`; JUCE `ValueTree`; shared DSP `A/B compare preset snapshots APVTS state XML`.
- FILES RETRIEVED: `PROMPTS/SLICE_PRESET_AB_HEADER_POLISH.md`, `Source/PluginProcessor.{h,cpp}`, `Source/ui/MainView.{h,cpp}`, `Source/ui/MasterLimiterLookAndFeel.{h,cpp}`, `Source/ui/PresetManager.{h,cpp}`, `docs/SIGNAL_FLOW.md`, `docs/PROGRESS.md`, and `PROMPTS/PLAN.md`.
- SECTIONS CITED: existing plugin state wrapper, full-state user preset load/save helpers, header preset menu layout, `MasterLimiterLookAndFeel` combo/button rendering, JUCE async `FileChooser`, and APVTS `copyState()` / `replaceState()`.
- REUSE CHECK: reused APVTS full-state snapshots, existing user preset APIs, and the existing plugin state wrapper. I checked the local library but found no DSP implementation to reuse because this slice is UI/message-thread state only.

## Verification Gate
- [x] Plugin Release build clean via `cmake --build build`.
- [x] User AU/VST3 clean-reinstalled to `~/Library/Audio/Plug-Ins/...`.
- [x] System VST3 copied to `/Library/Audio/Plug-Ins/VST3/`.
- [ ] System AU copy blocked by root-owned `/Library/Audio/Plug-Ins/Components/MasterLimiter.component`.
- [x] AU validation clean via `auval -v aufx MaLm Melc`; clean user reinstall reports the current 35-parameter build.
- [ ] Audition confirms A/B round-trips distinct full-state voicings, including DEV values.
- [ ] Audition confirms A/B pair survives session reload.
- [ ] Audition confirms Load from file opens the presets folder and loads `.mlpreset`.
- [ ] Audition confirms the preset menu arrow is the stock JUCE arrow.
- [ ] Audition confirms Bypass shows `Bypass` / `Bypassed` clearly.

## Audition Notes
- A/B is intentionally scratch state; named user presets remain long-term storage.
- Switching to the inactive slot captures the live active slot first, then loads the target slot.
- The copy button direction follows the active slot: `A→B` when A is active, `B→A` when B is active.
