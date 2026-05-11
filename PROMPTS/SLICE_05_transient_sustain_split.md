# Cursor Task — Slice 5: Transient/Sustain Split

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Both repos.** HQ: extend `LimiterEnvelope` with the T/S split mechanism;
> bench gets Slice 5 criteria. Product: new APVTS parameter
> `release_sustain_ratio`, processor wires it into the envelope.
> ADR-0005 already committed in HQ.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE (MANDATORY)
────────────────────────────────────────

Obey `melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- ONE task = ONE focused diff. Stay strictly within §4 file scope.
- RT-safety on the audio thread:
  - No allocations in `LimiterEnvelope::process` or `processBlock`.
  - All new state members preallocated in `prepare()`.
  - No locks, no logging.
- Engine/UI boundary preserved. Snapshot via existing `currentGrDb_`
  atomic — no new public state.
- APVTS is the only parameter source. New parameter ID
  `release_sustain_ratio` is FROZEN from this slice onward.
- Minimal diff. No speculative features.

────────────────────────────────────────
1. TRINITY (lightweight)
────────────────────────────────────────

Before any code:

1. `melech_internal_server` (or repo grep fallback) — confirm:
   - `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h` and
     `.cpp` are the canonical files to extend.
   - `MasterLimiter/Source/parameters/ParameterIDs.h` and
     `Parameters.cpp` are the canonical locations for new param IDs.
   - `MasterLimiter/Source/PluginProcessor.{h,cpp}` already reads
     `release_ms` per block — pattern to mirror for the new param.
   - Existing `mdsp_dsp::Smoother` pattern (for reference; we use
     direct atomic param reads in the processor today).
2. `juce_api_server` — verify JUCE 8 `AudioParameterFloat` ctor
   accepting `NormalisableRange` (already used for `release_ms`).
3. `melech_dsp_server` — confirm no existing transient/sustain split
   helper in `mdsp_dsp/` (expected: none — this is new functionality
   inside `LimiterEnvelope`).

Output the retrieval log. If any field is missing, output ONLY a
retrieval plan and STOP.

────────────────────────────────────────
2. TASK TITLE
────────────────────────────────────────

Extend `mdsp_dsp::LimiterEnvelope` with a fast/slow envelope-difference
T/S split per ADR-0005. Audio path unchanged. New APVTS parameter
`release_sustain_ratio` (float, 1.0–10.0, default 4.0) drives the slow
envelope's release time. `ratio == 1.0` collapses to current single-band
behavior. Bench gets a Slice 5 criteria block targeting both 3 dB and
5 dB GR depths, with thresholds tightened toward SPEC §5 final.

────────────────────────────────────────
3. CONTEXT
────────────────────────────────────────

- Architecture: `melechdsp-hq/docs/DECISIONS/ADR-0005-transient-sustain-split.md`
  (committed alongside this slice).
- ADR-0004 Stage 2 (parent): `melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md`
- Spec: `MasterLimiter/docs/SPEC.md` §5
- Pattern reference: existing `LimiterEnvelope::process` is the file
  you extend in place — both the backward-propagation block and the
  release-smoothing block stay the same except for the addition of a
  parallel slow envelope.

Decisions locked from Q5 interview:

| # | Decision |
|---|----------|
| Q5-1 | Method (a) fast/slow envelope-difference, detection-bus only. |
| Q5-2 | Single new param `release_sustain_ratio` (1.0–10.0, default 4.0). |
| Q5-3 | Detection-bus split only — audio path untouched. |
| Q5-4 | Extend existing `LimiterEnvelope` in place; do NOT create a new component. |
| Q5-5 | Slice 5 bench criteria block targets both 3 dB and 5 dB GR. |
| Q5-6 | Audition target: re-A/B vs Ozone IRC 1; gap should drop from 2 LU to ≤ 1 LU. |
| Q5-7 | Method (a) recorded in ADR-0005 (already committed). |

────────────────────────────────────────
4. SCOPE — files you may CREATE / MODIFY
────────────────────────────────────────

### HQ (`melechdsp-hq/`)

**MODIFY:**
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h` (new state members, optional ratio setter)
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` (parallel slow envelope; `min` combine)
- `shared/dsp_bench/criteria.py` (`SLICE_05_3DB_TS_SPLIT`, `SLICE_05_5DB_TS_SPLIT`, `evaluate_slice05`)
- `shared/dsp_bench/bench.py` (slice 5 dispatch, default GR depths `[3.0, 5.0]`)
- `shared/dsp_bench/drivers/master_limiter.py` (slice 5 default params include `release_sustain_ratio`)

### Product repo (`MasterLimiter/`)

**MODIFY:**
- `Source/parameters/ParameterIDs.h` (new constant `release_sustain_ratio`)
- `Source/parameters/Parameters.cpp` (new `AudioParameterFloat` in layout)
- `Source/PluginProcessor.h` (cached param pointer)
- `Source/PluginProcessor.cpp` (apply ratio per block)

**Do not modify:** anything not listed. The UI auto-builds from APVTS in
Slice 1's MainView pattern; the new control should appear automatically.
If MainView needs explicit handling for the new control, STOP and ask.

NON-GOALS:
- NO new components in `mdsp_dsp/dynamics/`. Extend `LimiterEnvelope` in place.
- NO multiband, no spectral split.
- NO change to `IspTrimStage`, `TruePeakDetector`, `LookaheadDelay`, `PeakDetector`.
- NO new UI components, no editor changes.
- NO meter additions (Slice 8).
- NO auto-release algorithm changes (Slice 9).

────────────────────────────────────────
5. ALGORITHM — `LimiterEnvelope` extension
────────────────────────────────────────

### New header state (LimiterEnvelope.h)

Add to private members:

```cpp
float releaseSustainRatio_ = 4.0f;  // ≥ 1.0
float alphaSlow_           = 1.0f;
float stage1Slow_          = 1.0f;
float stage2Slow_          = 1.0f;
```

Add public setter (matching the existing `setReleaseMs` pattern):

```cpp
void setReleaseSustainRatio (float ratio) noexcept;
```

`recomputeAlpha()` already recomputes `alpha_`. Extend it (or add a
sibling helper) to also recompute `alphaSlow_` using
`releaseMs_ × releaseSustainRatio_`. Clamp the ratio to [1.0, 10.0]
inside the setter.

### Algorithm change in `process()`

In the existing release-smoothing loop (the second pass after backward
attack propagation), run the cascade TWICE per sample — once with the
fast coefficient and once with the slow — and output the **per-sample
min**:

```cpp
// Existing fast cascade — unchanged
if (inp < s2_fast - 1.0e-12f) { s1_fast = inp; s2_fast = inp; }
else {
    s1_fast = alpha_     * s1_fast + (1.0f - alpha_)     * inp;
    s2_fast = alpha_     * s2_fast + (1.0f - alpha_)     * s1_fast;
}

// New slow cascade — same shape, slow alpha
if (inp < s2_slow - 1.0e-12f) { s1_slow = inp; s2_slow = inp; }
else {
    s1_slow = alphaSlow_ * s1_slow + (1.0f - alphaSlow_) * inp;
    s2_slow = alphaSlow_ * s2_slow + (1.0f - alphaSlow_) * s1_slow;
}

const float g = std::clamp (std::fmin (s2_fast, s2_slow), 1.0e-12f, 1.0f);
gainOut[j] = g;
```

Persist `stage1Slow_` / `stage2Slow_` across blocks the same way the fast
stages are persisted.

`reset()` initialises both fast and slow stages to 1.0.

### Behaviour

- **`ratio = 1.0`:** fast and slow alphas equal → `s2_fast == s2_slow`
  every sample → `min` is a no-op → output identical to current Slice 3/4
  behaviour. This is the regression-safety path.
- **`ratio > 1.0`:** slow envelope releases slower; on transient-rich
  material the fast envelope recovers quickly between peaks (preserving
  transient feel) while the slow envelope holds a steadier GR floor
  (raising sustained loudness). Output = whichever envelope demands
  more reduction.

────────────────────────────────────────
6. PRODUCT REPO — `release_sustain_ratio` parameter
────────────────────────────────────────

### 6.1 `Source/parameters/ParameterIDs.h`

Add a new namespace constant alongside the existing IDs:

```cpp
inline constexpr std::string_view release_sustain_ratio { "release_sustain_ratio" };
```

ID is FROZEN from this slice. Do not rename, retype, or remove.

### 6.2 `Source/parameters/Parameters.cpp`

Add to the layout (place near `release_ms` for readability):

```cpp
layout.add (std::make_unique<AudioParameterFloat> (
    pid (release_sustain_ratio, 1),
    "Release Sustain Ratio",
    NormalisableRange<float> (1.0f, 10.0f, 0.01f),
    4.0f,
    AudioParameterFloatAttributes().withLabel ("x")));
```

Version hint **1** (same as the existing v1 params — the introduction is
part of v1).

### 6.3 `Source/PluginProcessor.h`

Add a cached pointer member:

```cpp
juce::AudioParameterFloat* releaseSustainRatio_ = nullptr;
```

Initialise in the same `dynamic_cast` block as the existing param
pointers; `jassert(releaseSustainRatio_)`.

### 6.4 `Source/PluginProcessor.cpp`

In `processBlock`, where `envelope_.setReleaseMs(...)` is called, add
immediately after:

```cpp
envelope_.setReleaseSustainRatio (releaseSustainRatio_->get());
```

No other processor changes. Latency, snapshots, mode-switching all stay
the same.

────────────────────────────────────────
7. BENCH UPGRADES
────────────────────────────────────────

### 7.1 `criteria.py`

Add two new dicts and a Slice 5 evaluator:

```python
SLICE_05_3DB_TS_SPLIT: dict[str, Any] = {
    "null_residual_kweighted_db": {
        "pink_noise_60s":     -28.0,
        "drum_loop_dense":    -22.0,
        "full_mix_dense":     -28.0,
        "full_mix_dynamic":   -28.0,
        "vocal_solo":         -25.0,
    },
    "noise_residual_pct_pink_max":   10.0,
    "imd_smpte_pct_max":              0.25,
    "transient_crest_delta_db_min":  -0.8,
    "sample_peak_overs_max":          200,
    "true_peak_overs_max":            500,
    "aliasing_gate_enabled":          False,
    "latency_reported_match_samples": 1,
    "latency_alignment_lag_max":      1,
    "calibration_failures_max":       0,
}

SLICE_05_5DB_TS_SPLIT: dict[str, Any] = {
    "null_residual_kweighted_db": {
        "pink_noise_60s":     -22.0,
        "drum_loop_dense":    -18.0,
        "full_mix_dense":     -22.0,
        "full_mix_dynamic":   -22.0,
        "vocal_solo":         -20.0,
    },
    "noise_residual_pct_pink_max":   15.0,
    "imd_smpte_pct_max":              0.40,
    "transient_crest_delta_db_min":  -1.2,
    "sample_peak_overs_max":          300,
    "true_peak_overs_max":            700,
    "aliasing_gate_enabled":          False,
    "latency_reported_match_samples": 1,
    "latency_alignment_lag_max":      1,
    "calibration_failures_max":       0,
}
```

`evaluate_slice05(metrics, calibration)` follows the Slice 3/4 pattern but
runs both GR-depth tables. If both depths pass → overall PASS. If either
fails → overall FAIL with per-depth detail.

### 7.2 `bench.py`

- Slice 5 default GR depths: `[3.0, 5.0]`.
- Slice 5 dispatch routes to `evaluate_slice05`.
- Per-row `pass` column for null-residual uses the *correct depth's*
  threshold map (avoid the cosmetic issue from prior slices where wrong
  thresholds leaked into the table — Q4-10 fix extended to depth-aware).
- Per-row `pass` column for `aliasing_residual_db` continues to honor
  `aliasing_gate_enabled` (informational only).

### 7.3 `drivers/master_limiter.py`

When `slice >= 5`, default params include `"release_sustain_ratio": 4.0`.
Slice 3/4 default omits it (parameter exists in v1 but Slice 3/4 runs
exercise the `ratio = 1.0` no-T/S path by NOT passing the value, letting
the plugin's APVTS default kick in — which is 4.0 actually).

Wait: with the param defaulting to 4.0 in Parameters.cpp, **Slice 3 and
Slice 4 regression runs will now see T/S split engaged** unless the
driver explicitly sets ratio=1.0 for those slices.

**Required fix in driver:** Slice 3 and Slice 4 default params include
`"release_sustain_ratio": 1.0` to keep the regression behavior identical
to the pre-Slice-5 measurements. Slice 5 default uses 4.0. Document the
reason in a comment.

────────────────────────────────────────
8. BUILD & VERIFY
────────────────────────────────────────

### Build both repos

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```

Zero new warnings from §4 files.

### Slice 5 bench (T/S split engaged, both 3 dB and 5 dB GR)

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
python bench.py --driver master_limiter --slice 5 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE05_TS_SPLIT
```

### Slice 3 regression (ratio forced to 1.0 via driver)

```bash
python bench.py --driver master_limiter --slice 3 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE05_REGRESSION_S3
```

### Slice 4 regression (ratio forced to 1.0 via driver)

```bash
python bench.py --driver master_limiter --slice 4 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE05_REGRESSION_S4
```

────────────────────────────────────────
9. EXPECTED OUTCOME
────────────────────────────────────────

Architect predicts (within ballpark):

**Slice 5 @ 3 dB GR:**

| Metric | Prediction | Threshold | Verdict |
|---|---:|---:|---|
| Pink null K-w | −28 to −33 LUFS | ≤ −28 | PASS |
| Pink noise residual | 6–9 % | ≤ 10 | PASS |
| IMD | 0.10–0.20 % | ≤ 0.25 | PASS |
| Transient crest Δ | small | ≥ −0.8 dB | PASS |
| TP / SP overs | similar to Slice 4 | thresholds same | PASS |

**Slice 5 @ 5 dB GR:**

| Metric | Prediction | Threshold | Verdict |
|---|---:|---:|---|
| Pink null K-w | −22 to −26 LUFS | ≤ −22 | PASS |
| Pink noise residual | 10–14 % | ≤ 15 | PASS |
| IMD | 0.20–0.35 % | ≤ 0.40 | PASS |
| Transient crest Δ | moderate | ≥ −1.2 dB | PASS |

**Both regressions (Slice 3, Slice 4) PASS unchanged** with driver
forcing ratio = 1.0.

**Three outcome cases:**

1. All thresholds met at both depths AND regressions pass → SLICE 5 PASS,
   slice closes.
2. 3 dB passes, 5 dB fails on one or two metrics by ≤ 30% margin →
   STOP and report; architect decides nudge vs tuning.
3. Anything weirder (regressions fail, both depths fail badly, etc.) →
   STOP and report.

────────────────────────────────────────
10. GATE CHECKLIST
────────────────────────────────────────

- [ ] HQ + product build clean; only §4 files touched.
- [ ] Slice 5 bench at 3 dB GR meets `SLICE_05_3DB_TS_SPLIT`.
- [ ] Slice 5 bench at 5 dB GR meets `SLICE_05_5DB_TS_SPLIT`.
- [ ] Slice 3 regression: PASS at ratio=1.0 (single-band behavior intact).
- [ ] Slice 4 regression: PASS at ratio=1.0.
- [ ] `ratio = 1.0` produces bit-identical output to the Slice 4 plugin
      (manual sanity check via signed-zero null on pink noise — not a
      blocking gate item, just an informal verification).
- [ ] Latency reporting unchanged from Slice 4.

────────────────────────────────────────
11. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. RETRIEVAL LOG (§1 format).
2. File list created / modified with one-line purpose each, split by repo.
3. Build summary lines (HQ + product, both Debug and Release; warnings count).
4. Slice 5 bench: invocation, exit code, stdout final line.
5. Full pasted `results.md` from the Slice 5 run.
6. Slice 3 regression: invocation, exit code, final line (confirm PASS).
7. Slice 4 regression: invocation, exit code, final line (confirm PASS).
8. Self-check vs §9 prediction tables.
9. Any open questions.

DO NOT iterate beyond §9 outcome 1. If outcome 2 or 3, STOP and let the
architect decide.
