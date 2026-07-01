# SLICE M1 — I/O meter estimator unification (number = bar = truth)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` (+ shared `mdsp_ui` only if a smoother must change there — prefer product-side). UI/metering only; no DSP/param/audio/latency change.
**Reference:** `docs/METERING_ACCURACY_AUDIT.md` findings **1, 2, 3, 4, 8**.

---

## Why
The number, the bar body, and the max line for the same quantity are three different smoothed estimators → they disagree during transients/decays, and the "SP" number holds a stale value ~1 s. Make each quantity have **one source of truth**, live where read, held only where labeled.

## Allowed files
```
Source/ui/meters/MeterGroupComponent.cpp / .h
Source/ui/meters/MeterComponent.cpp / .h        (only if the bar-max source needs re-routing)
Source/PluginProcessor.h                         (floor constant only, if unified there)
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md  docs/METERING_ACCURACY_AUDIT.md (tick M1)
PROMPTS/SLICE_M1_METER_ESTIMATOR_UNIFY_CLOSE.md  (new)
```
**Non-goals / STOP:** no change to the DSP measurements (peak/TP/RMS taps are correct); no scale-mode/bar-geometry change; don't touch GR (M2) or clip/LUFS (M4/M3).

## Changes (`MeterGroupComponent::sync`)
1. **SP number = live peak with a short honest hold (~150 ms), decaying to the true value.** Replace the `PeakNumericSmoother` used for the SP readout (0.85 s hold + 0.95 s-τ release) with a short peak-hold: **~150 ms flat hold then a fast release** (e.g. τ ≈ 0.12 s, or the shared `PeakHoldModel` with holdMs≈150, falloff so it reaches the live value within ~250 ms). The SP readout must **track down to the true current peak** within ~250 ms — never sit dB-high for a second.
2. **Bar peak and SP number from one value.** Feed the **same** value to the SP formatter and the bar peak (`displaySmooth.peakDb`), so they can never disagree. Pick the bar's estimator as the single source (the 60 dB/s display release rising instantly is a fine honest "current peak") and format the SP number from *that*, or route both from the raw block peak with the short hold in #1 — but they must be identical.
3. **Bar MAX line = MAX number.** The yellow max line currently comes from the provider's smoothed peak; draw it instead from `dbToNormForScale(processor_.getMax{In,Out}PeakLDb())` so the line and the "MAX" number are the same latched value (`MeterComponent` max-line source, `MeterRenderStateProvider` provider max is display-only).
4. **RMS number = direct measurement, single-smoothed.** The RMS readout must be `formatRmsReadout(rmsDbFromMeanSquare(msIn…))` (the 300 ms power average IS the smoothing) — **remove the second `rmsSmooth` hold stage** for the number. The RMS bar stays on `displaySmooth.rmsDb`; verify bar and number now agree at steady state and track together on decays.
5. **Unify the floor.** One constant `kMeterFloorDb = -120.0f` used for: atomic inits, `reset*Holds()`, all dB conversions, and the "-inf" display test (currently −100 in formatters, −120 in conversions, −200 in smoothers). A real −110 dBFS must render as −110, not "-inf".

## Build, verify, close
```bash
cmake --build build 2>&1 | tail -8 && auval -v aufx MaLm Melc 2>&1 | tail -5
./scripts/install_user.sh build 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–4; avishali auditions 5):**
1. Build clean; auval PASS; installed fresh; no param/latency change.
2. **Transient test:** −6 dB tick over a −20 dB bed → the **SP number** returns to ~−20 within ~250 ms (not stuck at −6 for ~1 s); the **bar peak and SP number read the same** at all times.
3. **MAX test:** a single transient → the bar's max line and the MAX number sit at the **same** dB and both hold until Reset peaks.
4. **RMS test:** on a level drop the RMS number and RMS bar track together and settle to the true 300 ms average (number no longer reads high for ~1 s); −110 dBFS reads "−110", not "-inf".
5. **Audition:** number, bar, and max agree; nothing reads stale-high; the meter is trustworthy to 0.1 dB.

**Close gate:** update `docs/SIGNAL_FLOW.md` §7, `docs/METERING_ACCURACY_AUDIT.md` (mark M1), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit (product; submodule bump only if a shared smoother was edited); archive CLOSE.

## Note for the architect
The clean end state: **"SP" = live peak (short hold), "MAX" = latched max, "TP" = latched true-peak max, "RMS" = 300 ms average** — four honest, distinct, correctly-labeled figures, each matching its bar element.
