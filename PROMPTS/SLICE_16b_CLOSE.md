# Cursor Task — Slice 16b CLOSE (visual restyle: 16b + 16b.1 + 16b.2 + 16b.3)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only close** of the visual-restyle family. avishali approved. No HQ
> change (palette is a product-local override) → no submodule bump. Commit +
> push, FF `main`.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Stage by **explicit path only**. No force-push/amend/skip-hooks. Auth via
  system credential helper. `Co-Authored-By: Claude` trailer.
- No HQ commit / no submodule bump (UI-only, no `Parameters.cpp`/`ParameterIDs.h`
  changes).

────────────────────────────────────────
1. PRE-FLIGHT
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-16b-visual-restyle
cmake --build build-release -j   # clean, no new Source/ warnings
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/SLICE16b_CLOSE_S$SLICE"; done
```
Gate: build clean; Slice 3/4/5 PASS (UI-only → unchanged). STOP on failure.

────────────────────────────────────────
2. COMMITS (explicit paths)
────────────────────────────────────────

**Commit A — visual-restyle UI source:**
```bash
git add Source/ui/MainView.cpp Source/ui/MainView.h \
        Source/ui/MasterLimiterLookAndFeel.cpp Source/ui/MasterLimiterLookAndFeel.h \
        Source/ui/loudness/LoudnessNumericPanel.cpp \
        Source/ui/meters/MeterComponent.cpp \
        Source/ui/meters/MeterGroupComponent.cpp Source/ui/meters/MeterGroupComponent.h
git commit -m "MasterLimiter Slice 16b: visual restyle — refined dark/teal palette, button redesign, power button, horizontal link icons, segmented selectors, LUFS box, knob/fader refinement"
```

**Commit B — docs (write §3 first):**
```bash
git add docs/PROGRESS.md PROMPTS/PLAN.md
git commit -m "docs: Slice 16b close (visual restyle) + PROGRESS/PLAN"
```

**Commit C — archive 16b prompts:**
```bash
git add PROMPTS/SLICE_16b_visual_restyle.md PROMPTS/SLICE_16b_1_controls_and_meters.md \
        PROMPTS/SLICE_16b_2_readouts_and_steps.md PROMPTS/SLICE_16b_3_fader_width_maxhold.md \
        PROMPTS/SLICE_16b_CLOSE.md
git commit -m "docs: archive Slice 16b prompts"
```

────────────────────────────────────────
3. DOCS CONTENT (write before Commit B)
────────────────────────────────────────

### `docs/PROGRESS.md` — new TOP entry
Slice 16b — visual restyle (UI-only; no DSP/param/audio change; Slice 3/4/5
unchanged; matches `design/ui_direction_v1.html`):
- Refined clean/dark/teal palette (product-local override, no HQ Theme.h edit).
- Button redesign: gradient + clear teal on-state; **power button** for Limiter
  On/Off; **horizontal Ozone-style link icons** for Gain⇄Ceiling + I/O links;
  **segmented selectors** for Clipper Hard/Soft, Stereo L/R·M/S, **Character
  (Clean/Tight/Aggressive)**, **Auto-release mode** (all 3-visible).
- Ozone-style type-in: double-click to edit, fits inside the knob, click-away/
  Enter commits, Esc cancels.
- Knob/fader refinement: two-tone teal arc + soft glow; **short rim tick** clear
  of the value text; faders are slim handles that don't obstruct the meter;
  numeric value moved outside the meter box.
- Meters: scale numbers clear of tick lines + aligned ("0" with "+6"); over/clip
  red is a thin cap (never covers the bar); +/- buttons draw real glyphs;
  default range Full; **max-peak readout latches/holds** until meter click-reset.
- LUFS box restyled. Gain/Ceiling/Clipper show one decimal + 0.1 dB drag
  (UI-only; params untouched).

### `PROMPTS/PLAN.md`
- Mark **Slice 16b ✅ Shipped** (visual restyle). Visual look is now in place.
- Active next: **Slice 17** (packaging: param-smoothing + default-state audit,
  factory preset(s), version stamp), then manual, then beta build.
- Backlog additions:
  - **Palette-injection centralization** — colours are currently applied
    product-local across the UI files; centralize via one `PluginEditor.cpp`
    UiContext/theme override to prevent drift (post-beta tidy).
  - **Full bespoke knob/slider re-draw** (post-beta) — 16b was the light
    refinement; deeper custom knob art deferred.

────────────────────────────────────────
4. PUSH
────────────────────────────────────────

```bash
git push origin slice-16b-visual-restyle
git checkout main && git pull --ff-only && git merge --ff-only slice-16b-visual-restyle && git push origin main
```
Record new product `main` SHA; confirm submodule pointer unchanged.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Pre-flight (build + Slice 3/4/5 PASS). 2. Three commit SHAs. 3. Push + FF
main; new main SHA; submodule unchanged. 4. Final `git status --short` (clean).
5. PROGRESS/PLAN excerpts. 6. Open questions.

────────────────────────────────────────
6. AFTER CLOSE
────────────────────────────────────────

Next: **Slice 17** (packaging) off the new `main`. The visual + interaction work
is complete. **Do not start 17 here.**
