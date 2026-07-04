# SLICE — Per-band numeric GR readouts (LO / MID / HI)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali (Asaf on DEV build)
**Repos:** plugin `MasterLimiter` only. **Meter-UI only — no DSP, no params, no SDK, no window resize.**
**Companion:** `docs/SIGNAL_FLOW.md` §metering (§2.14 taps).

> ⚠️ **First — retrieval log.** Line numbers below are from a mapping pass and will drift. Read `Source/ui/meters/GainReductionMeter.{h,cpp}` and re-confirm before editing. Output the actual current lines.

---

## Why
The GR meter shows **one** number today — the **total** (deepest band × wideband) `cur / max` in the bottom readout strip ([GainReductionMeter.cpp:353–364](Source/ui/meters/GainReductionMeter.cpp:353)). The three per-band bars have no numbers, so during voicing you can't read *how much* each band is pulling. Add a compact **numeric GR readout per band** (LO/MID/HI), column-aligned under each band's bar. The data already exists — this is pure meter draw + one layout strip.

**All the taps already exist** ([PluginProcessor.h:96–107, 102–107](Source/PluginProcessor.h:96)): `getCurrentGrLow/Mid/High{L,R}Db()` and `getMaxGrLow/Mid/High{L,R}Db()`. `resetMaxGr()` already zeroes all per-band maxes. Nothing in the processor changes.

---

## Allowed files
```
Source/ui/meters/GainReductionMeter.h / .cpp
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_PERBAND_GR_READOUTS_CLOSE.md   (new, at close)
```
**Non-goals / STOP:** No DSP/param/SDK. No processor change (taps + reset already exist). **No window/component resize** (`meterGr_` stays 198×354 — there's ample vertical room). Don't touch the bars, ballistics, sub-bar M/S/L/R micro-labels, solo row, or the total readout. Not per-*channel* numbers (6 would be illegible at 198px) — **one band-max number per band**.

---

## Design

**Per band, show `cur / max` where the value is the band max of its two channels** (matches the history-trace + sub-bar "band max of L/R" convention and the deepest-reduction semantic):
```
curBand[b]  = max(getCurrentGr{Band}LDb(), getCurrentGr{Band}RDb())
maxBand[b]  = max(getMaxGr{Band}LDb(),     getMaxGr{Band}RDb())
```
for `{Band}` ∈ {Low, Mid, High}.

**Smoothing (readability):** the current value must use the **same peak-hold-then-release smoother** as the total readout — reuse `tickGrReadoutSmoother(...)` ([:55–86](Source/ui/meters/GainReductionMeter.cpp:55)) with **one independent state per band**, ticked in `sync(dtSec)` at the same `holdTicks`. Raw per-band current flickers otherwise. The **max** value is a latched peak — display it directly (no smoother).

**Formatting:** reuse `formatDbBare(...)`. One line per band, e.g. `3.2 / 6.1` (cur / max); if a column is too narrow for both, stack cur over a smaller max, or drop the max to a fainter sub-line — **cur is required, max is strongly preferred** (the getters + reset are free). Use the same muted styling as the total readout caption.

---

## Layout (fits current footprint — 354px tall)
In `resized()`, the current bottom-up order is total `removeFromBottom(18)` → solo `removeFromBottom(22)` → `meterBounds_` = rest ([:187–189](Source/ui/meters/GainReductionMeter.cpp:187)). Insert a **per-band readout strip directly under the bars, above the solo row**, so each number sits beneath its own bar:
```cpp
readoutBounds_ = bounds.removeFromBottom (18);      // total — unchanged, stays at very bottom
soloBounds_    = bounds.removeFromBottom (22);      // solo — unchanged
bandReadoutBounds_ = bounds.removeFromBottom (H);   // NEW (H ≈ 16–22; pick for legibility)
meterBounds_   = bounds.reduced (4, 4);             // bars — slightly shorter, fine at 354
```
Split `bandReadoutBounds_` into 3 columns using the **same `bandW`/`bandGap` math as the bars/solo** ([:239–240](Source/ui/meters/GainReductionMeter.cpp:239)) so numbers align under their bars. Draw each band's `cur / max` centered in its column.

---

## State + reset (`.h` + `.cpp`)
- Add per-band smoother state: `float bandReadoutHeld_[kNumBands]{}, bandReadoutDuty_[kNumBands]{}; int bandReadoutHoldTicks_[kNumBands]{};` and `juce::Rectangle<int> bandReadoutBounds_;`.
- `sync()`: tick each band's smoother from `curBand[b]`.
- `resetPeakHolds()`: also zero the 3 smoother states + duty + holdTicks (the processor's per-band max is already zeroed by the existing reset path — confirm `resetMaxGr()` is called on the same reset button and don't double-wire).

---

## Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
```
**Acceptance (Claude verifies 1–4; avishali auditions 5):**
1. Build clean, no new warnings; AU validates; **no latency/DSP change** (meter-only — the audio path is untouched by construction).
2. Each band shows a `cur / max` number, column-aligned under its bar; total readout unchanged.
3. Numbers read correctly: drive a band hard → its number rises and holds like the total; a quiet band reads ~0; **max latches** the deepest hit and clears on the reset-peaks button.
4. No layout regression — bars, solo row, sub-bar labels, total readout all intact within 198×354 (no clipping, no overlap); works in Stereo and M/S modes (band-max collapses L=R when linked).
5. **Audition:** per-band numbers legible and useful for voicing (which band is doing the work, how deep).

**Close gate:** update `docs/SIGNAL_FLOW.md` §metering + `docs/PROGRESS.md` + `PROMPTS/PLAN.md`; commit **plugin-only, do not push** (Quell hold); archive `SLICE_PERBAND_GR_READOUTS_CLOSE.md`.

## Output requirements
1. Retrieval log (actual lines). 2. Meter diff. 3. Build + auval. 4. `git status --short` + commit hash (no push). 5. Open questions (column-width legibility of cur/max; chosen strip height H).

## Notes for the architect (not for Cursor)
- Zero-risk slice: no processor edit (taps + `resetMaxGr()` already there), no resize. The only judgment is the H strip height + whether cur/max both fit per 198/3 ≈ 55px column.
- Band-max (not per-channel) keeps it to 3 legible numbers; the sub-bars already carry the L/R (or M/S) visual split.
