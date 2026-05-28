# Cursor Task — Slice 10: Maximizer UI shell (Ozone-inspired two-panel layout)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. UI/editor-thread only — NO DSP, NO new APVTS params,
> NO range changes to existing params.** This is a *visual shell* to lock the
> layout. Existing controls are re-laid-out and stay wired. New controls
> (I/O faders, Gain-Match toggles, Learn Input Gain, the re-ranged Gain
> drive) are **visual placeholders** — styled to match, NOT attached to any
> parameter. Real params + wiring + the ADR come in the next slice.
> Create a branch, build, **do NOT commit** — architect auditions the look.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- ONE focused diff, files in §4 only. No HQ edits, no AnalyzerPro edits.
- No DSP changes. No new APVTS parameters. Do NOT change the range,
  default, or ID of any existing parameter (esp. `input_gain_db`).
- RT-safety: audio thread untouched.
- **Create a branch `slice-10-maximizer-ui-shell` off `main`. Do NOT
  commit, do NOT push.** Leave the tree dirty for architect audition.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

We are moving MasterLimiter toward an Ozone-Maximizer-style control model:
a global **I/O** panel (independent Input/Output trims + Gain Match) and a
**Maximizer** panel (the limiter: Gain drive, Output Level/Ceiling, etc.).
Before introducing new parameters and DSP, we want to **lock the visual
layout** in the real plugin. This slice builds that layout as a shell.

The approved layout reference is committed in the repo:
**`docs/mockups/SLICE10_ui_layout.svg`** — open it. It is schematic
(placement + grouping); final knob/fader styling should use the existing
`mdsp_ui` LookAndFeel / theme, not the flat shapes in the SVG.

────────────────────────────────────────
2. TRINITY (lightweight)
────────────────────────────────────────

Read before editing:
- `docs/mockups/SLICE10_ui_layout.svg` (the target layout).
- `Source/ui/MainView.{h,cpp}` (current single-view layout — you will
  reorganize this).
- `Source/ui/meters/MeterGroupComponent.h`, `Source/ui/loudness/LoudnessNumericPanel.h`
  (existing meter + LUFS components to reuse, repositioned).
- `Source/PluginEditor.{h,cpp}` (size/timer — unchanged here).
- `Source/parameters/ParameterIDs.h` (existing param IDs — do not add).

Output the retrieval log.

────────────────────────────────────────
3. LAYOUT (per docs/mockups/SLICE10_ui_layout.svg)
────────────────────────────────────────

Keep the editor size as-is (default 1100×620, min 960×540). Two panels
side by side under the header, with a footer:

### Header (top, ~52 px)
- "MasterLimiter" title (existing styling).

### MAXIMIZER panel (left, wide — ~68% width)
Section label "MAXIMIZER".
- **Gain** — big rotary, the *drive*. **PLACEHOLDER** (see §5): styled
  positive-only, value reads "0.0 dB", pointer parked at the **hard-left
  minimum**, range shown 0 → +24, "push up" feel. NOT attached to
  `input_gain_db` (its range is changing next slice).
- **Output Level** + **True Peak** — WIRED to existing `ceiling_db`
  (value box / rotary) and `ceiling_mode` (the True-Peak toggle/choice).
- **Character** — WIRED to existing `character`.
- Small-knob row — WIRED to existing params: **Release** (`release_ms`),
  **Sustain** (`release_sustain_ratio`), **Lookahead** (`lookahead_ms`),
  **Auto Rel** (`release_auto`), **Stereo Lk** (`stereo_link_pct`),
  **M/S Lk** (`ms_link_pct`).
- **GR meter** — reuse the existing GR `MeterGroupComponent`, placed at the
  panel's right edge (vertical, top-down).
- **Gain Match** group:
  - **Auto / Track** toggle — PLACEHOLDER.
  - **Gain ⇄ Ceiling Link** toggle — PLACEHOLDER (may sit near the Gain
    knob as in the SVG).

### I/O panel (right, narrow — ~32% width)
Section label "I/O".
- **Learn Input Gain** + LUFS readout (e.g. "−11.0 LUFS") — PLACEHOLDER.
- **Input** and **Output** meter bars — reuse existing input/output
  `MeterGroupComponent`s (L/R), placed vertically.
- **Input** gain fader + readout — PLACEHOLDER (independent trim).
- **Output** gain fader + readout — PLACEHOLDER (independent trim).
- LUFS numeric (M/S/I) — reuse existing `LoudnessNumericPanel`, placed in
  this panel (your call where it reads best — under the meters is fine).

### Footer (bottom)
- **Bypass** button — PLACEHOLDER (no host-bypass wiring this slice).
- Keep the existing **Reset Peaks** affordance somewhere sensible (it is
  already wired; you may relocate it).
- Optional: the existing In/Out/TP peak readout line.

Match the SVG's grouping and proportions. Spacing/padding per `mdsp_ui`
metrics (as in current MainView).

────────────────────────────────────────
4. SCOPE — files you may MODIFY
────────────────────────────────────────

- `Source/ui/MainView.h`
- `Source/ui/MainView.cpp`

Do NOT add new source files, new params, or touch CMake (unless you must —
prefer keeping everything in MainView). Do NOT touch `PluginProcessor`,
`Parameters`, `ParameterIDs`, HQ, or AnalyzerPro.

NON-GOALS:
- No new APVTS params, no param range/default/ID changes.
- No DSP, no gain-match logic, no I/O gain application — placeholders are
  inert visuals only.
- No bench impact (editor-only).

────────────────────────────────────────
5. PLACEHOLDER CONVENTION
────────────────────────────────────────

Placeholders must **look like real, finished controls** (so the look can
be judged) but do nothing:
- Use the same `mdsp_ui` LookAndFeel styling as the wired controls.
- Do NOT create `SliderAttachment`/`ButtonAttachment` for them.
- They may be interactive-but-inert (move freely, affect nothing) OR
  disabled — your call for the cleanest look; prefer enabled-but-inert so
  the layout reads as live.
- Give each a tooltip: "Placeholder — wired in the I/O + Gain-Match slice."
- The **Gain** placeholder specifically: positive-only feel, value "0.0 dB",
  indicator at the hard-left minimum, 0 → +24 travel (see SVG).

────────────────────────────────────────
6. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout -b slice-10-maximizer-ui-shell
cmake --build build-debug -j
cmake --build build-release -j
```

- Zero new warnings from MasterLimiter `Source/` files.
- No bench run required (editor-only; DSP/audio path untouched).

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Files modified, one-line purpose each.
3. Which controls are WIRED (to which existing param) vs PLACEHOLDER —
   an explicit table.
4. Build summary (Debug + Release, warnings).
5. A short description of the layout you produced vs the SVG (any
   deviations + why).
6. Confirmation: branch created, NO commit, NO push.
7. Open questions.

────────────────────────────────────────
8. ARCHITECT AUDITION (after Cursor reports)
────────────────────────────────────────

avishali loads the Release build in Ableton (VST3) and judges the **look**
against `docs/mockups/SLICE10_ui_layout.svg`: panel split, control grouping,
the Gain drive reading 0.0 dB at hard-left, placeholders looking finished,
existing meters/LUFS live. On look-approval, a close prompt commits the
shell; then we write the ADR + the I/O-gains/Gain-Match DSP slice that
replaces the placeholders with real params and wiring.
