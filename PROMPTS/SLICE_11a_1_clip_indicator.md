# Cursor Task — Slice 11a.1: wire the clip indicator on Input/Output meters

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. UI-only — NO DSP, NO param changes.** Continues the
> uncommitted Slice 11a work on branch `slice-11a-io-gains`. Build, **do
> NOT commit, do NOT push.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Files in §3 only. No HQ edits, no AnalyzerPro edits.
- No DSP, no new APVTS params, no range/default/ID changes.
- RT-safety: audio thread untouched.
- Stay on branch `slice-11a-io-gains`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. WHY
────────────────────────────────────────

Slice 11a smoke check: when Output Gain is pushed past 0 dB the OUT meter
correctly reads above 0 dBFS (post-Output-Gain measurement is working) —
but **no clip indicator lights up**. Same on the IN meter side if input
content is already hot.

Cause: `MeterComponent` already draws a latched clip LED (the small dot at
the top of each meter — red on clip, neutral otherwise), driven by
`renderState_.clipLatched`. That bit flows from
`MeterRenderStateProvider::updateFromValues(peakDb, rmsDb, clipped, bypassed)`.
But in `MeterGroupComponent::sync`, the `clipped` arg is currently
**hardcoded `false`** for both Input and Output buses, so the LED never
turns on.

Fix: compute `clipped` from the actual peak being pushed into the provider.

────────────────────────────────────────
2. TRINITY (lightweight)
────────────────────────────────────────

Read:
- `Source/ui/meters/MeterGroupComponent.{h,cpp}` (the `sync` method that
  calls `provider*_.updateFromValues(...)`).
- `Source/ui/meters/MeterComponent.cpp` paint path that uses
  `renderState_.clipLatched` (just to confirm the existing latched-LED
  behaviour — should need no change).

────────────────────────────────────────
3. SCOPE — files
────────────────────────────────────────

**MODIFY:**
- `Source/ui/meters/MeterGroupComponent.cpp` — feed real `clipped` into the
  provider for `BusKind::Input` and `BusKind::Output`.

Do NOT touch any other file. The GR `BusKind` path (if still reached
through this component for anything) stays untouched — GR's
`GainReductionMeter` is a separate component, so this scope is just the
Input/Output buses.

────────────────────────────────────────
4. WHAT TO DO
────────────────────────────────────────

In `MeterGroupComponent::sync`, where `provider0_.updateFromValues(...)`
and `provider1_.updateFromValues(...)` are called for `BusKind::Input` and
`BusKind::Output`, replace the hardcoded `false` clipped arg with:

```cpp
constexpr float kClipThresholdDb = 0.0f;  // digital full-scale
const bool clippedL = std::isfinite (lPeak) && lPeak >= kClipThresholdDb;
const bool clippedR = std::isfinite (rPeak) && rPeak >= kClipThresholdDb;

provider0_.updateFromValues (lPeak, lPeak, clippedL, false);
provider1_.updateFromValues (rPeak, rPeak, clippedR, false);
```

Keep the bypass arg as `false` (no bypass feature this slice).

The LED is **latched** by the provider — once lit it stays lit until the
user hits **Reset Peaks** (which already calls `provider.resetPeakHold()`
via `handlePeakReset`). Confirm the reset path also clears `clipLatched`
in the provider; if `resetPeakHold` does not clear it, **add a separate
provider call to clear the latched-clip state in `handlePeakReset`** — but
only if necessary; check first.

No other behavior changes. No threshold tuning beyond `0.0 dB` (digital
full-scale is the right line for a mastering ceiling-respecting tool).

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-11a-io-gains
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new `Source/` warnings. No bench run needed (UI-only).

### Smoke checks for the architect audition
- Push Output Gain past 0 dB on a non-silent track → both OUT L/R clip LEDs
  light red and **stay lit** (latched).
- Pull Output Gain back below 0 dB → LEDs remain lit (latched).
- Click **Reset Peaks** → LEDs clear.
- IN meter: hot input source above 0 dBFS at the IN measurement point
  (post-I/O-Input) → IN clip LEDs light & latch the same way.
- GR meter visually unchanged (no clip LED there by design).

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. The `MeterGroupComponent.cpp` diff for the `clipped` wiring + a note on
   whether `handlePeakReset` already clears the latched clip (or needed a
   small addition).
3. Build summary (Debug + Release, warnings).
4. Confirmation: branch unchanged, NO commit, NO push.
5. Open questions.

────────────────────────────────────────
7. ARCHITECT AUDITION (after Cursor reports)
────────────────────────────────────────

avishali confirms clip LEDs light + latch on IN/OUT meters when peak ≥ 0
dBFS, and Reset Peaks clears them. On approval → Slice 11a close prompt
(single clean commit + FF main + push).
