# Cursor Task — Slice 11b1.2: fix meter readout truncation (drop " dB" suffix)

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
- Stay on branch `slice-11b1-gain-ceiling-link`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. WHY
────────────────────────────────────────

The Slice 11b1.1 latched-max readout is rendering as **"−12...."** in
the IN/OUT meters — truncated. Cause: the formatted string is "−12.0 dB"
at 14 px font, which is wider than the meter column's numeric area.
The vertical area is fine (30 px); only horizontal width is the issue.

Fix: format the latched max as **number only** (no " dB" suffix) — same
convention as commercial meters (Ozone / Pro-L). The meter's tick scale
beside the bar already communicates the unit. The compact format fits
cleanly in the column width at 14 px.

The bigger knob readouts (Output Level, etc.) and the fader readouts
below the meters keep their " dB" suffix — they're in wider boxes and
the unit is contextually useful there. **Only the meter peak/latched
readouts change.**

────────────────────────────────────────
2. WHAT TO DO
────────────────────────────────────────

In `Source/ui/meters/MeterGroupComponent.cpp`, change the format helper
that produces the **latched max-peak readout text** so it omits " dB":

- Finite, > −100 dBFS  → `juce::String (v, 1)` (e.g. `"-12.0"`).
- Otherwise            → `"-inf"`.

This is the helper feeding `meter*_->setNumericReadoutOverride(...)` in
the `BusKind::Input` and `BusKind::Output` branches of `sync()`. If a
single existing helper (`PeakNumericSmoother::formatDb` or similar) is
used in multiple places (e.g. also by the `MainView` TP readout via a
shared smoother), **do not change it in those other callers** — they
may want the suffix. Either:

- Add a small local `formatDbBare` helper used only for the meter group
  readout, **or**
- Inline the format in the latch update path.

Whichever is cleaner; the constraint is that **only the meter peak
readouts lose " dB"**, not the TP readout or anything else.

If, after the format change, "−12.0" still doesn't fit at 14 px in the
column width, reduce the font height in `MeterComponent`'s Level-kind
numeric draw to **12 px** (still readable, more headroom). Try 14 px
first; only drop to 12 if visibly clipped at the design size.

────────────────────────────────────────
3. SCOPE — files
────────────────────────────────────────

**MODIFY:**
- `Source/ui/meters/MeterGroupComponent.cpp` — format the latched max
  readout without the " dB" suffix.
- `Source/ui/meters/MeterComponent.cpp` — ONLY if the font size needs
  to drop from 14 → 12 px to fit. Otherwise leave untouched.

Do NOT touch any other file. Do NOT change the TP readout's format in
`MainView` (that one keeps " dB").

NON-GOALS: no DSP, no param changes, no GR meter changes, no fader/
button changes, no readout-area resizing.

────────────────────────────────────────
4. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-11b1-gain-ceiling-link
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new `Source/` warnings. No bench run.

### Smoke checks
- IN/OUT meter readouts show full numbers (e.g. `-12.0`, `-3.4`, `0.0`,
  `-inf`) — **no ellipsis / no truncation**.
- Latched max behaviour from 11b1.1 still works (number stays at the
  highest dB hit until Reset Peaks).
- Click LED / value / Reset Peaks button still clear the latch.
- TP readout elsewhere in the UI still has " dB" (untouched).
- Other knob/fader readouts unchanged.
- GR meter readout unchanged.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. The exact format change in `MeterGroupComponent.cpp` (before/after).
3. Whether you needed the 14 → 12 px font drop in `MeterComponent.cpp`
   (if yes, the line changed; if no, that file untouched).
4. Build summary (Debug + Release, warnings).
5. Confirmation: branch unchanged, NO commit, NO push.
6. Open questions.

────────────────────────────────────────
6. ARCHITECT AUDITION
────────────────────────────────────────

avishali confirms IN/OUT meter readouts now display the full number
without truncation. Then we run the **Slice 11b1 close prompt** —
single clean commit collapsing link + meter readout + readout-fit, FF
main, push. After that: **Slice 9 (limiter character)**.
