# Cursor Task — Slice 15: meter ballistics (GR + Clip)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **UI-only slice. No DSP / engine / parameter changes.** Replaces the GR
> meter's sluggish symmetric 100 ms smoothing with proper meter ballistics
> (near-instant attack, ~300 ms release) and adds a peak-hold (hold ~1 s then
> decay) to the L|R GR bars; applies matching ballistics to the Clip readout
> number and the clip LED. First item of the beta-prep batch. Branch off
> product `main`. **Do NOT push** — architect handles push + close.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product UI edits only: `Source/ui/meters/GainReductionMeter.{cpp,h}`,
  `Source/ui/MainView.{cpp,h}`, and one small new header
  `Source/ui/meters/MeterBallistics.h` (shared helper). Add it to
  `CMakeLists.txt` `target_sources` if headers are listed there.
- NO engine / `PluginProcessor` / parameter / `mdsp_dsp` changes. The meters
  keep reading the same atomic snapshots; only the *display* smoothing
  changes. No new params, no ADR.
- This is UI-thread display logic only — runs in the existing 30 Hz meter sync,
  not the audio thread.
- Branch `slice-15-meter-ballistics` off product `main`. **Do NOT commit, do
  NOT push.**

────────────────────────────────────────
1. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `Source/ui/meters/GainReductionMeter.{cpp,h}` — `sync(dtSec)` (the symmetric
  `tau = 0.1` one-pole at ~45–50), `displayGrLDb_/displayGrRDb_/`
  `displayCurrentGrDb_/displayMaxGrDb_`, `paint()` (how the L|R bars + readout
  draw), `resetPeakHolds()`, `mouseDown` reset.
- `Source/ui/MainView.cpp` — `syncMetersFromProcessor()` (the 30 Hz `dt`,
  `meterGr_.sync(dt)`), the Clip readout update (~736–746,
  `getCurrentClipDb()/getMaxClipSinceResetDb()`, `formatClipReadout`), and the
  clip LED (`clipLedLevel_`, drawn ~603–606).
- Confirm the meter sync is driven by a timer (find its rate; currently
  `dt = 1/30`).

Output the retrieval log.

────────────────────────────────────────
2. SHARED BALLISTICS HELPER
────────────────────────────────────────

New `Source/ui/meters/MeterBallistics.h` — a tiny, header-only,
UI-thread-only helper (no JUCE component, just math), reused by GR and Clip:

```cpp
#pragma once
#include <cmath>
#include <algorithm>

namespace ui {

/** Display ballistics for a meter value (dB or 0..1).
    Instant attack (snap up), smoothed release, plus peak-hold with a
    timed hold then decay. UI-thread only. */
struct MeterBallistic
{
    float display = 0.0f;   // smoothed live level
    float peak    = 0.0f;   // peak-hold level
    float hold    = 0.0f;   // seconds remaining in hold

    void reset() noexcept { display = peak = hold = 0.0f; }

    // raw: latest snapshot (clamped >= 0). dt: seconds since last update.
    void update (float raw, float dt,
                 float releaseTau = 0.30f,   // ~300 ms fall
                 float holdSec    = 1.00f,   // peak-hold time
                 float peakTau    = 0.40f) noexcept
    {
        raw = std::max (0.0f, raw);

        // instant attack, smoothed release
        if (raw >= display) display = raw;
        else                display += (raw - display) * (1.0f - std::exp (-dt / releaseTau));

        // peak-hold: catch new peak, hold, then decay toward the live display
        if (display >= peak) { peak = display; hold = holdSec; }
        else if (hold > 0.0f) { hold -= dt; }
        else                  { peak += (display - peak) * (1.0f - std::exp (-dt / peakTau)); }
    }
};

} // namespace ui
```

(Match the project's namespace/style if it differs; keep it dependency-free.)

────────────────────────────────────────
3. GR METER (GainReductionMeter)
────────────────────────────────────────

- Replace the symmetric `tau = 0.1` block in `sync(dtSec)` with **two**
  `ui::MeterBallistic` members (one per channel: `ballL_`, `ballR_`), updated
  from `rawL`/`rawR` with the defaults above (instant attack, 300 ms release,
  1 s hold, 0.4 s peak decay).
- `displayGrLDb_ = ballL_.display; displayGrRDb_ = ballR_.display;`
  `displayCurrentGrDb_ = max(...)` as today.
- `displayMaxGrDb_` stays the **click-to-reset latched max** (unchanged — it is
  separate from peak-hold).
- In `paint()`, draw a **peak-hold marker per bar** at `ballL_.peak` /
  `ballR_.peak` (a thin line / small segment at that level on each L|R bar),
  styled subtly per the theme. The bar fill stays at `.display`.
- `resetPeakHolds()` and the readout `mouseDown` reset must also
  `ballL_.reset(); ballR_.reset();`.

────────────────────────────────────────
4. CLIP READOUT + LED (MainView)
────────────────────────────────────────

- Add a `ui::MeterBallistic clipBall_;` member. In `syncMetersFromProcessor()`,
  update it from `processor_.getCurrentClipDb()` with the same defaults, and
  display its `.display` value as the "current" clip number in
  `formatClipReadout(...)` instead of the raw per-block value (so the number
  stops jittering). The "max" stays the latched `getMaxClipSinceResetDb()`.
- **Clip LED:** drive `clipLedLevel_` from a peak-hold so the LED lights
  immediately when clipping occurs and holds ~1 s then fades (reuse the
  ballistic's hold/decay, or a small dedicated hold+decay on `clipLedLevel_`).
  It must not flicker per-block. Map it to a 0..1 alpha as today (line ~606).
- The clip readout reset (`resetMaxClip`, ~532/768) should also reset
  `clipBall_`.

────────────────────────────────────────
5. NON-GOALS
────────────────────────────────────────

- Do NOT touch the I/O level meters (`MeterGroupComponent` / `meterIn_` /
  `meterOut_`) this slice — out of scope per the interview.
- Do NOT change the LUFS loudness panel (already BS.1770 time-windowed).
- Do NOT change the 30 Hz sync rate (acceptable for these ballistics).
- No engine/param/DSP changes.

────────────────────────────────────────
6. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout -b slice-15-meter-ballistics
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings.
- UI-only change → no bench impact; a quick Slice 3 quick run may be done for
  sanity but is expected unchanged (the bench is headless).
- In a host: GR meter rises instantly on a transient, falls smoothly (~300 ms),
  peak-hold marker hangs ~1 s then eases down; per-channel marks track L vs R.
  Clip number stops jittering; clip LED lights on a clip and holds ~1 s then
  fades. Click-to-reset still zeroes the latched max + peak-holds.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. New `MeterBallistics.h`; diffs for GainReductionMeter and MainView.
3. Build summary (Debug + Release, warnings).
4. Note confirming the GR peak-hold + clip LED behaviour (offline reasoning if
   not host-observed; flag what needs avishali's eyes).
5. `git status --short` (on the slice branch, no commit).
6. Open questions (e.g. peak-hold marker styling, release time feel).

────────────────────────────────────────
8. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali eyeballs the meters in the host (attack/release feel, peak-hold,
clip LED). Tune `releaseTau` / `holdSec` if the feel is off (architect can
adjust the defaults). On approval → architect closes Slice 15 (commit + push +
PROGRESS/PLAN) and opens Slice 16 (UI/UX interaction). **Do not self-close.**
