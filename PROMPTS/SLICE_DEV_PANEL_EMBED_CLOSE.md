# SLICE CLOSE — DEV controls embedded panel (AAX fix)

**Date:** 2026-06-22
**Status:** implemented locally; build/install/AU validation pending; AAX audition pending

## Summary
- Fixed AAX/Pro Tools DEV controls dead/snap-back by embedding `DevControlsComponent` in the editor hierarchy instead of a detached `DocumentWindow`.
- Added editor-owned `DevPanel` (title bar + close ✕ + existing `DevControlsComponent`) and a translucent scrim overlay; toggled by the header **DEV** button.
- Removed `DevWindow` / `devWindow_` plumbing. History Graph separate window unchanged (read-only).

## Retrieval
- TOOLS USED: `user-melech_internal`, `user-juce_docs`, local file reads.
- QUERIES ISSUED: MasterLimiter `PluginEditor` path lookup; JUCE `Component` child hierarchy.
- FILES RETRIEVED: `PROMPTS/SLICE_DEV_PANEL_EMBED.md`, `Source/PluginEditor.{h,cpp}`, `Source/ui/DevControlsComponent.{h,cpp}`, `Source/ui/MainView.cpp`, `docs/SIGNAL_FLOW.md`, `docs/PROGRESS.md`, `PROMPTS/PLAN.md`.
- SECTIONS CITED: removed DEV `DocumentWindow` path, unchanged `DevControlsComponent` APVTS attachments, `SIGNAL_FLOW.md` §6.
- REUSE CHECK: `DevControlsComponent` reused unchanged; only parenting changed. No DSP reuse needed (UI-only).

## Verification Gate
- [ ] Plugin Release build clean via `cmake --build build`.
- [ ] AU/VST3 copied to system plug-in folders.
- [ ] AU validation clean via `auval -v aufx MaLm Melc`.
- [ ] Audition: header **DEV** opens in-window panel (no separate OS window); close ✕ hides it.
- [ ] Audition: VST3/AU regression — every DEV control drives/persists its APVTS value.
- [ ] Audition: **AAX/Pro Tools** — DEV slider (e.g. LA Release or Attack) audibly changes sound and holds value.
- [ ] Audition: editor resize keeps panel centered/clamped correctly.

## Audition Notes
- Root cause: Pro Tools/AAX does not commit parameter gestures from OS windows outside the hosted editor hierarchy.
- After this lands: rebuild + sign + notarize AAX beta and bump build number before re-uploading to testers.
