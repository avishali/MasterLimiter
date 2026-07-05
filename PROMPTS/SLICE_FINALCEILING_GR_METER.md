# SLICE — FinalCeiling GR meter (main UI)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition:** avishali
**Repos:** plugin `MasterLimiter` only. **UI-only** — the data already exists. No DSP, no params, no SDK.

> ⚠️ **Retrieval log first.** Confirm the getters + where the existing GR meter / output meters / clipper readout live in `MainView.{cpp,h}` and `Source/ui/meters/`. Output actual lines.

---

## Why
FinalCeiling (TruePeak) runs **all the time** now and does real work (it catches leaked peaks; we measured it pumping — flattening 8.2→6.7 dB). avishali needs to **see how hard it's working**, especially while voicing the Smart engine's leakage. Today its GR is only a muted DEV text readout (`lblFinalCeilingReadout_`); promote it to a visible **GR meter** in the main UI.

**Data already plumbed** ([PluginProcessor.h:111-112](Source/PluginProcessor.h:111)): `getCurrentFinalCeilingDb()` + `getMaxFinalCeilingDb()`, fed from `finalCeiling_.getLastBlockMaxReductionDb()` ([PluginProcessor.cpp:1989-1997](Source/PluginProcessor.cpp:1989)). Nothing to add in the processor.

---

## What to add
A small **FinalCeiling GR meter** in the main UI showing **current + max-hold** reduction:
- Read `getCurrentFinalCeilingDb()` (current) and `getMaxFinalCeilingDb()` (peak-hold) at the existing meter sync rate (~30 Hz).
- **Range 0–6 dB** (FinalCeiling GR is small when the limiter holds, larger when it pumps — 6 dB headroom is plenty; make the scale legible in that range).
- A thin vertical bar (or a compact horizontal bar) + numeric current/max readout, styled like the existing GR meter. Label it **"FC"** or **"Final"**.
- **Reset** the max-hold with the existing reset-peaks button (it already calls the processor's max resets — confirm `getMaxFinalCeilingDb` is zeroed on that path via `resetMaxDevClampReadouts()`/whatever reset already zeroes `maxFinalCeilingDb_`; wire it if not).

**Placement:** near the main GR meter / output meters (avishali/Cursor pick — a slim bar beside `meterGr_`, or under the output meters). Keep it small; it's a "how hard is the safety net working" indicator, not a primary meter.

---

## Non-goals
No DSP/param/SDK. Don't change the DEV `lblFinalCeilingReadout_` (leave it) or the main GR meter. No window resize unless genuinely needed for the small addition.

## Build, verify, close
```bash
cmake --build build --config Release 2>&1 | tail -6
bash scripts/install_user.sh build      # ⚠️ pass 'build'
auval -v aufx MaLm Melc 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–2; avishali auditions 3):**
1. Build clean, AU validates; UI-only diff (MainView + meters). Install from `build`, verify via VST3.
2. Meter reads plausibly: near-0 when the main limiter holds the ceiling (FinalCeiling idle); rises when you push into it / with Smart leakage. Max-hold latches; reset-peaks clears it.
3. **Audition (avishali):** the FC meter makes the pumping visible — you can watch it move as you sweep Smart's Leak.

**Close gate:** update `docs/SIGNAL_FLOW.md` §metering + `docs/PROGRESS.md` + `PROMPTS/PLAN.md`; commit plugin-only; push. Archive CLOSE.

## Notes for the architect
- Pairs with the Smart-release voicing: watching FC GR while turning Leak up is how avishali finds the point where leakage starts overworking FinalCeiling.
- Near-zero risk (UI reading existing atomics). Only judgment is placement + scale.
