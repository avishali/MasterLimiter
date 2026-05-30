# Cursor Task — Slice 16: UI/UX interaction (type-in, tooltips, hide dead controls, layout, clip tidy)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product UI slice — no DSP/param/audio change.** Beta-readiness pass:
> (1) double-click type-in value entry on all value controls, (2) tooltips on
> all controls, (3) label clarity, (4) **hide the Lookahead and T/S Sustain
> controls** (frozen IDs retained at default) and re-lay the cluster cleanly,
> (5) finalize Color-knob placement, (6) move the clip-ballistics free
> functions out of `GainReductionMeter.cpp` into their own file. Branch off the
> new `main` (after the auto-release close). **Do NOT push.**

> **Ordering:** assumes auto-release has closed and is on `main`. If not, STOP.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product UI edits only: `Source/ui/MainView.{cpp,h}`, `Source/ui/meters/
  GainReductionMeter.cpp` (+ new `Source/ui/meters/ClipBallistics.{h,cpp}`),
  `CMakeLists.txt`. No DSP, no `mdsp_dsp`/`mdsp_ui` source, **no parameter
  changes** (hiding a control must NOT remove its frozen ID — the param stays
  in the APVTS at its default).
- No ADR. UI-thread only.
- Branch `slice-16-ui-interaction` off `main`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. HIDE LOOKAHEAD + T/S SUSTAIN (keep params at default)
────────────────────────────────────────

- Remove the **Lookahead** slider (`sldLookahead_`/`lblLookahead_`) and the
  **T/S Sustain** slider (`sldReleaseSustain_`/`lblReleaseSustain_`) from the
  UI: drop their `addAndMakeVisible`, `setBounds`, styling, and their
  `SliderAttachment`s. You may delete the now-unused members.
- **Do NOT remove the `lookahead_ms` or `release_sustain_ratio` parameters** —
  they stay frozen in `Parameters.cpp`/`ParameterIDs.h` at their current
  defaults (lookahead fixed 5 ms in the engine; sustain ratio inert under the
  `max` combinator). The engine behaviour is unchanged.

────────────────────────────────────────
2. RE-LAY THE CLUSTER + COLOR PLACEMENT (close up cleanly)
────────────────────────────────────────

- With Lookahead + T/S removed and the auto-release **Auto toggle + Auto-mode
  selector** present, re-lay the release/character cluster so the remaining
  controls (Character, Release knob, Auto toggle, Auto-release mode selector)
  sit balanced and centered — no leftover gaps.
- Finalize the **Color** knob placement (the single Link knob + Stereo-Mode
  toggle from 7b are its neighbours now). Place Color where it reads naturally
  in the maximizer/stereo cluster.
- Keep the overall window size and the clean/dark/teal aesthetic; the layout
  must stay tidy on resize. avishali audits exact placement in-host.

────────────────────────────────────────
3. TYPE-IN VALUE ENTRY (all value controls)
────────────────────────────────────────

- Make every value control's text box **editable** (double-click to type):
  Drive, Ceiling, I/O Input L/R, I/O Output L/R, Color, Link, Release, Clipper
  drive, and any other `%`/dB rotary or fader. (Choice/toggle controls stay
  click-to-select.)
- Ensure typed entry **parses the unit** correctly (e.g. typing `-6` or
  `-6 dB` into a dB control, `50` or `50%` into a % control) and clamps to the
  param range. Use the existing display formatters for the reverse direction so
  the displayed text and the parsed text round-trip.
- Confirm via APVTS: typed values update the parameter (and host automation
  state) exactly like dragging.

────────────────────────────────────────
4. TOOLTIPS + LABEL CLARITY
────────────────────────────────────────

- Add a concise, accurate **tooltip** to every interactive control that lacks
  one (knobs, faders, toggles, selectors, meter +/-, RMS toggle, mode/Link,
  Color, Auto/Auto-mode, Bypass, on/off, Gain-Match, Gain⇄Ceiling Link, I/O
  links). One short sentence each — what it does + units/range where helpful.
- Quick **label clarity** pass: fix any abbreviation that reads poorly now that
  the layout changed (e.g. ensure "Color", "Link", "Auto" etc. are clear).
  Don't rename parameters — display labels only.

────────────────────────────────────────
5. CLIP-BALLISTICS FILE TIDY (the backlog item)
────────────────────────────────────────

The clip-ballistics free functions (`makeClipBallisticsState`,
`resetClipBallistics`, `processClipReadout`, `processClipLed`,
`getClipReadoutCurrent`) currently live in `GainReductionMeter.cpp` purely as a
firewall TU (it's the one place that includes the new-system `MeterTypes.h`
without the old-system `MeterRenderState.h`). Move them to a dedicated
`Source/ui/meters/ClipBallistics.{h,cpp}`:

- `ClipBallistics.h`: the opaque `ClipBallisticsState` forward-decl + deleter +
  free-function declarations (move from `MainView.h`).
- `ClipBallistics.cpp`: includes `MeterTypes.h`/`MeterBallistics.h`/
  `PeakHoldModel.h` (the new system) and defines `ClipBallisticsState` + the
  functions. **Must NOT include `MeterRenderState.h`** (keeps the firewall).
- `MainView.cpp`/`GainReductionMeter.cpp` include `ClipBallistics.h`; remove the
  functions from `GainReductionMeter.cpp`.
- Add `ClipBallistics.{h,cpp}` to `CMakeLists.txt` `target_sources`.
- Behaviour identical — pure relocation.

────────────────────────────────────────
6. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout -b slice-16-ui-interaction
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings.
- UI-only → Slice 3/4/5 unchanged (optional sanity).
- In a host: Lookahead + T/S knobs gone, cluster looks balanced; Color placed
  well; double-click any value control → type a value (with/without units) →
  it applies + clamps; tooltips appear on hover; clip readout/LED unchanged
  (relocation only).

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Diffs: hidden controls + cluster relayout + Color placement; type-in
   enablement + unit parsing; tooltips/labels; ClipBallistics file move +
   CMakeLists.
3. Build summary (Debug + Release, warnings).
4. Confirmation params for the hidden controls remain in the APVTS at default.
5. `git status --short` (slice-16 branch, no commit).
6. Open questions (placement specifics for avishali's audit).

────────────────────────────────────────
8. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali audits the layout (cluster balance, Color placement), type-in on each
control, tooltips, and confirms clip behaviour is unchanged. Tune placement by
eye. On approval → architect closes Slice 16, then Slice 17 (packaging:
smoothing/default audit, presets, version stamp). **Do not self-close.**
