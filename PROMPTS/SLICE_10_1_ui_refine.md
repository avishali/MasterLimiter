# Cursor Task — Slice 10.1: Maximizer UI refinement (look-and-feel pass + layout fixes + dedicated GR meter + locked-aspect scaling)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. UI/editor-thread only — NO DSP, NO new APVTS params,
> NO range/ID changes to existing params.** Continues the uncommitted shell
> on branch `slice-10-maximizer-ui-shell`. Bring the build's look-and-feel up
> to the mockup, fix resize, move/restyle controls, and add a dedicated
> Gain-Reduction meter. New controls (Gain drive, I/O faders, Gain-Match
> toggles, Learn) remain **placeholders** (no DSP). Build, **do NOT commit** —
> architect re-auditions.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Files in §4 only. No HQ edits, no AnalyzerPro edits.
- No DSP, no new APVTS params, no range/default/ID changes to existing params.
- RT-safety: audio thread untouched.
- Stay on branch `slice-10-maximizer-ui-shell`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. WHY
────────────────────────────────────────

The Slice 10 shell locked the layout but the audition surfaced refinements:
1. **Resize scrambles visuals** — controls overlap / blur at non-default sizes.
2. **Look-and-feel** should match the mockup `docs/mockups/SLICE10_ui_layout.svg`
   (clean dark framed panels, teal accent, arc-style knobs with a parked
   pointer, generous spacing, muted letter-spaced section labels) — not the
   flatter default look the first build came out with. **The SVG is the
   visual reference, not just a layout diagram.**
3. **Reset peaks** belongs in the I/O metering panel, not the footer.
4. **I/O Input/Output trims** should be **vertical L/R faders** — 2 for Input,
   2 for Output — **default L/R-linked**.
5. The **GR meter** must be purpose-built (top-down, short scale, no clip LED,
   no peak/RMS box), not the reused level-meter component.

This is a first look-and-feel pass; expect further visual iteration after.

────────────────────────────────────────
2. TRINITY (lightweight)
────────────────────────────────────────

Read: `docs/mockups/SLICE10_ui_layout.svg`, `Source/ui/MainView.{h,cpp}`,
`Source/PluginEditor.{h,cpp}`, `Source/ui/meters/MeterComponent.{h,cpp}` and
`MeterGroupComponent.{h,cpp}` (for the GR rendering to replace),
`third_party/melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/{Theme,LookAndFeel,UiContext,Metrics,Typography}.h`
(theme tokens + existing LookAndFeel to extend). Output the retrieval log.

────────────────────────────────────────
3. WHAT TO BUILD
────────────────────────────────────────

### 3.1 Locked-aspect + uniform scaling (fixes resize)

Lay the UI out **once at a fixed design size** and scale uniformly — do NOT
write per-size responsive math.

- In `PluginEditor`: set the constrainer to a **fixed aspect ratio** matching
  the design (1100×620 → `setFixedAspectRatio (1100.0 / 620.0)`), keep min
  ~960×540-equivalent and a sane max, all on that ratio.
- Keep `MainView` (or an inner content component) sized to the **design size
  (1100×620)** and lay everything out in absolute coordinates at that size.
- In `editor.resized()`, compute `scale = getWidth() / 1100.0f` and apply
  `mainView.setTransform (juce::AffineTransform::scale (scale))`, with
  `mainView` bounds at the design size. The whole UI then scales crisply with
  no overlap at any allowed size.

### 3.2 Look-and-feel pass (match the SVG)

Bring the visuals to the mockup. Implement via a product LookAndFeel
(subclass of `mdsp_ui::LookAndFeel` or `juce::LookAndFeel_V4`) and/or custom
`paint`, your call — but hit these targets from the SVG:

- **Panels**: rounded-rect framed panels (fill ≈ theme panel, subtle border),
  with muted, letter-spaced caps section labels ("MAXIMIZER", "I/O").
- **Background / header**: dark base, header bar with the teal "MasterLimiter"
  title + "v0.2 · Maximizer" subtitle.
- **Knobs**: dark body, a thin track arc, a **teal pointer/value arc**, value
  text below, label above (see the Gain knob in the SVG). The big **Gain**
  knob shows its pointer parked at the **hard-left minimum** at 0.0 dB.
- **Accent**: teal (theme accent) for active handles/arcs/title.
- **Faders**: vertical, teal handle.
- **Spacing**: generous padding/gutters per the SVG, not cramped.

Use `mdsp_ui` theme tokens (colors, metrics, typography) as the source of
truth where possible rather than hard-coding hexes.

### 3.3 Move / restyle controls

- **Reset peaks** → into the **I/O panel**, near the meters (remove from the
  footer). Stays wired to its existing handler.
- **I/O Input / Output trims** → **vertical L/R faders**: Input = 2 faders
  (L, R), Output = 2 faders (L, R), placed with the In/Out meters. Each pair
  has a **link toggle defaulting ON** so dragging one moves both (visual link
  only — these are still **placeholders**, no APVTS attachment). Keep a dB
  readout per pair (or per fader) like the SVG.
- Footer keeps **Bypass** (placeholder). The In/Out/TP peak line may stay or
  move into the I/O panel — your call for cleanest look.

### 3.4 Dedicated Gain-Reduction meter (new component)

Create `Source/ui/meters/GainReductionMeter.{h,cpp}`:
- **Top-down** fill (0 dB at top, grows downward with reduction).
- **Short scale** (e.g. 0 / −3 / −6 / −12, or 0..−20) — NOT the long level
  scale.
- **No clip LED**, **no peak/RMS readout box**. A small GR-dB numeric is OK.
- L and R bars (fed the same mono GR value, as today) to sit visually beside
  the level meters.
- Driven LIVE by the existing processor GR accessor (`getCurrentGrDb()`),
  polled from the editor timer like the other meters. This component is
  WIRED, not a placeholder.
- Replace the GR `MeterGroupComponent` usage in `MainView` with this.

────────────────────────────────────────
4. SCOPE — files
────────────────────────────────────────

**CREATE:**
- `Source/ui/meters/GainReductionMeter.h`
- `Source/ui/meters/GainReductionMeter.cpp`
- (optional) `Source/ui/MasterLimiterLookAndFeel.{h,cpp}` if you implement the
  look via a LookAndFeel subclass.

**MODIFY:**
- `Source/ui/MainView.{h,cpp}` — layout at design size, control moves, faders,
  GR meter swap, styling hookup.
- `Source/PluginEditor.{h,cpp}` — fixed-aspect constrainer + uniform scale
  transform; install the product LookAndFeel if added.
- `CMakeLists.txt` — register any new files.

Do NOT touch `PluginProcessor`, `Parameters`, `ParameterIDs`, HQ, AnalyzerPro.

NON-GOALS: no new params, no DSP, no gain-match logic, no I/O gain application
(faders inert), no bench impact.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # must be slice-10-maximizer-ui-shell
cmake --build build-debug -j
cmake --build build-release -j
```

- Zero new warnings from MasterLimiter `Source/` files.
- No bench run required (editor-only).
- Sanity: resize the editor across its range — visuals must scale cleanly
  with no overlap/blur (that's the whole point of §3.1).

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Files created/modified, one-line purpose each.
3. How you implemented locked-aspect scaling (constrainer + transform).
4. Look-and-feel approach (LookAndFeel subclass vs custom paint) + which SVG
   targets you hit.
5. GR meter: scale chosen, confirmation no clip LED / no peak-RMS box, still
   live from `getCurrentGrDb()`.
6. Build summary (Debug + Release, warnings).
7. Confirmation: branch unchanged, NO commit, NO push.
8. Open questions.

────────────────────────────────────────
7. ARCHITECT AUDITION (after Cursor reports)
────────────────────────────────────────

avishali loads Release in Ableton and judges look-and-feel vs the SVG,
resize stability (drag the window — must stay clean), Reset-peaks in the I/O
panel, vertical L/R linked faders, and the dedicated GR meter. Further visual
iteration expected; on look-approval the shell + polish get committed, then
ADR + Slice 11 wires the real I/O gains and Gain-Match.
