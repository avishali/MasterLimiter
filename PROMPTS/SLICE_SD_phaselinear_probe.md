# SLICE S-D — phase-linear 2-band split probe (bench `--split lr|linphase`)

**Status:** ready for Cursor (bench only) · **Architect:** Claude · **Measure:** Claude (LR vs linphase) · **Audition:** avishali (phase/quality)
**Repo/scope:** SDK **`melechdsp-hq`** — extend the existing `mbl_bench` tool only. Additive; **no module edits, no plugin change.**
**Why:** §4a proved **2-band** peak-controlled limiting reaches Ozone-parity breathing. `MultibandLimiter` uses the **LR IIR** split (0-latency, allpass phase). We also ship `LinearPhaseCrossover` (phase-linear FIR, adds latency). This probe measures whether a **phase-linear 2-band split** changes breathing (expected ≈ same) or audio quality (low-end coherence / transient integrity — the real question), before deciding whether to promote it to a module option. It's a **probe in the bench** (like the safety two-pass), not shipping topology.

> ⚠️ **Retrieval log first.** Read `filters/LinearPhaseCrossover.h` (esp. `prepareFixedLatency`, `installActiveKernel`, `processSample(ch, x, &low, &high, &xDelayed)`, `getLatencySamples`), `dynamics/SingleBandLimiter.h`, and the current `tools/mbl_bench.cpp` multi path. Output the crossover API you'll compose.

---

## Change — add `--split lr|linphase` to `mbl_bench`

- **Default `lr`** = current behaviour (`MultibandLimiter`, unchanged).
- **`linphase`** = a bench-composed **2-band** path (only N=2 supported for this probe): 
  1. `LinearPhaseCrossover` split the block into low/high band buffers (`processSample` loop; ignore `xDelayed`).
  2. Run each band buffer through its own `SingleBandLimiter` (same config as the `lr` per-band path: ceiling −1 SP, `--attack-mode`, `--release-ms` per band, `--lookahead-ms`).
  3. Sum the two limited bands. (No safety unless already wired — keep parity with the `lr` no-safety path for the comparison.)
- **Crossover frequency:** use `--crossovers "f"` (single freq for 2-band; default ~120 Hz if omitted) — same freq the `lr` N=2 uses so the comparison is apples-to-apples.
- **Latency:** report the true total = `LinearPhaseCrossover.getLatencySamples()` + `SingleBandLimiter` lookahead (both bands aligned). Print `latency_samples` and `split=linphase`.
- RT-safe, allocation in prepare; deterministic.

## Acceptance (Cursor)
1. Builds clean; `--split lr` unchanged (byte-identical output to before on a sample render).
2. `--split linphase --bands 2` renders sane output (peaks held with `--attack-mode ramp`, no NaNs); prints the correct (larger) latency.
3. **Unity reconstruction sanity:** high `--ceiling-db` (no limiting) → linphase 2-band split+sum is flat magnitude (the LinearPhaseCrossover reconstructs to ~−120 dB vs input delayed by its latency — it IS linear-phase, so vs-delayed-input is meaningful here, unlike LR). Report the max deviation.
4. Report a sample CLI + stdout for both splits.

## Claude's measurement plan (not Cursor's)
Re-use `mbl_measure.py`: render jazz/EDM through `--split lr` vs `--split linphase` at 2-band, `--attack-mode ramp`, drive-matched to Ozone RMS. Compare **300 ms range** (expect ≈ equal — phase doesn't change macro gain), **crest/10 ms peakiness** (transient integrity — linphase pre-ringing may soften), latency, and hand both renders to avishali for the low-end-coherence / phase audition (the real decider).

## Non-goals
- No module edits (probe lives in the bench). No plugin change. No >2 bands for linphase. No new DSP.

## Output requirements (Cursor)
1. Retrieval log. 2. Bench diff. 3. Build + sample runs (both splits) + latencies + unity-reconstruction deviation. 4. Confirm no module/existing-file edits. 5. Open questions.

## Notes for the architect (not for Cursor)
- If linphase measurably improves low-end/transients enough to justify its latency → promote to a real `MultibandLimiter` crossover-type option (proper module slice) per ADR-0013. If not → LR 0-latency stays the 2-band default. Either way it's a cheap, decisive probe.
- After this: the mission work — design the **spectral dynamic threshold lookahead engine** (STFT analysis → adaptive per-band thresholds; the leapfrog above parity).
