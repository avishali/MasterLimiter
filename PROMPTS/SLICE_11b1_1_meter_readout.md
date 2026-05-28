# Cursor Task — Slice 11b1.1: meter peak readout (latched max + larger text)

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

Two problems with the current Input/Output meter numeric readouts —
both matter for a mastering tool where the user must read levels
accurately:

1. **The displayed number does not match the latched max.** The meter
   bar already draws a separate "max peak" tick line (the
   `renderState_.maxPeakNorm`-driven yellow line in
   `MeterComponent::paintLevel`), but the numeric **text** under the
   meter follows the existing `PeakNumericSmoother` — a ~1 s hold then
   exponential decay back toward the current sample. So the *number*
   drifts down even though the *line* stays at the true max. For
   mastering you want the number = the latched max in dB, held until
   the user resets.
2. **The text is too small to read while monitoring.** Increase the
   font size and the readout area so the number is comfortable to read
   from a normal listening distance.

The GR meter (`GainReductionMeter`) is a separate component and is
**not** affected by this slice — its readout stays as-is.

────────────────────────────────────────
2. WHAT TO DO
────────────────────────────────────────

### 2.1 Latched max-peak per channel for the numeric readout

In `MeterGroupComponent` (Input + Output bus kinds only), track a per-
channel **latched max peak in dB**:

- Two `float` members on the component: `maxPeakLDb_`, `maxPeakRDb_`,
  initialised to `-200.0f` (or `-std::numeric_limits<float>::infinity()`
  if cleaner — pick one and be consistent with the format helper).
- On each `sync()` call (Input/Output branches only):
  - `maxPeakLDb_ = std::max (maxPeakLDb_, lPeak)` (only if `lPeak` is
    finite — guard against NaN).
  - Same for R.
  - Format the displayed text from the latched value:
    `"<value> dB"` when finite and above the floor, else `"−inf"` (or
    keep the existing `PeakNumericSmoother::formatDb` helper but feed
    it the latched value instead of the smoothed value).
- The existing `setNumericReadoutOverride (active, lText, rText)` call
  still drives the meter — just change the values you put into it.

The existing `PeakNumericSmoother` can stay around if it's used
elsewhere (e.g. the TP readout in `MainView` uses an instance of it —
leave that alone). For the *meter group* readouts, switch to the
latched-max format.

### 2.2 Reset behaviour

`MeterGroupComponent::handlePeakReset` (already called by the global
Reset Peaks button **and** the per-meter click-to-reset wired in
Slice 11a.2) must now also clear the latched max:

- Reset `maxPeakLDb_` and `maxPeakRDb_` back to the same floor sentinel
  used at init (`-200.0f` or `-inf`).
- Existing peak-hold / clip-latch / smoother resets stay.
- Confirm both globalReset-via-button AND click-on-LED-or-value clear
  this new latch (they should, since both flow through
  `handlePeakReset`).

### 2.3 Larger readout text

In `MeterComponent`:
- Increase the numeric-area height in `resized()` (currently ≈ 20 px on
  the design-size canvas — bump to something like **28–32 px** so a
  larger font fits cleanly with padding).
- Increase the font height for the numeric box from the current `10.0f`
  to roughly **13–15 px**. Use the existing `mdsp_ui` typography path
  if it exposes a label-ish font; otherwise a `juce::Font` with the
  larger height is fine. Keep the readout single-line.
- The two-line peak/rms split currently in `paintLevel` /
  `paintGainReduction` for Level meters can become a single centred
  line (the latched-max dB) since we no longer need a separate "peak
  with decay" line distinct from the latched max. Keep it readable —
  one clean line.

(If the typography change makes the readout area collide with the
fader handle overlay above it, shrink the fader's vertical extent so
the readout area stays exposed and clickable — Slice 11a.2's click-to-
reset on the readout must continue to work.)

### 2.4 GR meter untouched

`GainReductionMeter` is a separate component (Slice 10.1). Do not
change its readout, scale, or layout this slice.

────────────────────────────────────────
3. SCOPE — files
────────────────────────────────────────

**MODIFY:**
- `Source/ui/meters/MeterGroupComponent.{h,cpp}` — latched max-peak
  tracking + reset hook + readout text feed.
- `Source/ui/meters/MeterComponent.{h,cpp}` — larger numeric font and
  numeric-area sizing for Level kind.
- `Source/ui/MainView.cpp` — only if the fader-overlay bounds need a
  small reduction so the readout area stays exposed.

Do NOT touch any other file (`PluginProcessor.*`, `Parameters.*`,
`GainReductionMeter.*`, `LoudnessNumericPanel.*`, LookAndFeel,
PluginEditor, CMakeLists, HQ, AnalyzerPro).

NON-GOALS: no DSP, no APVTS changes, no link/coupling changes (Slice
11b1 logic is final), no GR meter changes.

────────────────────────────────────────
4. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-11b1-gain-ceiling-link
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new `Source/` warnings. No bench run required (UI only).

### Smoke checks
- Play hot audio → max peak readout climbs to the highest dB hit and
  **stays there** (no decay), matching the yellow max-peak line on the
  bar.
- Stop audio / quieter section → number does NOT drift down; it stays
  at the latched max.
- Click the **Reset Peaks** button → readout clears to `−inf`.
- Click the meter's clip LED → same reset (Slice 11a.2 behaviour).
- Click the meter's value/readout → same reset.
- Readout text is comfortably readable from normal listening distance.
- Resize still clean; fader interaction still works (click-to-snap
  still disabled per Slice 11a.3).
- GR meter unchanged.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Files modified, one-line purpose each.
3. Where the latched max lives, how it's reset, where it's formatted
   for display.
4. Font height + numeric-area height chosen for the Level readout, and
   whether you adjusted fader bounds.
5. Build summary (Debug + Release, warnings).
6. Confirmation: branch unchanged, NO commit, NO push.
7. Open questions.

────────────────────────────────────────
6. ARCHITECT AUDITION
────────────────────────────────────────

avishali confirms readouts now latch + are readable, reset paths still
work, and (with the better readouts) re-runs the Slice 11b1 link smoke
checks — including the range-edge clamp test. On approval, the Slice
11b1 close prompt collapses everything into one clean commit, FF main,
push. Then Slice 11b2.
