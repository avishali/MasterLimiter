# SLICE S-C — offline wav bench for the §4a test (real DSP: K=1 vs multiband)

**Status:** ready for Cursor (bench only) · **Architect:** Claude · **Measure/verify:** Claude (Python, drives the bench + 300 ms range) · **Audition:** n/a
**Repo/scope:** SDK **`melechdsp-hq`** — a new **CLI bench executable** that renders a wav through the real `SingleBandLimiter` / `MultibandLimiter` modules. Additive; no plugin change, no edits to existing SDK classes. **Cursor builds the bench (a pure renderer). Claude runs it + measures** (the tuning/measurement is Claude's, not the bench's).
**Why:** finally run the §4a test — *does per-band limiting + per-band release recover macro-dynamic breathing vs wideband?* — on **production DSP** (the S-A/S-B modules), not a Python re-implementation. This is the whole reason for the module arc.

> ⚠️ **Retrieval log first.** Read `dynamics/SingleBandLimiter.h`, `dynamics/MultibandLimiter.h` (the exact setters), and how the SDK builds a small executable/test target + does WAV I/O (JUCE `AudioFormatManager`, already a dep). Output the module setters you'll wire.

---

## Deliverable — a deterministic CLI renderer `mbl_bench`

New file(s) under the SDK (e.g. `shared/mdsp_dsp/tools/mbl_bench.cpp` + CMake target). It reads a stereo 48 kHz wav, applies input gain, runs ONE module config, writes a stereo 48 kHz float wav. **No tuning, no measurement, no sweep — a pure one-shot renderer** (Claude orchestrates sweeps by calling it repeatedly).

**CLI:**
```
mbl_bench --in <wav> --out <wav>
          --mode single|multi
          --drive-db <float>            # input gain applied before limiting
          --ceiling-db <float>          # per-band threshold, SAMPLE-PEAK (default -1.0)
          --lookahead-ms <float>        # default 5
          --release-engine lookahead|smart   # default lookahead
          # single mode:
          --release-ms <float>
          # multi mode:
          --bands <N>                   # 1..8
          --crossovers "f1,f2,..."      # N-1 freqs Hz; omitted => log-spaced defaults
          --release-ms "r1,r2,...,rN"    # per-band release (low→high). single value => all bands same
          # safety is OFF (do not enable it in this bench)
```
- `--mode single` → one `SingleBandLimiter` (the K=1 wideband baseline).
- `--mode multi` → one `MultibandLimiter`, `setSafetyEnabled(false)`, `--bands` N, crossovers set, per-band threshold = `--ceiling-db` (sample-peak), each band's release from the `--release-ms` list via `band(i)`.
- **No FinalCeiling, no clipper, no safety, no true-peak mode.** Match Ozone: sample-peak, ceiling −1, small inter-sample overs allowed.
- Print to stdout: `latency_samples=<int>` and `mode/bands/drive` echo. Nothing else needed (Claude measures the wav).
- Deterministic: same args → same output bytes.

**Acceptance (Cursor):** builds clean; `mbl_bench --mode single --in x.wav --out y.wav --drive-db 0 --ceiling-db 0 --release-ms 100` on a −6 dB test wav produces sane output (peaks ≈ 0 dBFS, no NaNs); a high `--ceiling-db` (no limiting) passes audio through (allpass for multi). Report the exact CLI, the build target name, and reported latency for a sample run.

## Claude's measurement plan (NOT Cursor's — for the architect's record)
Once the bench exists, Claude (Python, `tools/analysis/`) will:
1. For each genre (JAZZ `MIX 0003`, EDM `MIX 0001`) with Ozone target RMS (−11.09 / −10.60):
2. For each config — **single (K=1)**, **multi N∈{2,4,8}** — and per-band release profiles (uniform; and fast-HF/slow-LF):
3. Bisect `--drive-db` (ceiling fixed −1) so the rendered output RMS ≈ Ozone; render via `mbl_bench`; measure **300 ms range**, RMS, TP (existing `st_range`/`true_peak_db`/`rms_db`).
4. Compare 300 ms range: K=1 vs multiband, and vs Ozone (4.68 / 5.11). **Gate: does range climb with per-band limiting + per-band release at matched loudness/TP?** K=1 must reproduce ~2.48 / 3.35.

## Non-goals
- No plugin change. No safety stage. No tuning logic inside the bench. No new DSP.
- Do not edit the modules or other SDK classes.

## Output requirements (Cursor)
1. Retrieval log. 2. `mbl_bench.cpp` + CMake target. 3. Build output + one sample run (CLI + stdout + confirms a wav was written). 4. Confirm no existing files edited. 5. Open questions.

---

## REVISION 1 / S-C.1 (2026-07-06, after run 1 — architect) — peak control for the Ozone match

Run 1 gave a **strong preliminary positive** (multiband 300 ms range > wideband, meets/exceeds Ozone — see `docs/SPECTRAL_ENGINE_DESIGN.md` "S-C first result"). But it is NOT peak-matched to Ozone: the bench's hardcoded **Hybrid attack at default RC passes transients → TP 3.9–8.9 dB** (Ozone −0.48). To confirm at matched loudness AND peak, the bench needs attack control. Add:

1. **Attack control (CLI):** `--attack-mode ramp|real|hybrid` (default keep current), `--attack-ms <float>` (maps to the envelope's attack), and for hybrid the RC via `--real-attack-ms <float>`. Wire to `SingleBandLimiter`/`MultibandLimiter` band(i) via the existing `setAttackMode`/`setRealAttackMs` setters. **Goal: `--attack-mode ramp` (or fast hybrid) must actually HOLD peaks** so a driven render lands within ~1–2 dB of the ceiling (memory: Ramp attack catches, Real/Hybrid-default pass).
2. **Lookahead headroom:** `--lookahead-ms` currently looks capped (5/20/50 ms give identical output). Prepare the module `Spec.lookaheadSamples` with **enough headroom** for the requested lookahead (e.g. size for the max of {requested, 20 ms}) and `setActiveLookaheadSamples` to the runtime value, so `--lookahead-ms` genuinely changes peak-catching. Report the resulting latency.
3. **Truncation fix (robustness):** the bench does not cleanly overwrite an existing `--out` file (leaves a corrupt file if the prior one was larger) — truncate/recreate on open. (Claude's orchestrator currently `rm`s the file as a workaround.)

**Verify (Cursor):** a single-mode render of a −6 dB tone at `--drive-db 12 --ceiling-db -1 --attack-mode ramp` → output sample-peak ≤ ~0 dBFS (peaks HELD, not +10). Report sample-peak/TP for ramp vs hybrid to show the difference. Additive; no module edits unless a missing setter forces it (then flag).

After S-C.1: Claude re-runs `mbl_measure.py` with `--attack-mode ramp` (tuned so TP ≤ ~1 dB), re-measures §4a at matched RMS **and** matched TP → the rigorous confirmation. Then commit the bench (S-C + S-C.1 together).

## Notes for the architect (not for Cursor)
- Bench is the seam: Cursor owns the C++ renderer, Claude owns the sweep+measurement (correct role split — no Python re-implementation of DSP).
- If §4a shows a real range climb on 8-band production DSP → the mechanism is proven cheaply; the many-band/spectral (STFT) regime becomes the justified follow-on. If flat/negative even here → per-band limiting isn't the lever and we rethink before more investment. Either outcome is decisive and on REAL DSP this time.
