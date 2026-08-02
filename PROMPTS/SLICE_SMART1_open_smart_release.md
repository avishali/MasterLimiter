# SLICE SMART-1 — let the Open engine use `ReleaseEngine::Smart` (axis 1)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (measured, below) · **Audition:** avishali
**Scope:** **plugin only. NO SDK CHANGE REQUIRED** — everything needed already exists in `mdsp_dsp`.
**Frame:** axis 1 of `docs/PROGRAM_DEPENDENT_ENGINE.md`.

---

## What already exists (verified in the SDK, 2026-08-02)

Almost all of axis 1 is built. This slice is wiring, not DSP.

**`LimiterEnvelope`** (`melechdsp-hq/shared/mdsp_dsp/{include,src}/.../dynamics/LimiterEnvelope.{h,cpp}`)
- `enum class ReleaseEngine { AdaptiveSigma, LookaheadFollower, Smart }` — **`Smart` is fully implemented.**
- Setters: `setSmartFastReleaseMs` · `setSmartSlowReleaseMs` · `setSmartSustainMs` · `setSmartLeak`.
- Defaults: fast **20 ms**, slow **300 ms**, sustain **120 ms**, `smartDepthScale_` **4.0**.

**The algorithm** (`LimiterEnvelope.cpp` ~L620-632) — and note how closely it already matches the design
we wrote today, arrived at independently in July:

```
winMin    = sliding-window MINIMUM of the envelope over the LOOKAHEAD   (monotonic deque)
depth     = clamp((1 - winMin) * smartDepthScale_, 0, 1)                 <- how much GR is happening
smartSig_ = asymmetric-smoothed(depth)
relAlpha  = smartFastAlpha_ + smartSig_ * (smartSlowAlpha_ - smartFastAlpha_)
target    = (1 - smartLeak_) * winMin + smartLeak_ * instant
```

- It **decides from the lookahead** (`winMin` over the lookahead window) — avishali's axis-2 principle,
  already applied to release.
- Its adaptation is **scaled by reduction depth** — an embryonic `f(GR_depth)` (§7.2), with the polarity
  avishali asked for: deeper reduction ⇒ blend toward the **slow** release ⇒ smoother, fewer artifacts.

**`SingleBandLimiter`** — already owns a `LimiterEnvelope` and already exposes
`setReleaseEngine (LimiterEnvelope::ReleaseEngine)`.

**`MultibandLimiter`** — already exposes `SingleBandLimiter& band (int index)`, documented as
*"Per-band limiter — configure threshold, release, engine, attack independently."*

⇒ **The Open engine can already be told to use Smart. Nothing in the SDK needs to change.**

## ⚠️ CORRECTED 2026-08-02 — Cursor's retrieval log caught an architecture error in this slice

**My original claim ("every band silently runs `LookaheadFollower`") was WRONG.** Verified on disk:

```
LimiterEnvelope.cpp:477   if (autoRelease_ && releaseEngine_ == ReleaseEngine::LookaheadFollower)
LimiterEnvelope.cpp:562   else if (autoRelease_ && releaseEngine_ == ReleaseEngine::Smart)
LimiterEnvelope.cpp:657   else if (autoRelease_)                       // AdaptiveSigma
```

**All three engines are gated behind `autoRelease_`.** The Open path calls
`limiter.setAutoRelease(false)` (`configureMbBandLimiter` ~L1291), so none of them run — the manual
fixed-`dev_mb_release_ms` path does, and the existing `setReleaseEngine(LookaheadFollower)` call is inert.

Consequence: a literal "just call `setReleaseEngine(Smart)`" change would pass the null gate and **fail the
benefit gate**, because Smart would still never execute. Cursor was right to stop.

## What is missing (this slice)

Selecting a release engine in the Open path requires **`setAutoRelease(true)` as well** — which is a second
behavioural change, not just an engine swap. That must not be smuggled into the comparison.

> ⚠️ **Retrieval log first.** Read and report: where `mbEngine_` is configured in `PluginProcessor.cpp`
> (the `dev_mb_*` block), the existing `dev_release_engine` / `dev_smart_*` wiring on the INLINE path,
> and `MultibandLimiter::band()` / `SingleBandLimiter::setReleaseEngine`. Confirm the Open path currently
> sets no release engine.

### Build — THREE-way, not two (this is the change from Cursor's proposal)

New DEV param **`dev_mb_release_engine`** — choice { **`Manual`**, `Lookahead`, `Smart` },
**default `Manual`**:

| value | wiring | role |
|---|---|---|
| **`Manual`** (default) | `setAutoRelease(false)` + `setReleaseMs(dev_mb_release_ms)` — exactly today | the null, and the baseline everything is measured against |
| `Lookahead` | `setAutoRelease(true)` + `setReleaseEngine(LookaheadFollower)` | **isolates the auto-vs-manual change** |
| `Smart` | `setAutoRelease(true)` + `setReleaseEngine(Smart)` + forward the four `dev_smart_*` | the engine under test |

**Why three and not two:** with only { Manual-as-"Lookahead", Smart }, any measured win for Smart
conflates *the Smart algorithm* with *switching from fixed to auto release* — two changes at once. The
middle option makes `Smart vs Lookahead` a clean single-variable comparison, and both are still
referenced to today's shipping behaviour. This is the same one-variable-at-a-time discipline that the
ceiling/latency/stereo-link work needed.

Apply to **every band** and to the safety limiter when enabled:
```
for (int b = 0; b < mbEngine_.getNumBands(); ++b)  configure (mbEngine_.band (b));
if (safetyEnabled)                                  configure (mbEngine_.safety());
```
Forward the existing `dev_smart_fast_ms` / `_slow_ms` / `_sustain_ms` / `_leak` — **reuse the existing
params, do not add new ones.** Also add `dev_mb_release_engine` to `updateMbEngineRuntimeConfig`'s watch
list (it currently watches only crossover / attack / release_ms / safety / ceiling).

DEV UI: combo next to `dev_mb_release_ms`; show the shared Smart knobs when Open + Smart is selected;
grey `dev_mb_release_ms` when the selection is not `Manual`.

## Gate (Claude measures; `tools/analysis/` )
- [ ] **Default is a null.** With `dev_mb_release_engine = Manual`, output is bit-identical to
      pre-slice HEAD (residual ≤ −140 dB). This slice must not move the measured Open voicing.
- [ ] `mbl_calibrate.py` — **A–N and Z all PASS** (currently 54/55; the one FAIL is the known Open-vs-inline
      IMD difference). **sPk ≤ −1.00 and latency 3003 unchanged** in both settings.
- [ ] **Measured benefit — report all THREE settings** on the 4-source corpus at matched GR:
      `|MACRO| + |PUMP| + |ROUGH|` for `Manual` / `Lookahead` / `Smart`. Inline-path reference measured
      2026-08-02 (both with auto release ON, so it is a valid engine-vs-engine comparison):
      Smart beat Lookahead 2.90 vs 3.84 (live-show) and 2.97 vs 5.20 (homework).
      **If Smart does not improve on `Lookahead` in the Open path, say so and stop** — do not tune it into
      looking better; that is the architect's call with avishali.
- [ ] Build clean, AU + VST3, **both installed**, mtimes for both.

## Non-goals
- **No SDK edits.** Everything needed is already public. (SDK is shared with DeNoiser/Quell — keep the
  hold: additive only, and not needed here at all.)
- No change to the inline/Transparent path, to Ceiling/Drive, or to `dev_mb_attack_ms`
  (**inert in `Ramp` mode by design** — see `PROGRAM_DEPENDENT_ENGINE.md` §8).
- Not axis 2 or 3. The unified lookahead analysis block (§4) comes later; this slice only routes the
  existing Smart release into Open.

## Output requirements
1. Retrieval log (incl. confirmation that Open sets no release engine today). 2. Diffs.
3. Null proof at the default setting. 4. Full `mbl_calibrate.py`. 5. Build + install mtimes for BOTH
formats. 6. Confirm no SDK edits. 7. Open questions.
