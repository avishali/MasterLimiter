# SLICE — Embed DEV controls in the editor (fix AAX: DEV controls dead in Pro Tools)

**Status:** ready for Cursor · **Architect:** Claude · **Audition/decide:** avishali · **BUG FIX (AAX blocker)**
**Repo:** plugin `MasterLimiter` (UI only — no DSP, no SDK). Commit + push.

---

## The bug (confirmed)
In **AAX / Pro Tools, none of the DEV controls do anything** (sliders snap back / no audio change), while the **main controls work** and **VST3/AU work fully**. Root cause: the DEV controls live in a **separate top-level `DocumentWindow`** the plugin spawns (`PluginEditor` `toggleDevControls()` → `devWindow_` → `DevControlsComponent`). Pro Tools/AAX does not commit parameter gestures coming from a detached window the host doesn't manage. The code itself is correct (standard `SliderAttachment`/`ComboBoxAttachment` to `processor.getAPVTS()`, identical to the main view) — it's purely that the controls are outside the AAX-hosted editor hierarchy.

## The fix
**Stop using a separate window for the DEV controls. Embed `DevControlsComponent` as a panel/overlay inside the editor** (a child of the editor that Pro Tools hosts), toggled by the existing header **DEV** button. Single component hierarchy → parameter changes commit in every format including AAX. **Reuse `DevControlsComponent` as-is** — only its *parenting* changes (window → editor child).

The **History Graph stays a separate window** (it's read-only — never writes params, so AAX is unaffected). Only the param-writing DEV controls must move.

---

## Allowed files to touch
```
Source/PluginEditor.h
Source/PluginEditor.cpp
Source/ui/DevControlsComponent.h      (only if a header bar / close button needs adding)
Source/ui/DevControlsComponent.cpp
Source/ui/MainView.h / MainView.cpp   (only if the DEV button toggle wiring needs a tweak)
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_DEV_PANEL_EMBED_CLOSE.md  (new, at close)
```

---

## 1. Remove the DEV DocumentWindow
In `PluginEditor.{h,cpp}`:
- Delete the DEV `DocumentWindow` member (`devWindow_`) and its `HistoryWindow`-style subclass usage for DEV.
- `toggleDevControls()` no longer creates/destroys a window.
- Editor destructor no longer resets `devWindow_`.
(Leave the **History Graph** window plumbing exactly as-is.)

## 2. Embed `DevControlsComponent` as an in-editor panel
- Add an editor-owned **container** that holds `DevControlsComponent` (reuse it unchanged — it already takes `processor` + `ui_` and builds the APVTS attachments). Suggested: a small `Component` subclass `DevPanel` (or just a styled container) holding:
  - a **header bar**: title "DEV CONTROLS — tuning (temporary)" + a **close ✕ button**,
  - the `DevControlsComponent` below it, inside a `juce::Viewport` if it doesn't fit (so all grouped sections are reachable).
- **Add it as a child of the editor** (`addChildComponent(devPanel_)`, initially invisible) — NOT a separate window. This is the whole point: it lives in the AAX-hosted hierarchy.
- Give it an **opaque background** (it overlays the main view) and a sensible size (e.g. centered, ~700×560, clamped to the editor bounds). Optionally dim the main view behind it with a translucent scrim child.

## 3. Toggle from the header DEV button
- `mainView.onToggleDevControls` (already wired) → `toggleDevControls()` now just flips the panel's visibility:
  ```cpp
  void toggleDevControls() {
      const bool show = ! devPanel_->isVisible();
      devPanel_->setVisible(show);
      if (show) { layoutDevPanel(); devPanel_->toFront(true); }
  }
  ```
- The close ✕ in the panel header also hides it.
- In `resized()`, reposition the panel (center / clamp to editor bounds) so it stays correct on editor resize.

## 4. Keep DevControlsComponent's contents identical
Do **not** change the controls, sections, params, or attachments — only reparent. The grouped layout (ATTACK / LOOKAHEAD / RELEASE·Engine / ·Lookahead / ·Adaptive / ·Band scaling / ·Manual) stays. If the panel is shorter than the content, wrap it in a `Viewport` with vertical scroll.

---

## 5. Build, verify (SYSTEM install), close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
A=build/MasterLimiter_artefacts/Release
cp -R "$A/VST3/MasterLimiter.vst3"    /Library/Audio/Plug-Ins/VST3/
cp -R "$A/AU/MasterLimiter.component" /Library/Audio/Plug-Ins/Components/
auval -v aufx MaLm Melc 2>&1 | tail -5
```
Acceptance:
1. Builds clean; AU validates.
2. Header **DEV** button opens the DEV controls **as an in-window panel** (no separate OS window); close ✕ hides it.
3. **VST3/AU still work** (regression check): every DEV control still drives its param.
4. **AAX (Pro Tools): the DEV controls now work** — moving a DEV slider changes the sound and the value sticks (no snap-back). ← the fix.
5. Resizing the plugin window keeps the DEV panel positioned correctly.

Close gate: update `docs/SIGNAL_FLOW.md` §6 (DEV controls are now an embedded editor panel, not a separate window — fixes AAX param commit), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit + push; archive the CLOSE prompt.

---

## Notes
- This is the JUCE-recommended pattern: a plugin's interactive UI should be one component hierarchy rooted in the editor. Separate `DocumentWindow`s are fine only for **read-only** displays (the History Graph), which don't write params.
- After this lands, **re-release the beta** (the testers' AAX needs this fix) — rebuild + sign + notarize + re-upload, and **bump the build number** so the marker/version is fresh. Use the clean-rebuild + marker-verify steps (a stale-marker bit us once).
- avishali tests by loading the new AAX in Pro Tools and confirming a DEV slider (e.g. LA Release or Attack) audibly changes the sound and holds its value.
