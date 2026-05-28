# Cursor Task — Slice 11b1.3: restore the two-line meter readout (current peak + latched max)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. UI-only — NO DSP, NO param changes.** Continues
> uncommitted Slice 11b1 work on `slice-11b1-gain-ceiling-link`. Build,
> **do NOT commit, do NOT push.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Files in §3 only. No HQ edits, no AnalyzerPro edits.
- No DSP, no APVTS changes.
- RT-safety: audio thread untouched.
- Stay on branch `slice-11b1-gain-ceiling-link`. **Do NOT commit, do NOT
  push.**

────────────────────────────────────────
1. WHY
────────────────────────────────────────

Slice 11b1.1 collapsed the IN/OUT meter readout from two lines into a
single centred line (the latched max). avishali wants the two-line
layout back. The previous two-line code passed the smoothed peak for
both arguments (so both lines displayed the same number); we'll do
better this round and make the two lines **mathematically distinct**:

- **Top line** — current peak (the existing `PeakNumericSmoother`
  output: 1 s hold then exponential decay toward the current per-block
  peak).
- **Bottom line** — latched max peak (the 11b1.1 latched value that
  only goes up until Reset Peaks clears it).

Both peak-based; **real RMS is deferred to Slice 12** (it requires
per-channel RMS DSP measurement which is out of 11b1's scope).

The format helper from 11b1.2 (`formatDbBare`, no unit suffix) is used
for both lines so the values fit in the column width.

────────────────────────────────────────
2. WHAT TO DO
────────────────────────────────────────

### 2.1 Feed two distinct values into the meter

In `Source/ui/meters/MeterGroupComponent.cpp::sync` for `BusKind::Input`
and `BusKind::Output`:

- Compute `currentPeakText_L/R` from the existing `PeakNumericSmoother`
  result (`peakSmooth*.duty`) via `formatDbBare`.
- Compute `maxPeakText_L/R` from the latched max (`maxPeakL/RDb_`) via
  `formatDbBare`.
- Call `meter*_->setNumericReadoutOverride (true, currentPeakText,
  maxPeakText)` so the meter draws **line 1 = current**, **line 2 =
  max**.

The existing `setNumericReadoutOverride(active, line1, line2)` API
already supports two distinct strings — line 1 is drawn in
`numericTextPeak` slot, line 2 in `numericTextRms` slot. We're just
giving them meaningful, different values.

### 2.2 Restore two-line drawing in MeterComponent

In `Source/ui/meters/MeterComponent.cpp::paintLevel` (Level kind only —
GR meter stays untouched):

- Split the `numericArea_` rectangle into top/bottom halves (same
  pattern as the original Slice 8.1 code: `removeFromTop
  (numBounds.getHeight() / 2)`).
- Draw `peakLine` (= line 1 from the override) on the top half,
  `rmsLine` (= line 2) on the bottom half, each centred.
- Keep the **single font size for both lines** at the current readable
  size. The numeric area is already 30 px tall, which is enough for
  two lines at ~12 px each. **Reduce the font height from 14 px to
  ~12 px** so both lines fit vertically without crowding.
- Colour suggestion: top (current peak) in the existing peak text
  colour (`theme.seriesPeak` / accent); bottom (latched max) in a
  slightly muted variant (`theme.text` with reduced alpha) so the eye
  reads the latched max as secondary/static. Keep both readable.

If after the font reduction the two-line layout still looks cramped,
adjust the numeric area height up slightly (e.g. 30 → 34 px) — but
keep the LED/value click-areas clear and Slice 11a.2's click-to-reset
intact.

### 2.3 GR meter untouched

`GainReductionMeter` is a separate component and is **not** affected
by this slice.

────────────────────────────────────────
3. SCOPE — files
────────────────────────────────────────

**MODIFY:**
- `Source/ui/meters/MeterGroupComponent.cpp` — feed two distinct
  values into `setNumericReadoutOverride`.
- `Source/ui/meters/MeterComponent.cpp` — split numeric area into two
  lines for `Kind::Level`, font down to ~12 px, colour split.

Do NOT touch any other file (PluginProcessor, Parameters,
GainReductionMeter, LookAndFeel, MainView, LoudnessNumericPanel, HQ,
AnalyzerPro).

NON-GOALS: no DSP, no APVTS changes, no real RMS computation (that's
Slice 12), no IN/OUT calibration changes (Slice 12), no readout area
restyle beyond fitting two lines.

────────────────────────────────────────
4. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-11b1-gain-ceiling-link
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new `Source/` warnings. No bench run (UI-only).

### Smoke checks
- IN/OUT meters show **two lines per channel**:
  - Top: current peak (moves with the audio, 1 s hold + decay).
  - Bottom: latched max (only goes up; stays at the max hit).
- No truncation/ellipsis at the new font size.
- Click LED / click value / Reset Peaks button clear both lines back
  to `-inf`.
- Latched max never goes below the current peak.
- Click-to-reset still works on the readout area.
- GR meter unchanged (single readout there).
- TP and knob/fader readouts unchanged (still " dB" suffix).
- Resize still clean.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. The exact two-value feeding pattern in `MeterGroupComponent.cpp`.
3. Font height + numeric area height ended up at, and whether you
   needed to bump the area beyond 30 px.
4. Build summary (Debug + Release, warnings).
5. Confirmation: branch unchanged, NO commit, NO push.
6. Open questions.

────────────────────────────────────────
6. ARCHITECT AUDITION
────────────────────────────────────────

avishali confirms the two-line readouts read as **current peak / max
peak** per channel, no truncation, reset paths work. Then the **Slice
11b1 close prompt** runs — single clean commit collapsing link + all
three meter readout patches. Slice 12 (meter panel restructure +
per-channel Peak/RMS DSP + IN/OUT calibration check) opens next.
