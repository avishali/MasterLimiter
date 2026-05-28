# Cursor Task — Slice 10.2: Maximizer UI refinement #2 (knob behavior + value formatting, Bypass to top, faders on meters, link-button overlap, LUFS panel)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. UI/editor-thread only — NO DSP, NO new APVTS params,
> NO param range/default/ID changes (the Ceiling range change is explicitly
> deferred to Slice 11).** Continues the uncommitted work on branch
> `slice-10-maximizer-ui-shell`. New controls (Gain drive, I/O faders,
> Gain-Match toggles, Learn) stay **placeholders**. Build, **do NOT commit**.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:
- Files in §3 only. No HQ edits, no AnalyzerPro edits.
- No DSP, no new APVTS params, **no range/default/ID changes to any existing
  param** (esp. NOT `ceiling_db` — that range change is Slice 11).
- RT-safety: audio thread untouched.
- Stay on branch `slice-10-maximizer-ui-shell`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. WHY — audition feedback
────────────────────────────────────────

Resize/scaling is now good. Remaining issues from the audition (reference
`docs/mockups/SLICE10_ui_layout.svg`):

1. **Knob behavior + readouts are wrong** — value text is drawn *over* the
   knob and overlaps the pointer (e.g. the Gain knob), value boxes show raw
   float precision (`99.9999924`, `4.0000000`), and the **rotation itself
   feels wrong** (pointer parked position / value-to-angle mapping does not
   track correctly).
2. **Bypass** should live at the **top** (header), not the footer — frees
   vertical space.
3. **I/O faders** should sit **on / beside the meters** (overlay handles on
   the In/Out meter columns, like the mockup / Ozone), not in a separate row
   below them.
4. **Gain / Ceiling Link** button **overlaps** the Output Level knob — fix
   placement.
5. **LUFS panel** (M/S/I) is **scrambled** — text overlaps / doesn't fit.

────────────────────────────────────────
2. WHAT TO FIX
────────────────────────────────────────

### 2.1 Rotary knob behavior + look (MasterLimiterLookAndFeel)

- Use a conventional rotary sweep: start angle ≈ lower-left (~225°), end
  ≈ lower-right (~315°+360), ~270° total, sweeping clockwise through the top.
- The drawn **pointer and value-arc must track `sliderPosProportional`
  exactly** across that sweep (verify min sits at the start angle, max at the
  end angle). This is the "rotation feels wrong" fix — the indicator must
  match the actual value.
- **Indicator is a short tick near the rim**, NOT a full line through the
  centre — so it never crosses the centred value text.
- **Value text centred in the knob** (formatted, see 2.2). Do **not** also
  show a separate redundant value box below the same knob — one readout only.
  Keep the parameter **name label** above the knob (per SVG).
- The big **Gain** placeholder: pointer parked at the **hard-left minimum**
  (the start angle), value `0.0 dB`.

### 2.2 Value formatting

Format every readout with sensible precision + unit. Targets:
- Gain (placeholder): `0.0 dB`
- Output Level: `−1.00 dB`
- Release: `100 ms` (0 decimals, or 1 below 10 ms)
- Sustain: `4.0` (1 decimal, no unit / "×")
- Lookahead: `5.0 ms`
- Stereo Lk / M/S Lk: `100%`
- I/O Input/Output trims (placeholders): `0.0 dB`

No raw float spew. Apply via each slider's textbox config / `textFromValue`
(or the LookAndFeel), whichever is cleaner.

### 2.3 Bypass → top

Move the **Bypass** control into the header strip (top), e.g. right-aligned.
Remove it from the footer. It stays a placeholder (no host-bypass wiring).
Reclaim the freed footer space (tighten the layout or let panels grow).

### 2.4 Faders on the meters

Overlay the I/O **fader handles on the In/Out meter columns** (combined
meter+fader, like the mockup): Input handle(s) on the IN L/R bars, Output
handle(s) on the OUT L/R bars. Keep them **vertical, L/R, default L/R-linked**
(dragging one moves both; visual link only — still placeholders, no APVTS
attachment). Keep a small dB readout per pair. Remove the separate
below-the-meters fader row.

### 2.5 Gain/Ceiling Link placement

Reposition the **Gain / Ceiling Link** toggle so it does **not** overlap the
Output Level knob. Per the SVG it sits compactly between the Gain knob and
Output Level (or just under Gain) — your call, just no overlap.

### 2.6 LUFS panel

Fix the **M / S / I LUFS** rows so they fit their box without overlap —
three clean rows, legible at the design scale.

────────────────────────────────────────
3. SCOPE — files
────────────────────────────────────────

**MODIFY:**
- `Source/ui/MasterLimiterLookAndFeel.{h,cpp}` — rotary angle/pointer/value
  fixes; centred formatted value; rim-tick indicator.
- `Source/ui/MainView.{h,cpp}` — Bypass to header, faders onto meters, link
  button reposition, LUFS panel fix, per-slider value formatting.
- `Source/ui/loudness/LoudnessNumericPanel.{h,cpp}` — only if the M/S/I
  layout fix lives there.

Do NOT touch `PluginProcessor`, `Parameters`, `ParameterIDs`, CMake (no new
files expected), HQ, or AnalyzerPro.

NON-GOALS: no param changes (incl. ceiling range — Slice 11), no DSP, no
gain-match logic, faders remain inert.

────────────────────────────────────────
4. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-10-maximizer-ui-shell
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings. No bench run (editor-only).
- Sanity: knob pointers visibly track their values; readouts formatted; no
  overlaps; resize still clean.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Files modified, one-line purpose each.
3. Rotary fix: the start/end angles used and confirmation pointer tracks
   `sliderPosProportional`.
4. Confirmation: Bypass in header, faders overlaid on meters (L/R linked),
   link button no longer overlapping, LUFS rows fixed, readouts formatted.
5. Build summary (Debug + Release, warnings).
6. Confirmation: branch unchanged, NO commit, NO push.
7. Open questions.

────────────────────────────────────────
6. ARCHITECT AUDITION
────────────────────────────────────────

avishali re-auditions in Live: knob pointers/values correct and readable,
Bypass on top, faders on the meters, no overlaps, LUFS legible, resize still
clean. Further iteration possible. Ceiling-range (0…−24) and all real wiring
come in Slice 11 (ADR + DSP).
