# Cursor Task — Slice 16b.2: meter readouts, layout nudges, knob step granularity

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **UI refinement on the uncommitted Slice 16b branch** (`slice-16b-visual-restyle`).
> Final small batch from avishali's audit. Knob granularity is **UI-only**
> (slider interval + display decimals) — do NOT change the parameters'
> `NormalisableRange`/IDs, so the bench and frozen params are unaffected.
> **Do NOT commit, do NOT push.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Product UI only: `Source/ui/MainView.{cpp,h}`,
  `Source/ui/meters/MeterComponent.{cpp,h}`, `Source/ui/meters/
  MeterGroupComponent.{cpp,h}`, `Source/ui/MasterLimiterLookAndFeel.{cpp,h}`.
- **No parameter changes** (no `ParameterIDs.h`/`Parameters.cpp` edits) — the
  0.1 step is a slider/display setting only. No DSP, no HQ.
- Continue branch `slice-16b-visual-restyle`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. DEFAULT METER RANGE → back to Full
────────────────────────────────────────

Revert the default meter range from `Top12Db` back to **`FullRange`** (avishali
prefers Full as the default; the +/- zoom remains available).

────────────────────────────────────────
2. I/O FADER VALUE — outside the meter box
────────────────────────────────────────

The fader's value readout (the gain dB it sets) currently sits over/inside the
meter and obstructs it. Move the **fader value outside the meter box** (below
the bar, in the footer area), so nothing overlaps the meter bar. Goal: keep the
meter bar as unobstructed as possible.

────────────────────────────────────────
3. READOUT VALUES — move down to make room
────────────────────────────────────────

Shift the peak/RMS readout values **down** to make room for the relocated fader
value, so the footer stack (fader value + peak/RMS readouts) sits cleanly below
the meter without crowding. (avishali audits the exact spacing.)

────────────────────────────────────────
4. GAIN MATCH LABEL — nudge right
────────────────────────────────────────

The "GAIN MATCH" label is still slightly off the Auto/Track button — **move it
a bit to the right** so it sits properly aligned over the button.

────────────────────────────────────────
5. SP/TP READOUT — move under the SP/TP button
────────────────────────────────────────

The SP/TP readout (`SP: -inf dB`) is stranded in its old spot (near Character).
Move it to sit **directly under the SP/TP toggle button** (up by the Gain knob),
so the readout belongs to its control.

────────────────────────────────────────
6. METER CLICK → RESET PEAK
────────────────────────────────────────

Clicking on a meter must **reset its peak** (peak-hold line + latched max). This
was wired in Slice 15b; verify it still works after the restyle and fix if the
new paint/hit-area broke it. Applies to the I/O meters (and keep the GR meter's
click-reset working too).

────────────────────────────────────────
7. KNOB GRANULARITY — 0.1 dB / one decimal (Gain, Output Level/Ceiling, Clipper)
────────────────────────────────────────

For the **Gain (drive)**, **Output Level (ceiling)**, and **Clipper** knobs:
- Display **one decimal** (`0.0`, `-1.0`) instead of two (`0.00`) — 0.01 is too
  fine for the user.
- Set the **slider drag interval to 0.1 dB** so dragging moves in 0.1 steps.
- **Do this UI-side only** (the slider's interval + the value formatter / text
  display). Do NOT change the underlying `AudioParameterFloat`
  range/interval/ID — keep the params as shipped so the bench and automation
  resolution are unaffected.

────────────────────────────────────────
8. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j && cmake --build build-release -j
```
- Zero new `Source/` warnings.
- UI-only → Slice 3/4/5 unchanged (run a quick Slice 3 as sanity since knob
  granularity was touched; expect PASS unchanged since params are untouched).
- In host: meters default to Full; fader value sits below/outside the meter
  (bar unobstructed); peak/RMS readouts moved down cleanly; GAIN MATCH aligned;
  SP/TP readout under the SP/TP button; clicking a meter resets its peak; Gain/
  Output Level/Clipper read one decimal and drag in 0.1 steps.

────────────────────────────────────────
9. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log. 2. Diffs per item. 3. Build summary + Slice 3 quick result.
4. Confirmation no parameter (`Parameters.cpp`/`ParameterIDs.h`) changes.
5. `git status --short`. 6. Open questions (footer spacing, GAIN MATCH offset).
