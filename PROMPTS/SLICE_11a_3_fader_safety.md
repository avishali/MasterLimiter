# Cursor Task — Slice 11a.3: I/O fader click-to-snap safety (drag-only faders)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. UI-only — NO DSP, NO param changes.** Continues
> uncommitted Slice 11a work on `slice-11a-io-gains`. Build, **do NOT
> commit, do NOT push.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Files in §3 only. No HQ edits, no AnalyzerPro edits.
- No DSP, no APVTS changes.
- RT-safety: audio thread untouched.
- Stay on branch `slice-11a-io-gains`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. WHY
────────────────────────────────────────

The four I/O L/R faders sit overlaid on the IN/OUT meter columns. By
default, `juce::Slider` snaps the thumb to the click position on any
mouseDown within the track — so an **accidental click on the fader (while
monitoring loud audio) instantly jumps the gain**, which can cause a sudden
level spike. This is unacceptable for a mastering tool.

Fix: make those faders **drag-only** — the user must click on the thumb
itself and drag; clicking elsewhere on the fader/meter doesn't move the
value. The standard JUCE knob for this is
`Slider::setSliderSnapsToMousePosition (false)`.

While we're at it: preserve the **click-to-reset** behaviour just added in
Slice 11a.2 — clicking the meter's clip **LED** (at the top of the column)
and the **value readout** (at the bottom) must still reach the meter and
trigger the reset. Since the fader handle/track is in the middle of the
column, this should still work — but verify in the smoke check.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY:**
- `Source/ui/MainView.cpp` — call `setSliderSnapsToMousePosition (false)`
  on each of the four I/O L/R faders (Input L, Input R, Output L, Output R)
  during their setup.

Do NOT touch any other file. Do NOT apply this to the rotary knobs (rotary
sliders aren't affected by track-snap and changing their behaviour would
break expectations). Do NOT touch GR meter or its component.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

Where each of the four I/O L/R faders is constructed/configured in
`MainView`, add the call. For example, alongside the existing
`setSliderStyle (Slider::LinearVertical)` (or whatever style is used for
the overlaid faders):

```cpp
sldIoInputL_.setSliderSnapsToMousePosition (false);
sldIoInputR_.setSliderSnapsToMousePosition (false);
sldIoOutputL_.setSliderSnapsToMousePosition (false);
sldIoOutputR_.setSliderSnapsToMousePosition (false);
```

(Use the actual slider variable names from the existing code.) The result:
clicking the fader anywhere off the thumb does nothing — the user must
grab the thumb and drag.

If you find that the fader's hit area covers the meter's LED area at the
top or the value-readout at the bottom (so click-to-reset on the meter
would be blocked), shrink the fader's bounds so it only covers the meter
**bar** region — leave the LED and readout exposed for the meter to
receive clicks. The Slice 11a.2 click-to-reset behaviour must continue to
work.

No other behaviour changes. Specifically: do NOT change drag sensitivity,
velocity mode, double-click behaviour, modifier behaviour, or any other
slider option.

────────────────────────────────────────
4. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-11a-io-gains
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new `Source/` warnings. No bench run.

### Smoke checks
- Click anywhere on an I/O fader's track (not the thumb) → thumb does
  NOT jump; nothing happens.
- Click the thumb and drag → fader moves normally; L/R link still
  mirrors when on.
- Double-click behaviour (reset-to-default) preserved (if previously
  worked).
- Click the meter's **clip LED** at the top → clip latch + peak hold
  clear (Slice 11a.2 behaviour intact).
- Click the meter's **value readout** at the bottom → same reset.
- Global Reset Peaks button still works.
- GR meter and the rotary knobs (Gain, Output Level, Release, etc.)
  unchanged.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. The exact lines added in `MainView.cpp`.
3. Note whether you had to adjust fader bounds to keep the LED/readout
   click-areas exposed (and how).
4. Build summary (Debug + Release, warnings).
5. Confirmation: branch unchanged, NO commit, NO push.
6. Open questions.

────────────────────────────────────────
6. ARCHITECT AUDITION (after Cursor reports)
────────────────────────────────────────

avishali confirms accidental fader clicks do not move the value and
click-to-reset on the meter still works. On approval →
`SLICE_11a_4_close.md` commits the slice cleanly, then I FF main + push,
then Slice 11b.
