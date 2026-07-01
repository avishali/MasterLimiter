# SLICE — Fix the FinalCeiling GR readout (use real gain, not pre/post buffer diff)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` ONLY. **No SDK change** (the accessor already exists). No submodule bump.
**Companion:** `docs/SIGNAL_FLOW.md` §2.16/§4.4. Follows `SLICE_CLAMP_DEV_TOGGLES`.

---

## Why
The `dev_final_ceiling` readout is **wrong**. It compares `bufferAbsMax(buffer)` **before** vs **after** `finalCeiling_.process()` — but the FinalCeiling delays audio by ~125 samples (its lookahead), so the two peaks are **different, latency-offset slices** of the signal. On random material the max latches the worst-case window mismatch.

**Proven bogus:** at **0 dB drive · Color 0 · SP · pink noise −6 dBFS** — signal 6 dB *below* threshold, limiter doing nothing, output unclamped at ~−6 dBFS — the readout still showed **6.7 dB max**. Impossible if real. The earlier "9 dB hidden clamp" was this artifact, not real clamping.

**The SDK already computes the truth:** `mdsp_dsp::FinalCeilingLimiter::getLastBlockMaxReductionDb()` returns `envelope_.getLastBlockMaxGrDb()` — the FinalCeiling's **actual** applied reduction. Just read it.

> Note: the **M/S Safety Clamp** readout is already correct (it tracks the real `msSafetyGain` min over the block) — **leave it untouched.**

---

## Allowed files to touch
```
Source/PluginProcessor.cpp
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_FINALCEILING_METER_FIX_CLOSE.md   (new, at close)
```
**Non-goals / STOP:** no audio-path change; no toggle-behavior change; no SDK edit; do not touch the M/S clamp metering.

---

## The change (`PluginProcessor.cpp`, the FinalCeiling block ~[:1731–1741](Source/PluginProcessor.cpp:1731))
Replace the pre/post `bufferAbsMax` metering with the SDK accessor:
```cpp
limiterOversampler_.processSamplesDown (block);

float fcGrDb = 0.0f;
if (devFinalCeiling_ == nullptr || devFinalCeiling_->load (std::memory_order_relaxed) >= 0.5f)
{
    finalCeiling_.process (buffer);
    fcGrDb = finalCeiling_.getLastBlockMaxReductionDb();   // real applied reduction
}
// toggle OFF → stage bypassed → 0 (stale internal value not read)
currentFinalCeilingDb_.store (fcGrDb, std::memory_order_relaxed);
if (fcGrDb > maxFinalCeilingDb_.load (std::memory_order_relaxed))
    maxFinalCeilingDb_.store (fcGrDb, std::memory_order_relaxed);
```
Then **remove the now-unused `bufferAbsMax()` helper** (~[:28](Source/PluginProcessor.cpp:28)) — nothing else uses it (grep to confirm).

> RT-safe: one atomic read + one relaxed store; **removes** the two per-block full-buffer magnitude scans the old path added.

---

## Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
./scripts/install_user.sh build 2>&1 | tail -3   # install so audition build is fresh (not just built)
```
**Acceptance (Claude verifies 1–3; avishali auditions 4):**
1. Build clean; AU validates; installed to user folders (verify fresh mtime).
2. **The proof test:** 0 dB drive · Color 0 · SP · pink noise −6 dBFS → **Final Ceiling reads ~0.0 / ~0.0** (was 6.7). Toggling the stage on/off makes no audible difference here.
3. Push drive until the main GR meter shows real reduction → Final Ceiling now shows a **plausible, non-jittery** value that tracks actual peak catching; M/S clamp readout unchanged.
4. **Audition/re-diagnose:** with the honest meter, push **Color 100 at real drive** and read the true FinalCeiling load — this is what tells us whether band-headroom (or a stronger wideband catch) is actually needed. Report the number.

**Close gate:** update `docs/SIGNAL_FLOW.md` §4.4 (readout now = real GR via `getLastBlockMaxReductionDb`), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit plugin-only; archive CLOSE prompt.

---

## Notes for the architect (context, not for Cursor)
- This unblocks the real question: **is there any excess FinalCeiling clamping at Color 100 / heavy drive?** We only find out with an honest meter. My band-headroom hypothesis stays *unconfirmed* until then — do not implement it yet.
- Also folds in the install step to the gate (Cursor's "build clean" last time left the bundles uninstalled — see the missing-install incident).
