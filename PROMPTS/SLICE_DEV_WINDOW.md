# SLICE — Move DEV controls to a separate window (organized by section)

**Status:** ready for Cursor · **Architect:** Claude · **Audition/decide:** avishali
**Repo:** plugin `MasterLimiter` (UI only — no DSP, no SDK). Commit + push.
**Pattern to copy:** the existing **History Graph window** (editor-owned `DocumentWindow`, header button, always-on-top, non-modal).

---

## Goal
Remove the cluttered inline **DEV RELEASE** strip from the main view and put **all DEV controls in a separate window**, opened from a header **DEV** button — grouped and labeled **by operating section** so it's clear what each control affects. This declutters the main UI and makes the tuning rig readable.

No parameter changes, no DSP changes — pure UI relocation + organization. All controls keep their existing APVTS attachments.

---

## Allowed files to touch
```
Source/ui/DevControlsComponent.h     (new)
Source/ui/DevControlsComponent.cpp   (new)
Source/ui/MainView.h                 (remove DEV strip members + add DEV button)
Source/ui/MainView.cpp               (remove DEV strip layout/wiring + DEV button)
Source/PluginEditor.h                (own the DEV window)
Source/PluginEditor.cpp              (toggle/own DEV window, like history window)
CMakeLists.txt                       (register DevControlsComponent.cpp)
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_DEV_WINDOW_CLOSE.md     (new, at close)
```

---

## 1. New `DevControlsComponent` (`Source/ui/DevControlsComponent.{h,cpp}`)
`class DevControlsComponent : public juce::Component` — ctor takes `MasterLimiterAudioProcessor&` + `mdsp_ui::UiContext&` (match HistoryGraphComponent).

Holds **all** DEV controls (sliders/combos) with their `APVTSAttachment`s, laid out in **labeled section groups**. Use the existing dev-strip styling/L&F. Each group = a titled sub-panel (header label + a row/column of controls). Suggested layout — **organized by operating section**:

| Section header | Controls (param IDs) |
|---|---|
| **ATTACK** | `dev_attack_ms` |
| **LOOKAHEAD** | `dev_lookahead_band_ms` (Band), `dev_lookahead_wide_ms` (Wide) |
| **RELEASE · Engine** | `dev_release_engine` (Adaptive / Lookahead select) |
| **RELEASE · Lookahead engine** | `dev_la_release_ms` (Time), `dev_la_release_poles` (Poles) |
| **RELEASE · Adaptive engine** | `dev_sigma_attack_ms` (Sigma Atk), `dev_sigma_decay_scale` (Sigma Decay) |
| **RELEASE · Band scaling** | `dev_low_band_release_scale` (Low ×), `dev_high_band_release_scale` (High/Wide ×) |
| **RELEASE · Manual** | `release_sustain_ratio` (Sustain Ratio — *note: active only when Release Auto = Off*) |

- Add a short caption per control where helpful (e.g. tooltip: "Overrides Character; capped by lookahead" on Attack; "active only when Release Auto = Off" on Sustain Ratio).
- A small header line in the window: *"DEV — tuning controls (temporary; baked & removed for 0.4)"*.
- Self-contained component (so it can later be re-embedded if wanted), reasonable default size (~520×560), scroll/viewport if it doesn't fit.

## 2. Remove the inline DEV strip from MainView
- Delete the orange **DEV RELEASE** strip layout, its slider/combo members, and their attachments from `MainView.{h,cpp}` (they move into `DevControlsComponent`).
- Reclaim that space in the main layout (or just leave it cleaner — your call on reflow; keep the resize stable).

## 3. Header **DEV** button + window (mirror the History Graph plumbing)
- In `MainView`, add `juce::TextButton btnDev_ { "DEV" };` in the header next to the **Graph** button; expose `std::function<void()> onToggleDevControls;` and wire `btnDev_.onClick`.
- In `PluginEditor`:
  - Add `std::unique_ptr<juce::DocumentWindow> devWindow_;` (reuse the same `HistoryWindow`-style `DocumentWindow` subclass or a second small subclass with the deferred-close pattern).
  - `toggleDevControls()` — create/destroy the window; `setContentOwned(new DevControlsComponent(processor_, ui_), true)`; `setResizable(true,true)`; `setAlwaysOnTop(true)`; `centreWithSize(520, 560)`; non-modal.
  - **Reuse the deferred-close fix** (`MessageManager::callAsync` + `SafePointer`) — same as the history window, to avoid the close-button use-after-free.
  - Wire `mainView.onToggleDevControls = [this]{ toggleDevControls(); };`
  - Editor destructor resets `devWindow_`.

## 4. CMake
Add `Source/ui/DevControlsComponent.cpp` to the plugin target sources.

---

## 5. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
cp -R build/MasterLimiter_artefacts/Release/AU/MasterLimiter.component  ~/Library/Audio/Plug-Ins/Components/
cp -R build/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3     ~/Library/Audio/Plug-Ins/VST3/
cp -R build/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3     /Library/Audio/Plug-Ins/VST3/
auval -v aufx MaLm Melc 2>&1 | tail -5
```
Acceptance:
1. Builds clean; AU validates.
2. Main view no longer shows the DEV strip — it's clean.
3. Header **DEV** button opens a separate, always-on-top, resizable window with all DEV controls **grouped by section** and clearly labeled.
4. Every DEV control still drives its parameter (the attachments work from the new window); values persist with state.
5. Open/close (button + window close box) repeatedly — no crash (deferred-close pattern), no leak.
6. Graph window and DEV window coexist independently.

Close gate: update `docs/SIGNAL_FLOW.md` §6 (DEV controls now live in a separate DEV window, grouped by section), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit + push; archive `PROMPTS/SLICE_DEV_WINDOW_CLOSE.md`.

---

## Notes
- When we bake the final voicing for 0.4, this whole window + `DevControlsComponent` gets deleted along with the DEV params — keeping it isolated in one component makes that a clean removal.
- Order this **after** the lookahead-trim slice (`SLICE_LOOKAHEAD_TRIM_FINE.md`) if both are pending, so the trimmed lookahead range/labels land in the new window. (Either order works; just rebuild.)
