# SLICE S-E — bench per-band threshold + dynamic threshold curve (leapfrog prototype enabler)

**Status:** ready for Cursor · **Architect:** Claude · **Measure:** Claude (P-A prototype) · **Audition:** n/a
**Repo/scope:** SDK **`melechdsp-hq`** — extend `mbl_bench` only. Additive; **no module edits, no plugin change.**
**Why:** the leapfrog (`docs/ADAPTIVE_THRESHOLD_ENGINE.md`) — first validation (P-A): does driving the **low band's threshold down ahead of bass transients** (lookahead spectral analysis) reduce the clipper's LF work while keeping breathing? To test it, the bench must let Claude set **per-band thresholds, statically AND as a per-frame curve**. `MultibandLimiter` already has `band(i).setThresholdLinear` — this just exposes/drives it.

> ⚠️ **Retrieval log first.** Read `mbl_bench.cpp` (the `--split lr` MB path: `limiter.band(b)` config loop, `processInBlocks`, the block size) + `MultibandLimiter::band()` / `SingleBandLimiter::setThresholdLinear`. Also fix **BUG 0 while here** (below). Output the block loop + threshold setter.

## Changes to `mbl_bench`
1. **`--band-thresholds "d1,d2,...,dN"`** (dB, low→high, N = bands) — static per-band ceiling/threshold. If given, `band(i).setThresholdLinear(dbToLinear(d_i))` (overrides the single `--ceiling-db` for the bands). Lets us limit the low band harder than the high band (quick static test).
2. **`--band-threshold-curve <csv>`** (dynamic) — a CSV: **one row per process block** (the bench's fixed block size, e.g. 512 samples), **N columns = per-band threshold in dB** for that block. Before processing block `k`, apply `band(i).setThresholdLinear(dbToLinear(curve[k][i]))`. If fewer rows than blocks, hold the last row. This is the control signal that drives the adaptive low-band ducking. **Print the block size + expected row count** (= ceil(numSamples/blockSize)) so Claude generates a matching curve.
   - Threshold changes per block are just setter calls (no realloc); RT-safe pattern (this is an offline bench, but keep it clean).
3. **BUG 0 fix (do this):** `writeOutputWav` does not cleanly overwrite an existing `--out` file → stale/corrupt reads. Truncate/recreate on open (or `std::remove` first). This has been poisoning file-based measurement.

## Acceptance (Cursor)
1. Builds clean; existing behaviour unchanged when the new flags are absent.
2. `--band-thresholds "-12,-1"` on a driven file limits the low band harder (audibly/measurably lower low-band level) — sane output, no NaNs.
3. `--band-threshold-curve` applies a time-varying threshold (verify: a curve that ramps the low-band threshold down mid-file audibly ducks the low end there). Print block size + row count.
4. BUG 0: rendering onto an existing `--out` now gives the fresh result (== a rm'd render).
5. Report the exact CLI + a sample run.

## Claude's P-A plan (not Cursor's)
Compute in Python: STFT-analyse the input's low band with lookahead → a per-block low-band threshold curve that DIPS a few ms before bass transients (leave the high band fixed) → render via `--band-threshold-curve` → apply a clipper proxy (hardclip −1) → measure **clipper GR (peak overshoot the clip must remove), LF THD, 300 ms range, loudness** vs the fixed-threshold baseline. **Gate: adaptive low-band ducking reduces clipper LF work + LF THD while keeping breathing.** If yes → the leapfrog core is validated → build the C++ `SpectralThresholdController`.

## Non-goals
- No module edits (module already has per-band threshold). No plugin change. No STFT in C++ yet (that's post-P-A).

## Output requirements
1. Retrieval log. 2. Bench diff (both flags + BUG 0 fix). 3. Build + sample runs (static + dynamic) + block-size/row-count print. 4. Confirm no module/existing-file edits. 5. Open questions.

## Notes for the architect
- This is the ADAPTIVE_THRESHOLD_ENGINE §9 step S-E, scoped as a bench probe (fast, confound-free vs the plugin). It also finally fixes BUG 0. After P-A validates, the C++ brain (StftEngine analysis → controller → MultibandLimiter thresholds) is the real build.
