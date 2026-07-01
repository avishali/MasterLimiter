# SLICE M2 — GR meter mastering scale (make 0.1–3 dB reduction visible & trustworthy)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` only. UI-only — **no DSP/param/audio/latency change** (GR *values* are already accurate < 0.1 dB). No SDK.
**Reference:** `docs/METERING_ACCURACY_AUDIT.md` findings **5** and **9**.

---

## Why
The GR *numbers* are correct, but the **bar can't show them**: a **linear 0–12 dB** scale crushes the useful mastering range (0.1–3 dB) into the top ~25% of the bar as thin top-edge slivers, so nothing reads as reduction until ~2 dB. Compounded by a `> 0.5f` fill threshold (~0.02 dB dead zone) and a 50 ms release flicker. Fix the mapping so mastering GR is clearly visible and readable.

## Allowed files
```
Source/ui/meters/GainReductionMeter.cpp / .h
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md  docs/METERING_ACCURACY_AUDIT.md (tick M2)
PROMPTS/SLICE_M2_GR_MASTERING_SCALE_CLOSE.md  (new)
```
**Non-goals / STOP:** no processor/atomic/DSP change; no change to the GR *values*; no meter-footprint/layout change (that's done); don't touch the I/O meters.

## Changes (`GainReductionMeter.cpp`)
1. **Full-scale → 6 dB** with a **low-end-expanded (sqrt) mapping.** Replace `kMaxGrDb = 12.0f` with `6.0f`, and `normaliseGr`:
   ```cpp
   float normaliseGr (float grDb) noexcept
   {
       const float x = juce::jlimit (0.0f, 1.0f, grDb / kMaxGrDb);
       return std::sqrt (x);   // dedicate most of the bar to 0..3 dB
   }
   ```
   Result: 0.5 dB → 29% (~87px), 1 dB → 41%, 3 dB → 71%, 6 dB → full. The 0–3 dB region now owns ~70% of the bar. (Both the fill and the peak-hold tick route through `normaliseGr`/`grDbToY`, so they stay consistent automatically.)
2. **Scale ticks:** update `kGrScaleMarksDb` to a mastering set: **`{0.5, 1, 2, 3, 6}`** dB (they map through `normaliseGr`, so they land at the sqrt positions).
3. **Drop the dead zone:** the `if (fill.getHeight() > 0.5f)` guard → `> 0.0f` (or remove; `makeTopDownFill` already returns empty ≤ 0). Consider `fillRect` instead of `fillRoundedRectangle` for the *fill* so sub-corner-radius slivers render as a visible line rather than being eaten by the corner radius.
4. **Release for readability:** `makeGrBallisticsConfig` release `50 → ~180 ms` (keep attack 1 ms, hold 1000 ms) so small transient GR lingers long enough to read instead of flickering.
5. **Label the total readout:** the bottom numeric is **total** GR (deepest band × wideband) while the bars are **per-band** — add a small "total" caption (or tooltip) so it doesn't read as inconsistent with the bars.

## Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8 && auval -v aufx MaLm Melc 2>&1 | tail -5
./scripts/install_user.sh build 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–3; avishali auditions 4):**
1. Build clean; auval PASS; installed fresh. No param/latency change.
2. Pink noise pushed to **~0.5–1 dB GR** → the bars show a clearly visible, readable reduction (not a top-edge sliver); ticks at 0.5/1/2/3/6 land at the sqrt positions.
3. GR *values* unchanged (bottom readout matches prior); "total" label present.
4. **Audition:** small GR (the mastering range) reads clearly and honestly; no flicker; the meter feels trustworthy at 0.1 dB resolution.

**Close gate:** update `docs/SIGNAL_FLOW.md` §7, `docs/METERING_ACCURACY_AUDIT.md` (mark M2 done), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit plugin-only; archive CLOSE.
