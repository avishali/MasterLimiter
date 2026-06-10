# SLICE CLOSE — User presets

**Date:** 2026-06-10
**Status:** implemented locally; build/user install/AU validation clean; system AU copy blocked by stale root-owned bundle; audition pending

## Summary
- Added disk-backed user presets at `~/Library/Audio/Presets/MelechDSP/MasterLimiter/*.mlpreset`.
- User presets save the full APVTS state via XML, so every registered parameter round-trips, including temporary DEV controls.
- Extended the header preset menu with Factory and User sections plus Save current as, Delete active user preset, and Reveal presets folder actions.
- Added a small header Save button wired to the same full-state Save dialog.
- Factory presets remain curated subset presets through `PresetManager::applyPreset()`.

## Retrieval
- TOOLS USED: `user-melech_internal`, `user-juce_docs`, `user-melech_dsp`, local file reads/search.
- QUERIES ISSUED: MasterLimiter `PresetManager`/`MainView`/`PluginProcessor` lookup; JUCE `AudioProcessorValueTreeState`; JUCE `ValueTree`; JUCE `File`; JUCE `AlertWindow`; shared DSP `user presets APVTS XML full state save load DEV parameters`.
- FILES RETRIEVED: `PROMPTS/SLICE_USER_PRESETS.md`, `Source/ui/PresetManager.{h,cpp}`, `Source/ui/MainView.{h,cpp}`, `Source/PluginProcessor.{h,cpp}`, `docs/SIGNAL_FLOW.md`, `docs/PROGRESS.md`, and `PROMPTS/PLAN.md`.
- SECTIONS CITED: existing factory preset subset path, `MainView` header preset combo, APVTS `copyState()` / `replaceState()`, `ValueTree` XML serialization, JUCE `File` directory/list/write/reveal APIs, and async `AlertWindow` text entry.
- REUSE CHECK: reused existing `PresetManager`, `presetMenu_`, and APVTS state machinery. I checked the local library but found no DSP implementation to reuse because this slice is UI/state-manager only.

## Verification Gate
- [x] Plugin Release build clean via `cmake --build build`.
- [x] User AU/VST3 copied to `~/Library/Audio/Plug-Ins/...`.
- [x] System VST3 copied to `/Library/Audio/Plug-Ins/VST3/`.
- [ ] System AU copy blocked by root-owned `/Library/Audio/Plug-Ins/Components/MasterLimiter.component`; non-interactive sudo removal failed because a password is required.
- [x] AU validation clean via `auval -v aufx MaLm Melc`.
- [ ] Audition confirms Save current as writes `.mlpreset` and shows it under User.
- [ ] Audition confirms A/B switching between saved voicings restores DEV values exactly.
- [ ] Audition confirms Delete removes from menu and disk.
- [ ] Audition confirms Reveal opens `~/Library/Audio/Presets/MelechDSP/MasterLimiter`.

## Audition Notes
- Full-state user presets intentionally differ from factory presets: factory presets write selected musical controls only; user presets snapshot every APVTS parameter.
- Presets saved during DEV tuning should keep loading after DEV params are removed for 0.4; unknown ValueTree children are ignored by APVTS.
