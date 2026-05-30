# Cursor Task — Slice 16b.1: control + meter refinements

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **UI refinement on the uncommitted Slice 16b branch** (`slice-16b-visual-restyle`).
> avishali approved the restyle; this is the fix/tune pass. No DSP/param change
> (Character & Auto-release-mode stay their existing Choice params, only their
> widget changes). **Do NOT commit, do NOT push.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Product UI only: `Source/ui/MainView.{cpp,h}`,
  `Source/ui/MasterLimiterLookAndFeel.{cpp,h}`,
  `Source/ui/meters/MeterComponent.{cpp,h}`, `Source/ui/meters/
  MeterGroupComponent.{cpp,h}`. No params, no DSP, no HQ.
- Continue branch `slice-16b-visual-restyle`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. CHARACTER → 3-BUTTON SEGMENTED SELECTOR
────────────────────────────────────────

Replace the **Character** slider with a **3-segment selector** (Clean | Tight |
Aggressive), all three visible at all times, styled exactly like the other
segmented toggles, placed together in the cluster. Bind to the existing
`character` Choice param (index 0/1/2). Remove the old slider + its attachment.
Preserve any adjacent readout meaningfully (relocate cleanly; don't drop a real
readout).

────────────────────────────────────────
2. AUTO-RELEASE MODE → 3-BUTTON SEGMENTED (not a dropdown)
────────────────────────────────────────

The Auto-release mode is still an old-style dropdown. Make it a **3-segment
selector** (Transparent | Balanced | Reactive), all visible, matching the other
toggles, bound to `auto_release_mode`. Keep the greying behaviour (active only
when Auto is on).

────────────────────────────────────────
3. TYPE-IN EDITOR — fit inside the knob + click-away closes
────────────────────────────────────────

- Make the inline edit box **smaller so it stays within the knob** (does not
  overflow outside the knob bounds / cover neighbours).
- **Any click outside the editor closes it** (commit). Keep double-click to
  open, Enter to commit, Esc to cancel.

────────────────────────────────────────
4. GAIN MATCH LABEL — center over the Auto/Track button
────────────────────────────────────────

Center the **"GAIN MATCH"** header text horizontally over the **Auto/Track**
button (the cluster's anchor) so the label and button align. (avishali confirms
the exact alignment in-host.)

────────────────────────────────────────
5. METER SCALE NUMBERS — not covered by the tick lines
────────────────────────────────────────

In the centre dB scale, the number labels are overlapped by their tick lines.
Reposition so each number sits **clear of / aligned to** its line (label
adjacent, line not crossing the text).

────────────────────────────────────────
6. METER OVER/CLIP RED — must not cover the bar when zoomed
────────────────────────────────────────

When zooming with the +/- range, the red (above-0 / clip) region overlays and
covers the meter bar. Fix the over/clip rendering so it is correct at **every**
range mode and never obscures the bar (clip it to the proper region / draw it
as a thin cap or top indication, not a fill over the bar).

────────────────────────────────────────
7. SCALE NUMBER ALIGNMENT — "0" vs signed numbers
────────────────────────────────────────

The "0" label doesn't align with the others because the signed labels carry a
"+"/"−". Align them consistently (e.g. right-align the number column, or pad/
format so "0", "+6", "−12" share a common alignment).

────────────────────────────────────────
8. ZOOM +/- BUTTONS — render "+" and "−", not "..."
────────────────────────────────────────

The zoom buttons currently show "..." (ellipsis) instead of "+" — the restyle
sized them too small for the glyph (JUCE truncates to ellipsis). Size them so
the **"+" and "−"** glyphs render fully and crisply (or draw the glyphs in the
LookAndFeel rather than relying on button text).

────────────────────────────────────────
8b. DEFAULT METER RANGE → top-zoom (working-range emphasis)
────────────────────────────────────────

Change the **default** meter range (the +/- `ScaleMode` at load) from
`FullRange` to **`Top12`** (top 0…−12 dB — the limiter's working area), so the
working range fills the meter on load. Keep the linear scale and the +/-
control intact so the user can zoom out to the floor. (avishali may retune the
default between Top12/Top24 in-host.)

────────────────────────────────────────
8c. I/O FADERS — must NOT cover the meter signal
────────────────────────────────────────

The I/O gain faders sit **on top of** the level meters: the meter draws the
signal (peak/RMS coloring from Slice 15b), the fader is only the gain handle.
The 16b restyle gave the fader an opaque gradient **fill**, which now covers
the meter's signal coloring. Fix: render the I/O fader as a **slim handle +
thin/transparent track only** — no opaque signal-style fill — so the meter
underneath stays fully visible. The handle stays clearly grabbable and distinct
(e.g. a slim teal/neutral handle), but everything else is transparent over the
meter. (If `drawLinearSlider` is shared with non-fader sliders, scope this so
only the I/O meter-faders change.)

────────────────────────────────────────
9. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j && cmake --build build-release -j
```
- Zero new `Source/` warnings. UI-only → Slice 3/4/5 unchanged.
- In host: Character is 3 visible segments; Auto-release mode is 3 segments
  (not a dropdown); double-click editor fits in the knob and closes on any
  outside click; GAIN MATCH centered over Auto/Track; scale numbers clear of
  lines + aligned (0 with +6 etc.); red never covers the bar at any zoom; +/-
  buttons show + and −.

────────────────────────────────────────
10. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log. 2. Diffs per item. 3. Build summary. 4. `git status --short`.
5. Open questions (placement specifics; default range Top12 vs Top24).
