# SLICE AB-1.1 — A/B Match is an unbounded integrator; default it OFF and fix the control law

**Status:** ready for Cursor · **PRIORITY: highest** · **Architect:** Claude · **Reported by:** avishali (heard it)
**Scope:** plugin only. No SDK. No engine/voicing change.

## Report
> 1. *"A/B match is revealing the names instead of the other way around."*
> 2. *"levels are moving in a strange way its confusing."*
> 3. *"the overall clipper, ceiling parts are behaving unpredictably."*

**Issues 2 and 3 have a single root cause.** The plugin was measured clean at the CLIP-2 build (58/59,
ceiling −1.00 at all rates, latency exact); AB-1 shipped `ab_match` **ON by default** and regressed it.

## STEP 0 — de-risk first: default `ab_match` to **OFF**
One value, immediately. `ab_match` ON is now known to misbehave, and it is on by default in a build
avishali is auditioning. With it OFF the plugin returns to the verified CLIP-2 behaviour (Cursor's own
null test: −400 dB residual). Do this even if the rest of the slice takes longer.

## BUG A (issues 2 + 3) — the trim is a pure integrator with no leak

`updateAbMatchTrimDb`, `PluginProcessor.cpp` ~L1310:
```cpp
const float error = abTargetLufs_ - liveLufs;
abTrimDbSmoothed_ += compGainSmoothCoef_ * error;      // <-- accumulates the ERROR
```

Compare the existing, correct one-pole in `updateCompensationGainDb`:
```cpp
target = jlimit (-12.0f, 12.0f, ref - liveLufs);
compGainDbSmoothed_ += compGainSmoothCoef_ * (target - compGainDbSmoothed_);   // smooths TOWARD a target
```

The AB version never references its own current value, so it **integrates**: any persistent error ramps
the trim until it hits the ±6 dB clamp, and it can only stop when the error is exactly zero. Worse, the
comment says `liveLufs` is *"post-trim ST from the previous block (closed loop on matched output)"* — a
pure integrator inside a laggy closed loop, which oscillates or runs away.

**This predicts exactly what avishali heard**, and it matches my offline measurement: with the switch edge
firing, Transparent→Open offsets became −0.04 / **+6.20** / +0.66 / **−1.77** dB (≈ the +6 clamp), where
the same build with `ab_match` OFF gives the correct +0.11 / −0.11 / +0.97 / +0.07.

It also explains issue 3: the trim is applied **after** the peak stage with a sample-peak re-clamp, so as
the trim ramps, the re-clamp engages by a continuously changing amount — Ceiling and Drive appear to
behave unpredictably because the thing after them is moving.

**Fix:** make it a smoother toward a target, not an accumulator:
```cpp
const float targetTrim = juce::jlimit (-kAbMatchTrimClampDb, kAbMatchTrimClampDb,
                                       abTargetLufs_ - liveLufs);
abTrimDbSmoothed_ += compGainSmoothCoef_ * (targetTrim - abTrimDbSmoothed_);
```
⚠️ **And reconsider the closed loop.** If `liveLufs` is measured post-trim, the target must be computed
from the **pre-trim** loudness, or the loop chases its own output. State which you chose and why.

## BUG B (issue 1) — labels are not refreshed when `ab_match` changes

`isBlindEngineLabels()` (~L786) and the relabel block (~L833: `blind ? addItem("A") : addItem("Transparent")`)
both read correct. But `abReveal_` is only assigned inside `btnAbReveal_.onClick` (~L148), and the combo is
constructed with the real names (~L138).

**Prime suspect: the relabel is never invoked from the `ab_match` parameter listener** — only from the
Reveal button's click. So on load with match ON the combo still shows the names it was built with, and the
first thing that changes them is clicking Reveal — which is precisely the inverted behaviour reported.
Verify this before fixing; if it is something else, say so.

## Gate
- [ ] **`ab_match` defaults OFF**; fresh instance confirms.
- [ ] **`ab_match` OFF is a null** vs the CLIP-2 build (≤ −140 dB).
- [ ] **Trim settles, does not ramp.** Drive a constant-level signal with match ON and a latched target;
      `abTrimDbSmoothed_` must converge and stay put, never walk to the ±6 dB clamp. Report the settled
      value over time, not a single sample.
- [ ] **Loudness match works with match ON:** |Open+Smart − Transparent| ≤ 0.15 dB **LUFS** across all four
      corpus sources (gate restated in LUFS — ST-LUFS is the control variable, RMS was the wrong unit).
- [ ] **Ceiling unaffected:** sPk ≤ −1.00 with match ON and OFF, both engines, and Drive+Ceiling=Clip still
      passes the group-Z discontinuity check (the CLIP-2 regression test).
- [ ] **Labels:** match ON + Reveal OFF ⇒ selector reads **A / B**. Reveal ON ⇒ real names. Verified on a
      freshly loaded instance, not only after clicking something.
- [ ] `mbl_calibrate.py` 58/59; latency 3003; `mbl_frontier2.py` Open+Smart mean **3.956** with match OFF.
- [ ] Build clean, AU + VST3, both installed, mtimes.

## Non-goals
- Do not remove AB-1. The feature is needed for the tester round; it is the control law that is wrong.
- No engine/voicing change, no SDK edits.

## Note
Offline verification of this feature is structurally weak — pedalboard re-prepares between renders, so it
cannot faithfully test a live mid-stream engine flip. **The close gate needs avishali in a DAW**, watching
the M/S LUFS readout while flipping A↔B in both directions.
