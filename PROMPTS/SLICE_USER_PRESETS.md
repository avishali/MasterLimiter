# SLICE — User presets (save / load / compare voicings, full-state incl. DEV)

**Status:** ready for Cursor · **Architect:** Claude · **Audition/decide:** avishali · **PRIORITY**
**Repo:** plugin `MasterLimiter` (UI + a manager; no DSP, no SDK). Commit + push.

---

## Goal
Let the user **save the current state as a named preset**, **load** it back, and **compare** between saved presets — so voicing experiments can be stored and a good voicing is never lost. **Critically: a user preset captures the FULL APVTS state, including every DEV param** (attack, lookahead band/wide, LA release ms/poles, sigma, band scales, engine, sustain ratio). Factory presets stay as-is (curated subsets); user presets are full snapshots.

MVP flow: **Save As… → name it → it appears in the menu → select to load.** Plus Delete and Reveal-in-Finder.

---

## Why full-state (not the factory struct)
`PresetManager::applyPreset` sets a hand-picked subset and **omits DEV params** — useless for storing voicing. User presets must use `apvts.copyState()` → XML (captures *all* registered params) and `apvts.replaceState(...)` to load. That's the whole point: the DEV voicing round-trips.

---

## Allowed files to touch
```
Source/ui/PresetManager.h           (add user-preset API)
Source/ui/PresetManager.cpp         (implement save/load/list/delete to disk)
Source/ui/MainView.h                 (preset menu + Save/Delete UI)
Source/ui/MainView.cpp               (menu population, save dialog, load/delete wiring)
Source/PluginProcessor.h/.cpp        (only if a thin accessor is needed; UI has apvts via MainView)
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_USER_PRESETS_CLOSE.md  (new, at close)
```

---

## 1. `PresetManager` — user-preset API (`PresetManager.h/.cpp`)
Add static functions (keep the existing factory ones):
```cpp
// Directory: ~/Library/Audio/Presets/MelechDSP/MasterLimiter/  (created if missing)
static juce::File getUserPresetsDir();
static juce::Array<juce::File> listUserPresets();              // *.mlpreset, sorted by name
static bool saveUserPreset (const juce::AudioProcessorValueTreeState& apvts, const juce::String& name);
static bool loadUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::File& file);
static bool deleteUserPreset (const juce::File& file);
```
Implementation:
- **Dir:** `File::getSpecialLocation(userApplicationDataDirectory).getChildFile("Audio/Presets/MelechDSP/MasterLimiter")`; `createDirectory()` if absent.
- **Save:** `auto state = apvts.copyState(); auto xml = state.createXml();` → write to `dir/<sanitized name>.mlpreset`. Sanitize the name (strip path-illegal chars). Overwrite if same name (that's the natural "update"). Returns false on write failure.
- **Load:** parse XML → `ValueTree::fromXml`; if valid and type matches the APVTS state type, `apvts.replaceState(tree)`. **Robust to missing/extra params** — `replaceState` ignores unknown nodes, so presets saved now still load after DEV params are removed for 0.4 (and vice-versa). Returns false on parse failure.
- **List:** glob `*.mlpreset` in the dir, sorted.
- Use `.mlpreset` extension to namespace ours.

> Full state via `copyState()` includes DEV params automatically — no per-param enumeration needed.

## 2. UI — preset menu + Save/Delete (`MainView.h/.cpp`)
Extend the existing header `presetMenu_` (`juce::ComboBox`) and add a small **Save** affordance:
- **Menu population** (rebuild on open and after save/delete):
  - Section 1 — **Factory**: the existing `PresetManager::getPresetName(i)` entries (ids 1..N).
  - Separator.
  - Section 2 — **User**: `listUserPresets()` names (ids offset, e.g. 1000+).
  - Separator.
  - **"Save current as…"** item, and (when a user preset is active) **"Delete <name>"**, **"Reveal presets folder"**.
- **Selection handler:**
  - Factory id → `processor.applyPreset(index)` (existing).
  - User id → `PresetManager::loadUserPreset(apvts, file)`.
  - "Save current as…" → open a `juce::AlertWindow` with a text editor (default name like "Voicing 1"); on OK → `saveUserPreset(apvts, name)`, then refresh the menu and select the new preset.
  - "Delete" → confirm (`AlertWindow`), `deleteUserPreset(file)`, refresh.
  - "Reveal" → `getUserPresetsDir().revealToUser()`.
- A dedicated small **Save** button next to the menu is fine too (whichever fits the header cleanly) — wire it to the same "Save current as…" flow.
- Match existing header/L&F styling; keep the header layout/resize stable.

## 3. Comparison ergonomics (the point)
Because each user preset is a full snapshot, switching between two saved presets in the menu = instant A/B of two complete voicings (incl. DEV). No extra feature needed — just make sure selecting a user preset fully loads it (replaceState updates every control + the DSP via the param listeners).

---

## 4. Build, verify, install (SYSTEM folders), close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
# install to SYSTEM folders (primary; world-writable, no sudo once the stale root-owned AU is removed):
A=build/MasterLimiter_artefacts/Release
cp -R "$A/VST3/MasterLimiter.vst3"       /Library/Audio/Plug-Ins/VST3/
cp -R "$A/AU/MasterLimiter.component"    /Library/Audio/Plug-Ins/Components/
# (also user folders if you keep them in sync)
auval -v aufx MaLm Melc 2>&1 | tail -5
```
Acceptance:
1. Builds clean; AU validates.
2. **Save current as…** writes a `.mlpreset` and it appears under **User** in the menu.
3. Tweak DEV params → save "A"; tweak again → save "B"; switching A↔B in the menu **fully restores each voicing incl. all DEV controls** (verify a DEV value round-trips exactly).
4. Delete removes it from menu + disk; Reveal opens the folder.
5. Presets survive a plugin reload / new instance (they're on disk).
6. Loading a preset updates both the UI controls and the audio (param listeners fire).

Close gate: update `docs/SIGNAL_FLOW.md` §5/§6 (user presets save full state incl. DEV; location `~/Library/Audio/Presets/MelechDSP/MasterLimiter`), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit + push; archive `PROMPTS/SLICE_USER_PRESETS_CLOSE.md`.

---

## Notes
- **Install convention going forward:** primary install path is the **system** folder `/Library/Audio/Plug-Ins/{VST3,Components}` (the main plugins folder). It's world-writable → no sudo, *after* the one-time `sudo rm -rf /Library/Audio/Plug-Ins/Components/MasterLimiter.component` to clear the stale root-owned copy.
- After we **bake** the voicing for 0.4 and remove DEV params, old user presets still load (unknown nodes ignored); the baked voicing then lives in code + the user-facing params persist in presets. So saving DEV-laden presets now is safe.
- Pairs with the still-pending `SLICE_DEV_WINDOW.md` (DEV controls window) — independent; either order.
