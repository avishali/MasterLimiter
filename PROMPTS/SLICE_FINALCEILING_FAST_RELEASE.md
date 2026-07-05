# SLICE — FinalCeiling fast release (stop the macro-flatten/pump) — PRIORITY

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (rig, 300ms range) · **Audition:** avishali → Asaf
**Repos:** SDK `melechdsp-hq` (FinalCeilingLimiter) + plugin `MasterLimiter`.
**Why this is #1:** measured — FinalCeiling **flattens the 300 ms macro-range by 3.6 dB** (EDM 6.1→2.5) and masks everything the main limiter does. Its internal `LimiterEnvelope` has a **hardcoded 100 ms manual release** ([`FinalCeilingLimiter.cpp:32-34`](../../melechdsp-hq/shared/mdsp_dsp/src/dynamics/FinalCeilingLimiter.cpp:32)) — far too slow for a true-peak catcher, so it holds the reduction ~100 ms after each transient and ducks the following audio = the pump/flatten = the Ozone macro-gap. **Goal metric:** 300 ms range on jazz/EDM recovers toward Ozone (4.7 / 5.1) at matched loudness, true-peak still ≤ ceiling, no added distortion.

> ⚠️ **Retrieval log first.** Read `FinalCeilingLimiter.{h,cpp}` (the `prepare()` envelope setup ~26-34, `process()` ~85-93) and the plugin's `finalCeiling_` setup + `dev_final_ceiling` wiring. Output actual lines.

---

## SDK — `melechdsp-hq` FinalCeilingLimiter (touch only this pair; leave quell/StftEngine WIP)
1. **Make the release configurable** instead of hardcoded 100 ms:
   - Add `void setReleaseMs (float ms) noexcept;` (and optionally `setReleaseSustainRatio(float)`) that forward to the internal `envelope_`. Store + re-apply on `prepare()`.
   - **Change the default** in `prepare()` from `setReleaseMs(100.0f)` → **`setReleaseMs(5.0f)`** (fast true-peak catch) and reduce `setReleaseSustainRatio(4.0f)` → **`1.0f`** (no slow tail). These defaults are the starting point; the plugin exposes a knob to tune.
2. Keep everything else (mode Aggressive, lookahead, TruePeak OS) unchanged. This is release-only.

> A fast release on a true-peak catcher only ducks for the brief transient → minimal distortion (the reduction is short and small when the main limiter holds). This is standard TP-limiter behaviour; 100 ms was the bug.

---

## Plugin — `MasterLimiter`
1. **DEV param** `dev_final_ceiling_release_ms` — Float `1…100` ms, default **5**, "DEV FC Release". Cache pointer + jassert (mirror `dev_la_release`).
2. **Wire it:** where `finalCeiling_.setCeilingLinear/setMode` are set each block (~PluginProcessor.cpp:1315), also `finalCeiling_.setReleaseMs(devFinalCeilingReleaseMs_->load())`.
3. **DEV UI:** one slider "FC Release (ms)" near the FinalCeiling controls / RELEASE groups. (Pairs with the FinalCeiling GR meter slice — watch FC GR + tune its release together.)

---

## Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build --config Release 2>&1 | tail -6
bash scripts/install_user.sh build      # ⚠️ pass 'build'
auval -v aufx MaLm Melc 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–3 + measures 4; avishali auditions 5):**
1. Build clean (SDK+plugin), AU validates, **no latency change** (release-only; FC lookahead/latency unchanged).
2. Other products' FinalCeiling default behaviour: note this **changes the shipped FC release** (100→5 ms) — intentional. Confirm the only SDK change is FinalCeilingLimiter release.
3. Installed via `build`, verified via VST3; `dev_final_ceiling_release_ms` present.
4. **(Claude runs the rig)** sweep `dev_final_ceiling_release_ms` [100, 30, 10, 5, 2] on jazz+EDM (FC ON, TruePeak, matched loudness): does **300 ms range climb toward 4.7/5.1** as release drops, while **TP stays ≤ ceiling** and LF THD doesn't rise? Report range/RMS/TP/THD per release. Find the knee.
5. **Audition (avishali):** FC stops pumping; the mix opens up / breathes; watch the FC GR meter settle.

**Close gate:** update `docs/SIGNAL_FLOW.md` §2.14 + `docs/INTELLIGENT_RELEASE_DESIGN.md` (this reframes the fix: FinalCeiling was the dominant flattener) + `docs/PROGRESS.md` + `PROMPTS/PLAN.md`; commit SDK + plugin separately; push. Archive CLOSE.

## Output requirements
1. Retrieval log. 2. SDK diff (FinalCeiling release config). 3. Plugin diff (param + wiring + UI). 4. Build+auval. 5. Latency before/after. 6. Both commit hashes. 7. Open questions.

## Notes for the architect (not for Cursor)
- ⚠️ Cross-product: FinalCeilingLimiter is shared. Changing the *default* affects any product that uses it on submodule bump. If risky, keep the SDK default 100 and set 5 ms from the plugin only (via the new setter) — decide from the retrieval log which products use FinalCeilingLimiter. **Prefer plugin-sets-5ms, SDK-keeps-default, if any other product relies on it.**
- Expect the rig to show the range jumping with a fast FC release — this is likely THE Ozone macro-fix. Smart engine stays paused/parked until this lands and we re-measure whether the main limiter needs anything more.
