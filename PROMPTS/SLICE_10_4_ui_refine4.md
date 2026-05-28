# Cursor Task — Slice 10.4: Maximizer UI refinement #4 (Gain knob interactivity + double-dB suffix, meter visuals + rescale, I/O panel layout)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. UI/editor-thread only — NO DSP, NO new APVTS params,
> NO param range/default/ID changes.** Continues branch
> `slice-10-maximizer-ui-shell` (last commit `2e5ec5f` WIP). Build,
> **do NOT commit, do NOT push.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Files in §3 only. No HQ edits, no AnalyzerPro edits.
- No DSP, no new APVTS params, no range/default/ID changes.
- RT-safety: audio thread untouched.
- Stay on branch `slice-10-maximizer-ui-shell`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. WHAT TO FIX (audition feedback, with screenshot evidence)
────────────────────────────────────────

### 1.1 Gain knob doesn't move
The Gain placeholder is rendered with its pointer parked at the minimum and
**does not respond to drag** (all other knobs do). Reason: Cursor's Slice 10.0
made the placeholder "snap back to 0.0 dB," which effectively immobilizes it.

**Fix:** make the Gain knob **freely interactive** — drag changes its value
visually, the value stays where the user releases (no snap-back). It is
still a **placeholder** (no `SliderAttachment`, no DSP effect) — Slice 11
wires it to a real parameter. Range and feel for the placeholder: `0.0` to
`+24.0` dB, default `0.0`, increment ~`0.1` dB, `RotaryVerticalDrag`.

### 1.2 Double "dB" in Gain readout
The Gain value currently reads **`0.0 dB dB`** — duplicate suffix.

**Fix:** make value formatting have a **single source of truth** for the
unit. Either the LookAndFeel's centred value-text formatter appends " dB"
**or** the slider's `setTextValueSuffix(" dB")` does — not both. Resolve the
duplication for all dB readouts (apply the same fix to any other knob with
the same risk).

### 1.3 Meter visuals — remove the "blue signal" artifact
The four I/O meter columns show large cyan rectangles at the bottom that
read as **stuck signal** at silence (they are actually the peak/RMS readout
boxes or the level-fill region bleeding cyan). It looks like a rendering
mistake.

**Fix:** clean the level meter rendering so that at silence the meter bar
shows **no large cyan fill** — just the dark/empty bar. The signal fill
should only appear when there is actual signal, sized to the level. The
**readout boxes (peak/RMS) at the bottom should be dark/neutral**, not cyan.
Investigate `MeterComponent::paintLevel` and the numeric-area drawing in
`Source/ui/meters/MeterComponent.cpp` and the provider config in
`Source/ui/meters/MeterGroupComponent.cpp`.

### 1.4 Meter scale — top-weighted
The I/O meters span too wide a range. avishali cares most about the loud
zone (~0 to −6 dBFS).

**Fix:** switch the I/O meter scale mode to a **top-weighted scale** so the
top values are clearly readable. Use the existing
`mdsp_ui::meters::MeterScaleMode::Top24Db` (it's already implemented in the
MeterComponent paint code — see the `Top24Db` branch). Set via
`provider.setScaleMode(MeterScaleMode::Top24Db)` in `MeterGroupComponent` for
the IN/OUT groups. The GR meter (`GainReductionMeter`) is unaffected — it
keeps its own 0..−10 dB scale.

### 1.5 Decimal places in knob readouts (regression from 10.2)
Despite the 10.2 formatting fix, **Release reads "99.999..." and Sustain
reads "4.0000..."** — i.e. raw float precision still leaks through on some
knobs. The likely cause: the LookAndFeel's centred value text in
`drawRotarySlider` is rendering `slider.getValue()` directly (or with a
default formatter) and bypassing the slider's configured textbox format.

**Fix:**
- In `MasterLimiterLookAndFeel::drawRotarySlider`, the centred value text
  must use **`slider.getTextFromValue(slider.getValue())`** — so it respects
  whatever `setNumDecimalPlacesToDisplay` + `setTextValueSuffix` each slider
  declares. Single source of truth.
- In `MainView`, set per-slider precision so readouts are clean:
  - Release: 0 decimals (e.g. `100 ms`)
  - Sustain: 1 decimal (e.g. `4.0`)
  - Lookahead: 2 decimals (e.g. `5.00 ms`) — or 1
  - Stereo Lk / M/S Lk: 0 decimals (e.g. `100%`) — or 1 if you prefer
  - Output Level / Gain placeholder: 1–2 decimals dB
  - Any other slider: pick a sensible value, no raw float spew anywhere.

This dovetails with §1.2 (the single-suffix fix): same single source for
both unit and precision.

### 1.6 I/O panel layout
Three adjustments inside the I/O panel:

a. **Section label "I/O" centered** (currently left-aligned). Center it
   horizontally above the panel content.
b. **Learn button**: still too big. Make it **small** (compact pill-style,
   sized to its label "Learn"), and **place it elsewhere** so it doesn't
   compete with the meters/faders — e.g. tucked under the "I/O" title or at
   the bottom of the panel. **Meters + faders are the priority** in this
   panel; nothing else should dominate them.
c. **L/R Link buttons**: currently large and positioned near the
   Input/Output labels at the top, where they **obscure / push the meter
   labels (L/R, IN/OUT) off-center**. Replace with **small icon-style or
   compact buttons placed at the BOTTOM of each meter group**, and
   **center the meter section labels** ("Input"/"IN", "Output"/"OUT", and
   the L/R channel letters) so they read cleanly above the meters.

Visual priority order in the I/O panel: **meters + faders > everything
else**. Compact and unobtrusive for Learn, Link toggles, LUFS readouts.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (as needed):**
- `Source/ui/MainView.{h,cpp}` — Gain interactivity, I/O panel layout
  (centered label, smaller/relocated Learn, small bottom-placed L/R Link
  buttons, centered meter labels), value-suffix de-duplication.
- `Source/ui/MasterLimiterLookAndFeel.{h,cpp}` — fix the duplicated " dB"
  appending; ensure single unit source.
- `Source/ui/meters/MeterComponent.{h,cpp}` — clean the level rendering so
  silence shows no spurious cyan fill; dark/neutral readout boxes.
- `Source/ui/meters/MeterGroupComponent.{h,cpp}` — switch IN/OUT providers
  to `MeterScaleMode::Top24Db`.

Do NOT touch `PluginProcessor`, `Parameters`, `ParameterIDs`, CMake (no new
files expected), HQ, or AnalyzerPro.

NON-GOALS: no param changes, no DSP, no Gain/Ceiling-Link restyle, faders
remain inert placeholders. GR meter (`GainReductionMeter`) untouched.

────────────────────────────────────────
3. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-10-maximizer-ui-shell
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings. No bench run (editor-only).
- Smoke checks: Gain knob drags and stays; readouts show single " dB" units;
  IN/OUT meters at silence have no large cyan blocks; resize still clean.

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Files modified, one-line purpose each.
3. Confirmation of each item (1.1–1.6).
4. Where the duplicated " dB" was coming from and how you resolved it
   (which side owns the unit suffix now).
5. The scale mode set on IN and OUT (`Top24Db`).
6. Build summary (Debug + Release, warnings).
7. Confirmation: branch unchanged, NO commit, NO push.
8. Open questions.

────────────────────────────────────────
5. ARCHITECT AUDITION
────────────────────────────────────────

avishali re-auditions in Live: Gain knob drags freely, "0.0 dB" reads once,
I/O meters are clean at silence and top-weighted, I/O label centered, Learn
small/tucked away, L/R Link buttons compact at the bottom, meter section
labels centered and visible. Once approved, the Slice 10 close prompt
collapses the WIP into a clean commit (or two), then Slice 11 begins.
