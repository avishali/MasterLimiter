# Cursor Task — Slice 3.2: Bench alignment fix + Slice 3 criteria re-baseline

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Bench-only changes.** No DSP code is modified. Workspace: `melechdsp-hq`.
> All edits live under `shared/dsp_bench/`.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. This is offline measurement
tooling — no audio-thread RT rules apply, no Engine/UI boundary rules apply.
What does apply:

- STAY within the files listed in §4. STOP if any other file needs touching.
- Minimal diff. No speculative features. No new metrics.
- Reproducibility: bench must remain deterministic.

────────────────────────────────────────
1. ROOT CAUSE (diagnostic, read once)
────────────────────────────────────────

Architect-side diagnosis confirmed via direct experiments:

**Pedalboard `VST3Plugin` performs automatic plugin delay compensation.**
The `wet` array returned by `Pedalboard(...)(audio, sr)` is already
time-aligned to the input. `wet[0]` corresponds to `dry[0]`.

The bench's `_align_gain_match_residual(dry, wet, sr, latency)` then applies
an *additional* shift of `latency` samples (`wet[lat:lat+n]`), which
**double-compensates** for the plugin's latency and decorrelates dry vs. wet.
On pink noise this produces residual ≈ √2 × signal (107%); on a sine sweep
it produces residual louder than the signal (impossible without misalignment).

Cross-correlation evidence: pink @ 3 dB GR, lat=0 → residual/dry = 0.096
(reasonable single-band transparency). Same audio at lat=240 → 1.07.

The fix: stop shifting wet by latency for residual measurements. Pedalboard
already aligned them. The plugin's reported latency is still useful as a
**self-reporting validation** (does the plugin advertise the expected
lookahead?) — but it is NOT a wet-alignment offset.

Additionally, the lat_meas-vs-lat_rep selection logic in `bench.py` (Slice 3
branch) is back-to-front: when `lat_probe == 0` it currently falls back to
`lat_rep = 240`. With pedalboard PDC, `lat_probe == 0` is the CORRECT result,
not an error.

────────────────────────────────────────
2. ALSO: Slice 3 criteria are re-baselined
────────────────────────────────────────

SPEC §5 thresholds at 3/5/7 dB GR are **final-system** criteria — they assume
the full multi-stage architecture (Slices 3+4+5+6 combined). A single-band
sample-peak lookahead limiter (Slice 3 alone) cannot reach them on dense
material — that's specifically why Slice 5 adds transient/sustain split.

So Slice 3 gets its own intermediate "single-band achievable" criteria,
documented separately from the final SPEC §5 numbers. SPEC §5 thresholds
carry forward to the Slice 9 ship gate unchanged.

────────────────────────────────────────
3. TRINITY (lightweight)
────────────────────────────────────────

Re-read before editing (no MCP needed):

- `shared/dsp_bench/measure.py` — alignment helper + all functions that
  currently accept a `latency` parameter
- `shared/dsp_bench/bench.py` — the Slice 3 latency-selection branch and
  the per-signal measurement calls
- `shared/dsp_bench/criteria.py` — Slice 3 absolute thresholds

Output a brief retrieval log (which files were re-read) before producing the diff.

────────────────────────────────────────
4. SCOPE (files you may modify)
────────────────────────────────────────

- `shared/dsp_bench/measure.py`
- `shared/dsp_bench/bench.py`
- `shared/dsp_bench/criteria.py`

**Do NOT modify:** anything else, especially not under `MasterLimiter/` or
`shared/mdsp_dsp/`. If a fix appears to require touching another file,
STOP and ask.

────────────────────────────────────────
5. CHANGES IN DETAIL
────────────────────────────────────────

### 5.1 `measure.py` — alignment helper and all consumers

`_align_gain_match_residual(dry, wet, sample_rate, latency)`:

- Drop `latency` from the parameters entirely (signature change OR ignore
  the value, your choice — but the function must no longer slice `wet[lat:]`).
- Implementation: `n = min(dry.shape[0], wet.shape[0])`,
  `d_u = dry[:n]`, `w_u = wet[:n]`, then RMS gain-match and subtract as before.

Update **every consumer** of `_align_gain_match_residual` accordingly:

- `null_residual_kweighted_db(dry, wet, sample_rate)` — drop `latency` kwarg
- `noise_residual_pct(dry, wet, sample_rate)` — drop `latency` kwarg
- `imd_smpte_pct(dry, wet, sample_rate)` — drop `latency` kwarg
- `aliasing_residual_db(dry, wet, sample_rate)` — drop `latency` kwarg
- `measured_gr_db(dry, wet, sample_rate, signal_name=...)` — drop `latency`
- Any other helper that accepts `latency`: drop it.

Keep:
- `latency_samples(dry_impulse, wet_impulse)` — still useful as a
  diagnostic (it will return 0 against pedalboard, confirming PDC).
- `latency_samples_xcorr(...)` — same, diagnostic only.

Add (do not remove):
- A one-line module-level docstring or comment near the top noting that
  pedalboard is assumed to handle plugin delay compensation; residual
  measurements treat dry/wet as already aligned.

### 5.2 `bench.py` — drop latency from residual paths

- Remove the Slice 3 `lat_probe / lat_rep` selection branch entirely.
- Continue to *measure* latency via the dirac impulse and to *read* the
  plugin's reported latency, for the **latency-reporting gate** (see §5.4).
  Specifically:
  - `lat_probe_impulse = measure.latency_samples(dry_imp, wet_imp)` —
    expected to be **0** under pedalboard PDC.
  - `lat_reported = host_lat.reported_latency_samples()` — expected to be
    `round(0.005 * sr)` (240 @ 48 kHz).
- Stop passing `latency=` to any measurement call. Update every call site
  in `bench.py` accordingly.
- The variable `lat_meas` may be removed or kept only as `lat_probe_impulse`.
- Calibration loop: same change — drop `latency=` from `measured_gr_db` calls.

### 5.3 `bench.py` — latency rows in results

Replace the existing latency rows with these two cleaner ones:

- Row metric `"latency_reported_samples"`, value = `lat_reported`,
  pass = `abs(lat_reported - expected_latency) <= 1`.
- Row metric `"latency_alignment_lag_samples"`, value = `lat_probe_impulse`,
  pass = `abs(lat_probe_impulse) <= 1` (verifies pedalboard PDC actually
  zeroes the alignment as expected; if a future pedalboard release stops
  PDC'ing, this row will fail and we'll know).

Drop the old combined `latency_samples_measured` row.

### 5.4 `criteria.py` — Slice 3 single-band thresholds

Add a new dict, mirroring `SLICE_02_SELF_VALIDATION`'s style:

```python
SLICE_03_3DB_SINGLEBAND = {
    "null_residual_kweighted_db": {
        "pink_noise_60s":     -25.0,
        # Real-world signals (skipped if absent) — these are HEAD-ROOM-relative
        # numbers for the single-band Slice 3 stage. Final SPEC §5 thresholds
        # (-60 etc.) are restored at the Slice 9 ship gate.
        "drum_loop_dense":    -18.0,
        "full_mix_dense":     -22.0,
        "full_mix_dynamic":   -22.0,
        "vocal_solo":         -20.0,
    },
    "noise_residual_pct_pink_max":      5.0,    # was 0.10 in SPEC §5
    "imd_smpte_pct_max":                1.0,    # was 0.08
    "transient_crest_delta_db_min":    -2.0,    # was -0.6
    "sample_peak_overs_max":           200,     # SP-only path; TP+OS comes in Slice 4
    "true_peak_overs_max":            None,     # not gated in Slice 3 (Slice 4 introduces TP+OS)
    "latency_reported_match_samples":   1,
    "latency_alignment_lag_max":        1,
    "calibration_failures_max":         0,
}
```

Keep the existing `SLICE_03_3DB` dict for reference (rename to
`SPEC_FINAL_3DB` if you prefer, or leave under a clear comment) — it is the
final SPEC §5 row and will be wired back in at the ship gate.

Write a new `evaluate_slice03(metrics, calibration)` that consumes
`SLICE_03_3DB_SINGLEBAND`. It should produce pass/fail rows analogous to
`evaluate_slice02`, including:

- `null_residual_kweighted_db` per signal (skip rows for absent signals,
  emit a warning row instead).
- `noise_residual_pct_pink`
- `imd_smpte_pct` (only when imd_smpte signal was rendered)
- `transient_crest_delta_db` (compute as `crest_wet - crest_dry`, gate is
  `value >= min_threshold` — i.e. `value >= -2.0`)
- `sample_peak_overs_max` aggregated across the gated signal set
  (pink/sweep/imd_smpte — same `_tp_gate_signals` policy as Slice 2)
- `latency_reported_samples` and `latency_alignment_lag_samples`
- `calibration_failures`

Rows for absent real-world signals: emit with `pass = True, note="signal absent"`.
Aggregate `Overall: PASS` only if every gated row is a pass.

────────────────────────────────────────
6. RUN
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
python bench.py --driver master_limiter --slice 3 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE03_AFTER_BENCHFIX
```

────────────────────────────────────────
7. EXPECTED OUTCOME
────────────────────────────────────────

Architect-predicted numbers (within ballpark — actual may vary by a few dB):

- `latency_reported_samples` ≈ 240, pass.
- `latency_alignment_lag_samples` ≈ 0, pass.
- `null_residual_kweighted_db` on pink: somewhere in **−25 .. −35 LUFS**,
  pass (threshold −25).
- `noise_residual_pct` on pink: well under 5% (probably 1–3%), pass.
- `imd_smpte_pct`: ≤ 0.1% (already passed pre-fix at 0.071), pass.
- `transient_crest_delta_db`: depends on signal; the synthetic
  `transient_train` may need real auditioning, but should pass −2.0.
- `sample_peak_overs_max` aggregate: small double-digit count, pass.
- Calibration failures: 0.

Final line: `PASS N/N`.

If any of these fails by a wide margin **after** the alignment fix, STOP
and report — that indicates a second-order issue that needs separate
investigation.

────────────────────────────────────────
8. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Brief retrieval note (files re-read).
2. Unified diff (or per-file before/after if small) for each of the three
   files modified.
3. Bench invocation, exit code, and stdout final line.
4. Pasted contents of `results.md` from the new run.
5. The numeric values for the metrics listed in §7.
6. Self-check: each §7 expectation marked "pass" / "fail by Xdb / Xpp".
7. Any open questions.

DO NOT change the DSP under `shared/mdsp_dsp/` or `MasterLimiter/Source/`
under any circumstances in this slice. If you believe DSP needs changing,
STOP and report.
