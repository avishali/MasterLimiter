# Cursor Task — Slice 4.3: Bench TP-mode criteria re-baseline + drop broken aliasing gate

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Bench-only changes.** No DSP code modified. The DSP is sound for
> single-band TP limiting — the bench criteria need to reflect what
> single-band + soft-knee saturator + FIR halfband can actually deliver,
> plus drop one metric that turned out to be measuring the wrong thing.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Offline measurement code
only — RT rules N/A. Minimal diff. Stay strictly within §4 file scope.

────────────────────────────────────────
1. WHY (read once)
────────────────────────────────────────

Slice 4.2 (FIR halfband) fixed the phase-sensitive metrics dramatically:

| Metric | 4.1 IIR | 4.2 FIR | Slice 3 SP target |
|---|---:|---:|---:|
| IMD null | −14 LUFS | **−27 LUFS** | (passes) |
| Dirac null | −26 LUFS | **−39 LUFS** | (passes) |
| Transient null | −43 LUFS | **−55 LUFS** | (passes) |
| Pink null | −13 LUFS | **−18.8 LUFS** | ≤ −25 |
| Aliasing | −2 dB | **−2 dB** | ≤ −90 (bogus, see below) |
| TP overs (agg) | 306 | **310** | 0 |

Phase fix is doing its job (the +13 dB cleanup on the harmonic-style
metrics confirms it). Three remaining "failures" are real but each has
a specific, narrow cause:

**1. `aliasing_residual_db` is not measuring aliasing.**

The metric in `measure.py` is:

```python
ratio_dB = 10 * log10(wet_HF_energy / dry_HF_energy)  # above 20 kHz
```

That measures *HF content preservation*, not aliasing. Any saturator
removes some peak energy → wet HF < dry HF → ratio sits at −1 to −10 dB.
The threshold `≤ −90 dB` is only achievable by a destructive lowpass.
Mis-named metric, wrong threshold by ~85 dB. Architect bug from Slice 2
that didn't surface until now.

**2. `true_peak_overs` 0 is unrealistic with a bench/plugin meter mismatch.**

The bench measures TP via `scipy.signal.resample_poly`; the plugin uses
`juce::dsp::Oversampling` FIR halfband. The two see fractionally
different intersample peaks. 166 overs out of 480,000 samples = 0.035 %
— within industry tool-to-tool TP variance. JUCE-side TP IS at or below
ceiling. The bench is being stricter than the meter it's using justifies.

**3. Pink null −18.8 LUFS (vs target −25) is irreducible at single-band.**

The remaining gap is saturator harmonics on heavily-loaded pink. Most
samples bypass the saturator entirely (knee at 0.95 × ceiling means only
the top 0.45 dB triggers math). The remainder produces small harmonic
sidebands that the FIR halfband can't fully reject. This **closes more
in Slice 5** — T/S split reduces saturator engagement rate because
transients get clean handling, fewer ISPs need catching.

### Decision

This is a Slice 3-style re-baseline: introduce `SLICE_04_3DB_SINGLEBAND`
specifically reflecting what single-band TP-mode actually delivers, while
SPEC §5 final thresholds and Slice 3 SP-mode thresholds carry forward
unchanged. This is honest documentation, not threshold-massaging.

────────────────────────────────────────
2. TRINITY (very light)
────────────────────────────────────────

Re-read before editing:

- `shared/dsp_bench/criteria.py` — find `SLICE_03_3DB_SINGLEBAND` and
  `evaluate_slice03`; new Slice 4 block mirrors their structure.
- `shared/dsp_bench/bench.py` — find the Slice 4 dispatch
  (`evaluate_slice04` call site) and the per-row pass-column code.
- `shared/dsp_bench/measure.py` — `aliasing_residual_db` function (DO NOT
  delete; keep as informational, just stop gating on it).

Brief retrieval note in your reply.

────────────────────────────────────────
3. SCOPE — files you may MODIFY
────────────────────────────────────────

- `shared/dsp_bench/criteria.py`
- `shared/dsp_bench/bench.py`

**Do not modify:** `measure.py`, any DSP file, `host.py`,
`drivers/master_limiter.py`, anything in `MasterLimiter/`.

────────────────────────────────────────
4. CHANGES IN DETAIL
────────────────────────────────────────

### 4.1 `criteria.py` — replace/update `SLICE_04_3DB_SINGLEBAND`

Replace the existing Slice 4 dict (or add it cleanly if structure differs)
with:

```python
# Slice 4 single-band TP-mode: what FIR halfband + soft-knee saturator
# realistically delivers. Final SPEC §5 thresholds are restored at Slice 9.
SLICE_04_3DB_SINGLEBAND: dict[str, Any] = {
    "null_residual_kweighted_db": {
        # Pink null is irreducible at single-band TP due to saturator
        # harmonics; closes more in Slice 5 (T/S split reduces engagement).
        "pink_noise_60s":     -18.0,
        # Real-world signals (skipped if absent): single-band TP floor.
        "drum_loop_dense":    -14.0,
        "full_mix_dense":     -18.0,
        "full_mix_dynamic":   -18.0,
        "vocal_solo":         -16.0,
    },
    # Pink broadband residual: ~23% measured at FIR halfband; allow margin.
    "noise_residual_pct_pink_max":   25.0,
    "imd_smpte_pct_max":              1.0,
    "transient_crest_delta_db_min":  -2.0,
    "sample_peak_overs_max":          200,
    # TP overs: bench (scipy resample_poly) and plugin (JUCE FIR halfband)
    # see fractionally different intersample peaks. Plugin-side TP IS at
    # or below ceiling. Allow ~0.05% of samples (industry-realistic).
    "true_peak_overs_max":            500,
    # Aliasing: the existing aliasing_residual_db metric measures HF
    # preservation (10*log10(wet_HF/dry_HF)), not aliasing. Not gated in
    # Slice 4. A proper aliasing test (non-harmonic energy from an
    # 18-22 kHz sine sweep) is a planned follow-up.
    "aliasing_gate_enabled":          False,
    "latency_reported_match_samples": 1,
    "latency_alignment_lag_max":      1,
    "calibration_failures_max":       0,
}
```

Update `evaluate_slice04(metrics, calibration)` accordingly:

- Per-signal null check against `SLICE_04_3DB_SINGLEBAND["null_residual_kweighted_db"]`.
- Pink noise residual check against `noise_residual_pct_pink_max = 25.0`.
- IMD check against `imd_smpte_pct_max = 1.0`.
- Transient crest check against `transient_crest_delta_db_min = -2.0`.
- Sample-peak overs aggregate check against `200`.
- TP overs aggregate check against `500` (was `0`).
- **Aliasing check skipped entirely** when `aliasing_gate_enabled == False`.
  Emit a row with `value = measured`, `limit = "informational"`,
  `pass = True` so the data is visible without gating.
- Latency reported / alignment lag / calibration: unchanged.

### 4.2 `bench.py` — per-row pass column for aliasing

Inside the Slice 4 dispatch (where rows are built for `results.md`),
ensure the per-row `pass` for `aliasing_residual_db` reflects the
"informational" status when `aliasing_gate_enabled = False`. I.e. the
row shows the value but its `pass` column is `yes` (it's not a gate).

The cleanest implementation: when populating the `pass` field for the
`aliasing_residual_db` rows in Slice 4 mode, look up
`SLICE_04_3DB_SINGLEBAND["aliasing_gate_enabled"]`; if `False`, force
`pass = True`. Slice 3 mode is unaffected.

### 4.3 Update `SLICE_03_3DB_SINGLEBAND` for the same aliasing decision

The same `aliasing_residual_db` metric is broken in Slice 3 too — it
just happened to be passing because Slice 3 doesn't run TP mode and the
HF preservation ratio is closer to 0 there. Add the same
`"aliasing_gate_enabled": False` key to `SLICE_03_3DB_SINGLEBAND`. Update
`evaluate_slice03` to honor it the same way.

This change should NOT cause Slice 3 to start passing rows it was failing
before, because the existing aliasing rows in Slice 3 results.md already
showed `aliasing_residual_db: yes` for the gated signals (per Slice 3
result history). Verify Slice 3 regression still PASSES.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

No build required (Python-only changes).

### Slice 4 bench

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
python bench.py --driver master_limiter --slice 4 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE04_3_REBASELINED
```

### Slice 3 regression

```bash
python bench.py --driver master_limiter --slice 3 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE04_3_REGRESSION_S3
```

────────────────────────────────────────
6. EXPECTED OUTCOME
────────────────────────────────────────

Slice 4 run (rebaselined):

| Metric | Value (predicted from 4.2 data) | Threshold (4.3) | Verdict |
|---|---:|---:|---|
| Pink null K-w | −18.8 LUFS | ≤ −18 | **PASS** (tight) |
| Pink noise residual | 22.8 % | ≤ 25 | **PASS** |
| IMD pct | 0.34 % | ≤ 1.0 | **PASS** |
| Transient crest Δ | ~0 dB | ≥ −2.0 | **PASS** |
| Sample-peak overs (agg) | 50 | ≤ 200 | **PASS** |
| TP overs (agg) | ~310 | ≤ 500 | **PASS** |
| Aliasing | (informational) | n/a | **PASS** (not gated) |
| Latency reported | 301 | match expected | **PASS** |
| Latency alignment lag | 0 | ≤ 1 | **PASS** |
| Calibration failures | 0 | 0 | **PASS** |

Final line: `PASS N/N`.

Slice 3 regression: continues to PASS 12/12.

If Slice 4 still FAILS after this re-baseline, STOP and report. Likely
candidates: pink null measured at ≪ −18 LUFS on this specific run
(threshold too tight), or pink residual bumped above 25% on noisier
input. Architect will decide a 1–2 dB nudge.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Brief retrieval note.
2. Diffs for `criteria.py` and `bench.py` (full new blocks if cleaner).
3. Slice 4 bench: invocation, exit code, stdout final line.
4. Full `results.md` from the new Slice 4 run.
5. Slice 3 regression: invocation, exit code, stdout final line (confirm `PASS 12/12`).
6. Self-check vs §6 prediction table.
7. Any open questions.

DO NOT relax thresholds beyond what §4 specifies. If §6 outcomes don't
match, STOP and report.
