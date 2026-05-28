# Cursor Task — Slice 11a.2: click-to-reset on meter value/LED

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

avishali wants clicking directly on a meter's **value readout** or its
**clip LED** to reset peaks on that meter. `MeterComponent` already
handles this on the receiving side — its `mouseDown` calls
`onClipReset_` when the click is in the LED area and `onPeakReset_`
otherwise. The peak callback is already wired in `MeterGroupComponent`,
but the **clip callback is not set**, so clicking the LED currently does
nothing. Fix: wire it.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY:**
- `Source/ui/meters/MeterGroupComponent.cpp` — set
  `onClipResetCallback` on both `meter0_` and `meter1_` for
  `BusKind::Input` and `BusKind::Output`. Point it at the same
  `peakResetThunk` already used for the peak callback so clicking the LED
  (or the readout) both trigger `handlePeakReset` and clear the latched
  state (peak holds + clip latch + smoothers).

Do NOT touch any other file. GR meter (`GainReductionMeter`) is unaffected
— it doesn't use this path.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

In the `MeterGroupComponent` constructor's Input/Output branch (the `else`
that constructs Level meters), where this already exists:

```cpp
meter0_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);
meter1_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);
```

add right beside it:

```cpp
meter0_->setClipResetCallback (&MeterGroupComponent::peakResetThunk, this);
meter1_->setClipResetCallback (&MeterGroupComponent::peakResetThunk, this);
```

(Same thunk → same `handlePeakReset` → consistent reset semantics whether
the user clicks the LED or the value box.)

Then verify `handlePeakReset` does what it should: clears peak hold,
clears the clip latch (whether via `provider.resetPeakHold()` already, or
an explicit clip-latch clear added in §1 of Slice 11a.1). If the clip
latch isn't being cleared, fix that now — clicking the LED must clear it.

No other changes.

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
- Trigger a clip on OUT (Output Gain past 0 dB on a hot signal) → LED
  lights & latches.
- **Click the LED** → clip latch clears AND peak hold clears.
- Re-trigger clip → LED lights again.
- **Click the value readout** below the meter → resets peaks the same way.
- Same on IN meters.
- Reset Peaks button still works globally.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. The exact lines added in `MeterGroupComponent.cpp`.
3. Confirmation that `handlePeakReset` clears the clip latch (with a
   one-line note on how — existing path or a small addition).
4. Build summary (Debug + Release, warnings).
5. Confirmation: branch unchanged, NO commit, NO push.
6. Open questions.

────────────────────────────────────────
6. ARCHITECT AUDITION (after Cursor reports)
────────────────────────────────────────

avishali confirms click-LED and click-value both clear peak hold + clip
latch on IN/OUT meters. On approval → Slice 11a close prompt: single
clean commit, FF main, push. Then Slice 11b.
