# SLICE CLOSE — DEV controls window

**Date:** 2026-06-10
**Status:** implemented locally; build/install/AU validation clean; audition pending

## Summary
- Moved all temporary DEV tuning controls out of the main view and into a separate header **DEV** window.
- Added `DevControlsComponent`, grouped by operating section: Attack, Lookahead, Release Engine, Lookahead engine, Adaptive engine, Band scaling, and Manual.
- Reused the existing History Graph `DocumentWindow` ownership model, including deferred close via `MessageManager::callAsync` + `SafePointer`.
- Kept this UI-only: no DSP, SDK, parameter ID/range, state, or latency changes.

## Retrieval
- TOOLS USED: `user-melech_internal`, `user-juce_docs`, `user-melech_dsp`, local file reads/search.
- QUERIES ISSUED: MasterLimiter `PluginEditor`/`MainView`/History Graph path lookup; JUCE `DocumentWindow`; JUCE `AudioProcessorValueTreeState::SliderAttachment`; shared DSP `DEV controls UI APVTS slider attachment window`.
- FILES RETRIEVED: `PROMPTS/SLICE_DEV_WINDOW.md`, `Source/PluginEditor.{h,cpp}`, `Source/ui/MainView.{h,cpp}`, `Source/ui/HistoryGraphComponent.{h,cpp}`, `CMakeLists.txt`, `docs/SIGNAL_FLOW.md`, `docs/PROGRESS.md`, and `PROMPTS/PLAN.md`.
- SECTIONS CITED: History Graph editor-owned window and deferred close path, old `MainView` DEV strip wiring/layout, and `SIGNAL_FLOW.md` §6 DEV controls.
- REUSE CHECK: reused the existing History Graph window pattern and existing APVTS parameters/attachments. I checked the local library but found no DSP implementation to reuse because this slice is UI-only.

## Verification Gate
- [x] Plugin Release build clean via `cmake --build build`.
- [x] AU/VST3 copied to user plug-in folders, including system VST3.
- [x] AU validation clean via `auval -v aufx MaLm Melc`.
- [ ] Audition confirms main view no longer shows the DEV strip.
- [ ] Audition confirms the header DEV button opens a separate always-on-top, resizable window.
- [ ] Audition confirms every DEV control still drives and persists its APVTS value.
- [ ] Audition confirms DEV and Graph windows can coexist and close repeatedly without crash.

## Audition Notes
- The manual Sustain Ratio control is shown in the DEV window and is disabled when Release Auto is on.
- Character remains greyed out in the main view while `dev_attack_ms` owns attack timing.
- This component should be deleted with the temporary DEV params when the 0.4 voicing is baked.
