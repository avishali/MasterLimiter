# Cursor Task — Slice 16 CLOSE (UI interaction: 16 + 16.1 + 16.2)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only close** of the Slice 16 family (interaction/layout/type-in,
> hide dead controls, clip-ballistics file split). avishali approved. No HQ
> change → no submodule bump. Commit + push, FF `main`. The visual restyle is a
> separate slice (16b) on top.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Stage by **explicit path only**. No force-push/amend/skip-hooks. Auth via
  system credential helper. `Co-Authored-By: Claude` trailer.
- No HQ commit / no submodule bump this slice (UI-only).

────────────────────────────────────────
1. PRE-FLIGHT
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-16-ui-interaction
cmake --build build-release -j   # clean, no new Source/ warnings
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/SLICE16_CLOSE_S$SLICE"; done
```
Gate: build clean; Slice 3/4/5 PASS (UI-only → unchanged). STOP on failure.

────────────────────────────────────────
2. COMMITS (explicit paths)
────────────────────────────────────────

**Commit A — UI source (incl. the new ClipBallistics files):**
```bash
git add Source/ui/MainView.cpp Source/ui/MainView.h \
        Source/ui/meters/GainReductionMeter.cpp \
        Source/ui/meters/ClipBallistics.cpp Source/ui/meters/ClipBallistics.h \
        CMakeLists.txt
git commit -m "MasterLimiter Slice 16: UI interaction — Ozone double-click type-in, Clipper Hard/Soft toggle, hide Lookahead/T-S, tooltips, layout, clip-ballistics file split"
```

**Commit B — docs + 16b UI-direction reference (write §3 first):**
```bash
git add docs/PROGRESS.md PROMPTS/PLAN.md design/ui_direction_v1.html
git commit -m "docs: Slice 16 close (PROGRESS/PLAN) + 16b UI-direction mockup reference"
```

**Commit C — archive Slice 16 prompts:**
```bash
git add PROMPTS/SLICE_16_ui_interaction.md PROMPTS/SLICE_16_1_layout_and_typein.md \
        PROMPTS/SLICE_16_2_typein_sizing_layout.md PROMPTS/SLICE_16_CLOSE.md
git commit -m "docs: archive Slice 16 prompts"
```

────────────────────────────────────────
3. DOCS CONTENT (write before Commit B)
────────────────────────────────────────

### `docs/PROGRESS.md` — new TOP entry
Slice 16 — UI/UX interaction (UI-only, no DSP/param/audio change; Slice 3/4/5
unchanged):
- Hid the Lookahead and T/S Sustain controls (their frozen params remain in the
  APVTS at default — lookahead fixed 5 ms, sustain ratio 4.0).
- Ozone-style type-in: clean value labels, **double-click** to edit inline,
  Enter / click-away commits, Esc cancels, unit-aware + clamped.
- Clipper Hard/Soft is now a 2-state toggle (was a dropdown).
- Tooltips on all controls + label-clarity pass.
- Layout: Color below Clipper; Imaging (Stereo-Mode) compact + moved left clear
  of Color; Gain-Match centered footer; SP/TP + Gain⇄Ceiling Link + Limiter On
  fitted between the Gain and Ceiling knobs; `100%%`→`100%` fix.
- Clip-ballistics free functions moved out of `GainReductionMeter.cpp` into
  `Source/ui/meters/ClipBallistics.{h,cpp}` (firewall preserved).
- `design/ui_direction_v1.html` added as the **Slice 16b visual target**.

### `PROMPTS/PLAN.md`
- Mark **Slice 16 ✅ Shipped** (UI interaction).
- Active next: **Slice 16b** — visual restyle per `design/ui_direction_v1.html`:
  refined clean/dark/teal palette, button redesign (clear on-state), **power
  button** for Limiter On/Off, **horizontal Ozone-style link icon** for the
  Link toggles, segmented toggles, LUFS box restyle, knob/fader refinement.
  Then Slice 17 (packaging), manual, beta.
- Backlog: full knob/slider re-draw (post-beta); auto-release mode fine-tuning
  (post-beta); final transparent ceiling stage; HQ dual-meter-system
  consolidation.

────────────────────────────────────────
4. PUSH
────────────────────────────────────────

```bash
git push origin slice-16-ui-interaction
git checkout main && git pull --ff-only && git merge --ff-only slice-16-ui-interaction && git push origin main
```
Record new product `main` SHA; confirm submodule pointer unchanged (no HQ bump).

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Pre-flight (build + Slice 3/4/5 PASS). 2. Three commit SHAs. 3. Push + FF
main; new main SHA; submodule unchanged. 4. Final `git status --short` (clean).
5. PROGRESS/PLAN excerpts. 6. Open questions.

────────────────────────────────────────
6. AFTER CLOSE
────────────────────────────────────────

Next: **Slice 16b** (visual restyle) off the new `main`, architect-specced from
`design/ui_direction_v1.html`. **Do not start it here.**
