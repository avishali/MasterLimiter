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

## Changes (`GainReductionMeter.cpp`) — build on the current WIP
> The working tree already has: `kMaxGrDb = 12`, `grDbToY`, `kGrScaleMarksDb {1,3,6,12}`, `makeTopDownFill` returning empty (dead-zone threshold already gone), and `tickGrReadoutSmoother`. **Keep all of that.** M2 adds the low-end expansion on top.

1. **Keep full-scale = 12 dB (avishali: "extend to −12"), but expand the low end with a sqrt mapping.** Change `normaliseGr` from linear to sqrt:
   ```cpp
   float normaliseGr (float grDb) noexcept
   {
       const float x = juce::jlimit (0.0f, 1.0f, grDb / kMaxGrDb);   // kMaxGrDb = 12
       return std::sqrt (x);   // dedicate the bottom half of the bar to 0..3 dB
   }
   ```
   Result at `kMaxGrDb=12`: 0.5 dB → 20%, 1 dB → 29%, 2 dB → 41%, **3 dB → 50%**, 6 dB → 71%, 12 dB → full. The 0–3 dB mastering range now owns the bottom half of the bar while the scale still reaches −12. (Fill + peak-hold tick both route through `normaliseGr`/`grDbToY`, so they stay consistent automatically.)
2. **Scale ticks:** extend `kGrScaleMarksDb` to include the low end: **`{0.5, 1, 2, 3, 6, 12}`** dB (they map through `normaliseGr`, landing at the sqrt positions; the low marks are readable because the sqrt expands that region). Keep the 12 top mark.
3. **Fill rendering:** the dead-zone threshold is already removed (`makeTopDownFill` returns empty ≤ 0). If small slivers still get eaten by the corner radius, use `fillRect` for the *fill* body so a thin line still renders.
4. **Release for readability:** `makeGrBallisticsConfig` release `50 → ~180 ms` (keep attack 1 ms, hold 1000 ms) so small transient GR lingers long enough to read.
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
