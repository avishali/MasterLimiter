# SLICE — Preset A/B compare + Load-from-file + header polish

**Status:** ready for Cursor · **Architect:** Claude · **Audition/decide:** avishali
**Repo:** plugin `MasterLimiter` (UI + L&F; presets already exist). Commit + push.
**Builds on:** `e91c82e` (user presets), `cc60b81` (DEV window).

---

## Goals
1. **A/B compare buttons** — two slots to flip between two full-state voicings instantly.
2. **Load from file…** — a file chooser that opens the presets folder to load a `.mlpreset`.
3. **Default JUCE combo arrow** — the current custom preset-menu arrow looks bad; use JUCE's stock arrow.
4. **Fix the Bypass button text** — it renders as an empty button (confusing); it must show **"Bypass" / "Bypassed"**.

---

## Allowed files to touch
```
Source/ui/MainView.h
Source/ui/MainView.cpp
Source/ui/MasterLimiterLookAndFeel.cpp   (combo arrow + bypass text rendering)
Source/ui/PresetManager.h/.cpp           (reuse load/list; add a load-by-file if not present)
Source/PluginProcessor.h/.cpp            (persist A/B slots in plugin state — see §1)
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_PRESET_AB_HEADER_POLISH_CLOSE.md   (new, at close)
```

---

## 1. A/B compare (the main feature)
Standard mastering-style A/B comparator on **full APVTS state** (so it captures everything incl. DEV).

UI: two small toggle buttons **A** and **B** in the header next to the preset menu, plus a small **"A→B"** copy button.

State model (in `MainView`, backed by the processor for persistence):
- Two snapshots: `slotA`, `slotB` (each a `juce::ValueTree` / XML string of `apvts.copyState()`); an `activeSlot` flag (default A).
- **On startup:** initialize both slots from the current state.
- **Click the *inactive* slot button** (the comparator swap):
  1. Save the current live state into the **currently-active** slot.
  2. Load the **target** slot's state via `apvts.replaceState(...)`.
  3. Set active = target; highlight the active button.
  → So each slot remembers its own voicing, live edits accrue to the active slot, and flipping A↔B instantly compares the two complete settings.
- **"A→B" copy:** copy the active slot's state into the other slot (seed B from A to start tweaking a variant).
- Clicking the **already-active** slot does nothing (or re-asserts it).

Persistence: store `slotA`/`slotB`/`activeSlot` inside `getStateInformation` / restore in `setStateInformation` (alongside the APVTS state, e.g. as child nodes of the saved tree) so the A/B pair survives session save/reload. Keep it robust: if absent on load, re-init from current state.

> RT note: none — this is all message-thread (param changes go through APVTS as usual). `replaceState` on the message thread is how presets already load.

## 2. Load from file…
Add a **"Load from file…"** item to the preset menu (and/or a small button). On select:
- `juce::FileChooser ("Load preset", PresetManager::getUserPresetsDir(), "*.mlpreset")`, launched async (`launchAsync` with `openMode | canSelectFiles`).
- On a chosen file → `PresetManager::loadUserPreset(apvts, file)`; refresh the menu / active-preset label.
- Starting directory = the presets folder (`getUserPresetsDir()`), so it "opens the presets folder" as asked. (Also allow navigating elsewhere.)

## 3. Default JUCE combo arrow
The preset `ComboBox` currently shows a custom/ugly arrow (custom `arrowColourId` + the L&F's `drawComboBox`). Make `presetMenu_` use **JUCE's default arrow**:
- Easiest: in `MasterLimiterLookAndFeel`, either don't override `drawComboBox` for this box, or have the override fall back to `juce::LookAndFeel_V4::drawComboBox` (stock triangle). If the custom arrow is drawn in the L&F, remove that custom-arrow code path for the preset menu (or globally if it's only used here).
- Remove the `presetMenu_.setColour (juce::ComboBox::arrowColourId, palette::accentBright)` override if it's fighting the default look; let the stock arrow render.
- Keep the box background/outline colors as-is; only the **arrow** should become the default JUCE one. Verify it looks clean (a simple down-triangle).

## 4. Bypass button text fix
**Symptom:** the Bypass button is blank. The text *is* set (`updateBypassButtonState()` → `setButtonText("Bypass"/"Bypassed")`), so this is a **L&F rendering** bug. In `MasterLimiterLookAndFeel::drawButtonText`, the Bypass button falls into the custom path:
```cpp
if (isTextButtonActive (button) && ! button.getToggleState())
    drawButtonLabel (g, ui_, button, ..., true);   // <-- likely the culprit: not drawing the label text
```
**Fix:** ensure the Bypass button draws its `getButtonText()` in **both** toggle states. Inspect `isTextButtonActive` / `drawButtonLabel` — either make `drawButtonLabel` actually render the button's text, or exclude the Bypass button from that special path so it hits the base `mdsp_ui::LookAndFeel::drawButtonText` (which draws the label). Confirm the label is visible and legible against both the normal and the `warning`-tinted bypassed background. Also confirm the 60 px width (`setBounds (1030,12,60,28)`) fits "Bypassed" — widen slightly if it clips.

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
2. **A/B:** set a voicing → A→B copy → tweak DEV controls → flip A/B and the two full voicings swap instantly (verify a DEV value differs between slots and round-trips). A/B pair survives session reload.
3. **Load from file…** opens a chooser in the presets folder and loads a `.mlpreset`.
4. Preset menu shows the **default JUCE arrow** (clean triangle), not the custom one.
5. **Bypass button shows "Bypass" / "Bypassed"** clearly in both states.

Close gate: update `docs/SIGNAL_FLOW.md` §5 (A/B + load-from-file), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit + push; archive the CLOSE prompt.

---

## Note
- A/B and user presets are complementary: presets = named long-term storage; A/B = fast scratch comparison of two live voicings. Both use full-state, so they interoperate (load a preset into A, another into B, compare).
