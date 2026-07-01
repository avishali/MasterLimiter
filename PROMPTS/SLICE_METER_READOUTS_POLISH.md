# SLICE — I/O meter readouts polish (labels, peak-max hold, ballistics) + GR panel frame

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` only. **UI-only** — no DSP/param/audio/latency change. No SDK unless a processor max-peak atomic is added (see §3).
**Companion:** `docs/SIGNAL_FLOW.md` §7 (metering).

---

## Context — the 4 readout rows (per I/O meter)
Fed by `MeterGroupComponent::sync()` ([:303–349](Source/ui/meters/MeterGroupComponent.cpp:303)), drawn by `MeterComponent` ([:597–636](Source/ui/meters/MeterComponent.cpp:597)):
| Row | Source | Behavior now |
|---|---|---|
| **TP** | `processor.getMax{In,Out}TruePeak*Db()` (processor-latched max) | **holds** ✓, labeled "TP" |
| **SP** (peak) | `peakSmooth*.duty` (smoothed current sample peak) | decays; **unlabeled** |
| **MAX** (peak-max) | `maxPeakLDb_` (UI `jmax`) | **should** hold but reads as dropping; **unlabeled** |
| **RMS** | `rmsSmooth*.duty` (smoothed RMS) | decays; **unlabeled** |

---

## Allowed files to touch
```
Source/ui/meters/MeterComponent.cpp / .h        # 4-row labels + spacing
Source/ui/meters/MeterGroupComponent.cpp / .h   # peak-max hold + readout ballistics
Source/ui/meters/GainReductionMeter.cpp         # panel frame/border to match I/O meters
Source/PluginProcessor.h (only if §3 routes peak-max through a latched atomic)
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_METER_READOUTS_POLISH_CLOSE.md    (new, at close)
```
**Non-goals / STOP:** no meter *bar* rendering change; no scale/range change; no DSP/param change; keep the 4-row layout (don't add/remove rows).

---

## 1. Label all 4 readouts (captions between the values)
Currently only TP has a caption ([MeterComponent.cpp:614–617](Source/ui/meters/MeterComponent.cpp:614) — `removeFromLeft(15)` for "TP", value right-aligned). Do the **same** for the other three so each row reads `LABEL … value`:
- Row order + captions: **TP**, **SP**, **MAX**, **RMS**.
- For each of SP / MAX / RMS rows: `removeFromLeft(~18)` for the caption (small bold `textMuted`, left-aligned), draw the value **right-aligned** in the remainder — matching the TP row exactly, so the labels sit in the gutter to the left of each number and the numbers align in a column.
- Keep the existing per-row colours (peak, warning for max, text for rms). Captions in a muted tone.
- Verify the 4 rows + captions fit `numericArea_` at the readout font (bump `numericArea_` height a few px in `MeterComponent::resized()` if tight — it's a fixed sub-rect; do not overflow the meter).

## 2. GR panel frame/border to match the I/O meters
The GR meter panel frame ([GainReductionMeter.cpp:139–142](Source/ui/meters/GainReductionMeter.cpp:139)) uses `theme.background.darker(0.18f)` fill + `theme.grid` stroke, which reads differently from the I/O meter panels. Match the I/O panel look: use the **same panel fill + border colour, corner radius, and stroke weight** the meter/IO panels use (pull the exact `theme.*` + `m.r*`/`strokeThin` values from the I/O meter panel drawing so they're visually identical). The bars/content are the right size — only the enclosing **frame + border** changes so the GR panel sits in the row consistently with In/Out.

## 3. Fix peak-max hold (MAX row drops; should latch like TP)
`maxPeakLDb_ = jmax(maxPeakLDb_, lPeak)` ([MeterGroupComponent.cpp:315](Source/ui/meters/MeterGroupComponent.cpp:315)) is monotonic and only cleared in `resetPeakHolds()` — so it *should* hold. It's reading as dropping, so **diagnose then fix**:
- Confirm the MAX row actually renders `maxPeakLDb_` (via `numericOverrideMax_`), not the smoothed `peakSmooth.duty`.
- Confirm `maxPeakLDb_` isn't being reset anywhere per-block/per-sync besides `resetPeakHolds()`.
- Confirm `lPeak` (`getInput/OutputPeakLDb()`) is a genuine per-block peak, not an already-decayed value that makes `jmax` stall.
- **Robust fix (preferred):** make peak-max hold **exactly like TP** — route it through a **processor-latched max atomic**. The processor already latches max true-peak; add matching `maxInputPeak{L,R}Db_` / `maxOutputPeak{L,R}Db_` (updated in the input/output metering taps, `getMax…PeakDb()` getters, cleared by the existing peak-reset), and have the readout + `resetPeakHolds()` use those. This removes any UI-thread timing gap and guarantees it holds until Reset, identical to TP.

The success criterion: after a loud transient, the **MAX** number **stays put** until **Reset peaks**, exactly like **TP**.

## 4. Ballistics — make current readouts readable (not too fast)
The **SP** and **RMS** numbers (`peakSmooth*.duty` / `rmsSmooth*.duty`) update too fast to read. In `MeterGroupComponent::sync()` where the smoothers are configured/ticked ([:308–311](Source/ui/meters/MeterGroupComponent.cpp:308)), lengthen the **hold** and slow the **release** of the numeric readouts so a value is legible for **~700 ms–1 s** before it falls, with a gentle decay (not a jump). This is the *numeric readout* smoothing only — do **not** change the meter-bar ballistics. TP and MAX are held (no change).

---

## 5. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
./scripts/install_user.sh build 2>&1 | tail -3   # install so audition build is fresh
```
**Acceptance (Claude verifies 1–4; avishali auditions 5):**
1. Each I/O meter shows **TP / SP / MAX / RMS** with a caption next to each value; numbers align in a column; nothing overflows the readout box at any window size.
2. **MAX holds** after a transient until Reset peaks (matches TP); SP/RMS still track live.
3. GR panel frame + border are visually identical to the In/Out meter panels.
4. SP/RMS numbers are **readable** (~700 ms–1 s hold), no fast flicker; no DSP/param/latency change; auval clean; installed fresh.
5. **Audition:** readouts are clear and labeled; MAX and TP both hold; GR panel matches the meter row.

**Close gate:** update `docs/SIGNAL_FLOW.md` §7, `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit plugin-only (submodule bump only if a processor atomic was added — that's product `Source/`, so no SDK); archive CLOSE prompt.

---

## Notes for the architect
- #3's processor-latched route is the durable fix and mirrors the existing TP max-hold exactly; the UI-jmax path is the thing that's misbehaving. Prefer parity with TP.
- Labels (#1) also make #3 self-evident at audition (you can see which row holds).
