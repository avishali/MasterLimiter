# SLICE SMART-2 — attack decided from the lookahead (axis 2)

**Status:** spec — **SDK change required, read §Scope before starting** · **Architect:** Claude
**Verify:** Claude (`mbl_calibrate.py` + `mbl_frontier2.py`) · **Audition:** avishali
**Frame:** axis 2 of `docs/PROGRAM_DEPENDENT_ENGINE.md` §4. Follows SMART-1 (axis 1, landed).

---

## Scope warning — this one DOES touch the SDK

SMART-0 and SMART-1 were plugin-only. This is not: the attack ramp lives inside
`mdsp_dsp::LimiterEnvelope`. The SDK is shared with **DeNoiser / Quell / CrowdSep**, so:

- **Additive only.** New `AttackMode` enum value + new setters. Do not change the behaviour of
  `Ramp`, `Real` or `Hybrid` for any existing caller.
- **Null-test the existing modes** as part of the gate — an SDK regression here breaks other products.
- Keep the plugin-side default on the existing mode so this lands inert.

## The idea (avishali, 2026-08-02)

> *"attack should also be program dependent, using the lookahead to determine how fast to react."*

Today's `Ramp` mode already uses the lookahead but with a **fixed** shape — which is exactly why
`dev_mb_attack_ms` measures bit-identical from 0.5 ms to 25 ms in Ramp (`PROGRAM_DEPENDENT_ENGINE.md` §8).
There is no decision being taken; there is only a constant pre-ramp. This slice turns that shape into a
decision driven by what is actually in the lookahead buffer.

**The lookahead time is a budget.** The ramp must finish before the peak lands — that is what makes a
lookahead limiter distortion-free rather than a fast follower. The per-event question is how much of the
budget to spend:

| lookahead contains | ramp | why |
|---|---|---|
| isolated sharp transient, HF-dominated | **short** | the ear tolerates fast gain change on HF; a long ramp needlessly ducks material *before* the hit |
| large overshoot on LF content | **long** | fast gain change on a low-frequency waveform IS waveform distortion (measured — `SPECTRAL_ENGINE_DESIGN.md`, the wideband-attack distortion law) |
| sustained passage, small overshoot | **long / gentle** | nothing transient to catch; a fast attack only adds movement |
| dense transient train | **short, then hold** | re-ramping per hit is itself a modulation source |

## Reuse — most of the machinery exists

`ReleaseEngine::Smart` already computes, per sample, over the lookahead window
(`LimiterEnvelope.cpp` ~L620-632):

```
winMin    = sliding-window MINIMUM of the envelope over the lookahead   (monotonic deque, already there)
depth     = clamp((1 - winMin) * smartDepthScale_, 0, 1)
smartSig_ = asymmetric-smoothed(depth)
```

**Reuse `winMin` and `smartSig_` — do not build a second detector.** §4 of the design doc requires ONE
analysis of the lookahead driving all three axes, so the axes cannot act on different views of the signal.
What axis 2 adds is *time-to-peak* and an *LF-share* estimate; everything else is already computed.

> ⚠️ **Retrieval log first.** Read and report: the `Ramp` pre-ramp construction in `LimiterEnvelope.cpp`
> (where the ramp length comes from and how `AttackMode::Ramp` differs from `Real`/`Hybrid`), the
> `laMinDeque_` / `laMinOut_` sliding-minimum, `smartSig_`, and how `lookaheadSamples` reaches the
> envelope. **State whether time-to-peak is already available or must be derived.**

## Build

1. **SDK, additive:** `AttackMode::Adaptive` (append to the enum — do not reorder, other products may
   persist the index). Setters for the two voicing bounds: `setAdaptiveAttackMinMs`, `setAdaptiveAttackMaxMs`.
2. **The law** (start simple; this is a first cut to measure, not a final design):
   ```
   rampLen = lerp(minMs, maxMs, w)      w = f(LF share of upcoming block, overshoot depth)
   clamp rampLen <= time-to-peak        // the budget constraint -- never miss the peak
   ```
   LF share can start as a cheap one-pole split of the detector signal; do not add an FFT.
3. **Plugin:** new DEV param `dev_mb_attack_adaptive` (bool, **default OFF**) + two range params reusing
   `dev_mb_attack_ms` as the max bound where sensible. Applies to every band + safety, on the
   `updateMbEngineRuntimeConfig` watch list.

## Gate
- [ ] **Default OFF is a null** vs pre-slice HEAD (≤ −140 dB), both engines.
- [ ] **SDK regression null:** `AttackMode::Ramp` / `Real` / `Hybrid` bit-identical to pre-slice for the
      same inputs. This is the shared-SDK protection and is not optional.
- [ ] **Peak safety unchanged:** sPk ≤ −1.00 and latency 3003 in every combination. The ramp must never
      exceed the budget — a ramp that outruns time-to-peak means a missed peak, which is an automatic fail.
- [ ] `mbl_calibrate.py` A–N and Z all PASS.
- [ ] **Measured benefit:** `mbl_frontier2.py`-style score (`|MACRO|+|PUMP|+|ROUGH|`) on the 4-source
      corpus, adaptive-attack ON vs OFF, with `dev_mb_release_engine = Smart` fixed so this is a
      single-variable comparison. **If it does not improve, say so and stop** — do not tune it into looking
      better. That is the architect's call with avishali.
- [ ] Build clean, AU + VST3, **both installed**, mtimes for both.

## Non-goals
- Not axis 3 (adaptive depth) — that is SMART-3 and is Python-prototyped first.
- Do not change `Ramp`'s behaviour, or the inline/Transparent path, or Ceiling/Drive.
- Do not raise `dev_mb_lookahead_ms` in this slice. A bigger budget is a real lever but reported latency is
  fixed at the maximum across all configs (CLIP-1.1), so raising it raises latency for *every* user —
  that is a product decision for avishali, not a side effect of this slice.

## Output requirements
1. Retrieval log (incl. whether time-to-peak already exists). 2. Diffs, SDK and plugin separately.
3. Null proofs: plugin default-off, AND the three existing AttackModes. 4. Full `mbl_calibrate.py`.
5. Measured ON-vs-OFF table. 6. Build + install mtimes for BOTH formats. 7. Open questions.
