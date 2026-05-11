# Cursor Task — MasterLimiter Slice 3: Lookahead + Sample-Peak Single-Band Limiter

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Two repos this slice.** HQ adds new `mdsp_dsp/dynamics/` components.
> Product repo wires them up and fixes a small param bug. Open Cursor with
> both `melechdsp-hq` and `MasterLimiter` visible, or use absolute paths.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE (MANDATORY)
────────────────────────────────────────

Obey `melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Headline rules that apply to
this slice in full force (this is the first real DSP slice):

- ONE prompt = ONE task. Stay strictly within §4 file scope.
- Real-time safety on the audio thread:
  - No allocations in `processBlock` or any DSP callee.
  - No locks. No logging. No file I/O.
  - All buffers preallocated in `prepareToPlay` / `prepare()`.
  - Parameter smoothing via `mdsp_dsp::Smoother`.
- Engine/UI boundary: UI consumes snapshots only. Snapshot publication is via
  `std::atomic<T>` (mirror the `LoudnessAnalyzer` pattern).
- APVTS is the only source of parameter truth. Parameter IDs are FROZEN —
  never rename or retype existing IDs.
- Minimal diff. No speculative parameters, modes, or hooks.

────────────────────────────────────────
1. TRINITY PROTOCOL RETRIEVAL GATE
────────────────────────────────────────

Before any code, complete retrieval and output the log:

1. `melech_internal_server` (or repo grep fallback) — confirm canonical paths:
   - `shared/mdsp_dsp/include/mdsp_dsp/Smoother.h`
   - `shared/mdsp_dsp/include/mdsp_dsp/MeterBallistics.h`
   - `shared/mdsp_dsp/include/mdsp_dsp/loudness/LoudnessAnalyzer.h`
     (reference for the `std::atomic<float>` snapshot pattern).
   - `shared/mdsp_dsp/CMakeLists.txt` (this is where new src files register).
2. `juce_api_server` — verify:
   - `juce::dsp::DelayLine`, `juce::dsp::ProcessSpec`, `juce::AudioBuffer`
     interfaces for JUCE 8.
   - `juce::AudioProcessor::setLatencySamples` and `getLatencySamples`.
   - `juce::NormalisableRange<float>` 3-arg constructor `(start, end, interval)`
     and its jassert behavior (`end > start` required).
3. `melech_dsp_server` (or repo grep) — confirm there is **no existing
   limiter / envelope / lookahead component** under `shared/mdsp_dsp/`
   (expected: none — `dynamics/` is new).

Output RETRIEVAL LOG in the format from `PROMPTS/TEMPLATE_task.txt` before
producing any diff. If any field is missing, output ONLY a Retrieval Plan
and STOP.

────────────────────────────────────────
2. TASK TITLE
────────────────────────────────────────

Add `shared/mdsp_dsp/dynamics/` to HQ with three new components
(`LookaheadDelay`, `PeakDetector`, `LimiterEnvelope`). Wire them into the
MasterLimiter product repo as a single-band, sample-peak, lookahead limiter
(ADR-0004 Stages 1+3). Fix the `lookahead_ms` param assertion. Upgrade the
bench driver to target a specified GR depth by input-gain iteration. Pass
Slice 3's bench gate at 3 dB GR.

True-peak / oversampling (Stage 6) is Slice 4 — not in scope here.
Transient/sustain split (Stage 2) is Slice 5 — not in scope here.

────────────────────────────────────────
3. CONTEXT
────────────────────────────────────────

- Architecture: `melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md`
  (Stages 1 and 3 of the topology table).
- Pass criteria for this slice: `MasterLimiter/docs/SPEC.md` §5 at **3 dB GR**.
- Bench (the gate keeper): `melechdsp-hq/shared/dsp_bench/`.
- Existing snapshot pattern to mirror:
  `shared/mdsp_dsp/include/mdsp_dsp/loudness/LoudnessAnalyzer.h` lines 51–58.

Decisions already locked (do not relitigate):

| # | Decision |
|---|----------|
| Q3-1 | Lookahead truly fixed at 5 ms in v1. Param exists but is functionally read-only this slice. |
| Q3-2 | Release shape: **two cascaded one-pole low-passes**. Single `release_ms` drives both. |
| Q3-3 | Attack: **raised-cosine** ramp over the lookahead window, backward-propagated from each peak. |
| Q3-4 | Stereo detection: **hardcoded 100% stereo link** (mono peak = `max(|L|,|R|)`). `stereo_link_pct` / `ms_link_pct` params stay inert. |
| Q3-5 | New components live under `shared/mdsp_dsp/dynamics/`. |
| Q3-6 | Reported latency = lookahead samples; envelope applied to delayed audio. |
| Q3-7 | Publish `std::atomic<float> currentGrDb_` in processor. UI consumption deferred to Slice 8. |
| Q3-8 | Bench: measured-iterate input-gain search to hit target GR. |
| Q3-9 | Slice 3 gate uses SPEC §5 thresholds at the 3 dB GR column. |

────────────────────────────────────────
4. SCOPE — files you may CREATE / MODIFY
────────────────────────────────────────

### HQ (`melechdsp-hq/`)

**CREATE:**
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LookaheadDelay.h`
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/PeakDetector.h`
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h`
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`

**MODIFY:**
- `shared/mdsp_dsp/CMakeLists.txt`            (register new src file)

### Product repo (`MasterLimiter/`)

**MODIFY:**
- `Source/PluginProcessor.h`                  (members, snapshot atomic)
- `Source/PluginProcessor.cpp`                (prepareToPlay, processBlock, latency)
- `Source/parameters/Parameters.cpp`          (lookahead_ms range fix only)

### Bench (`melechdsp-hq/`)

**MODIFY:**
- `shared/dsp_bench/bench.py`                  (per-GR iterative input-gain calibration loop)
- `shared/dsp_bench/drivers/master_limiter.py` (real `params_for_gr_db`, ceiling stays at -1 dBFS)
- `shared/dsp_bench/criteria.py`               (add SLICE_03 criteria + evaluator)
- `shared/dsp_bench/measure.py`                (add `measured_gr_db(dry, wet, sr, latency)` helper)

NON-GOALS in this slice:
- NO true-peak detection. NO oversampling. NO ceiling-mode switching beyond
  reading the param to select sample-peak path (always sample-peak in v1
  until Slice 4).
- NO transient/sustain split. NO multiband.
- NO M/S link handling. Param stays inert.
- NO meter UI. NO `mdsp_ui` changes.
- NO new APVTS parameters. NO renames or retypes of existing IDs.
- NO changes to `Source/PluginEditor.{h,cpp}` or `Source/ui/MainView.{h,cpp}`.
- NO modifications to `mdsp_core`, `mdsp_ui`, `smoke_test`, or any HQ module
  other than `mdsp_dsp` and `dsp_bench`.

────────────────────────────────────────
5. COMPONENT SPECS
────────────────────────────────────────

### 5.1 `mdsp_dsp::LookaheadDelay<FloatType>` (header-only)

Template circular delay line. RT-safe. No allocation outside `prepare`.

```cpp
template <typename FloatType>
class LookaheadDelay {
public:
    LookaheadDelay() = default;

    // numChannels and maxDelaySamples allocate the internal buffer.
    void prepare (int numChannels, int maxDelaySamples);

    // Set the active delay in samples. Must be <= maxDelaySamples.
    // RT-safe; just updates an index.
    void setDelaySamples (int delaySamples);

    int getDelaySamples() const noexcept;
    int getMaxDelaySamples() const noexcept;

    void reset() noexcept;  // zeros buffer

    // Per-sample push/pop. write returns the delayed sample.
    FloatType pushPop (int channel, FloatType in) noexcept;
};
```

Implementation: stereo storage as `std::vector<std::vector<FloatType>>`
allocated in `prepare`. Read/write indices managed per channel or shared
(implementer choice; document). No JUCE dependency strictly required, but
you may use `juce::AudioBuffer<FloatType>` internally if it simplifies the
code — must remain RT-safe.

### 5.2 `mdsp_dsp::PeakDetector` (header-only)

Sample-peak across stereo, returns instantaneous absolute peak.

```cpp
class PeakDetector {
public:
    PeakDetector() = default;
    void prepare (int numChannels);
    void reset() noexcept;

    // Stereo (or mono) sample-peak: returns max(|L|, |R|, ...).
    // RT-safe scalar function.
    float process (const float* const* channelPtrs, int numChannels, int sampleIndex) const noexcept;
};
```

No state needed for sample-peak; `prepare`/`reset` are no-ops. Kept as a
class for symmetry with the future `TruePeakDetector` (Slice 4) which will
have state (oversampler delay, filter taps).

### 5.3 `mdsp_dsp::LimiterEnvelope`

The heart of this slice. Single-band, sample-peak, lookahead limiter envelope.

**Header (`include/mdsp_dsp/dynamics/LimiterEnvelope.h`):**

```cpp
class LimiterEnvelope {
public:
    LimiterEnvelope();
    ~LimiterEnvelope() = default;

    struct Spec {
        double sampleRate     = 48000.0;
        int    lookaheadSamples = 240;   // matches reported plugin latency
        int    maxBlockSize     = 1024;
    };

    void prepare (const Spec& spec);
    void reset() noexcept;

    // RT-safe setters. May be called from audio thread.
    void setThresholdLinear (float thresholdLin) noexcept;
    void setReleaseMs (float releaseMs) noexcept;

    // Block processing:
    //   peakIn[i]    = sample-peak (>= 0) at sample i, undelayed (i.e. detection bus).
    //   gainOut[i]   = linear gain to apply to the *delayed* audio at sample i,
    //                  always in (0, 1]. 1.0 = no reduction.
    // Block size must be <= maxBlockSize.
    void process (const float* peakIn, float* gainOut, int numSamples) noexcept;

    // Snapshot for caller: max gain reduction in this block, in dB (>= 0).
    float getLastBlockMaxGrDb() const noexcept;
};
```

**Algorithm (`src/dynamics/LimiterEnvelope.cpp`):**

Use the standard lookahead-limiter algorithm:

1. For each input sample `i`, compute the **required linear gain** to bring
   the peak to threshold:
   ```
   required = (peakIn[i] > thresholdLin) ? (thresholdLin / peakIn[i]) : 1.0f
   ```
   `required ∈ (0, 1]`.

2. Maintain an internal "envelope" buffer of size `lookaheadSamples + maxBlockSize`
   (preallocated in `prepare`). It holds the gain that will be applied to
   the *delayed* audio.

3. **Backward-propagated raised-cosine attack.** When a new `required[i]` is
   smaller than any of the past `lookaheadSamples` envelope values, walk
   backward from `i` and clamp the envelope down along a raised-cosine
   profile that reaches `required[i]` exactly at sample `i`. Precompute the
   raised-cosine attack table in `prepare`:
   ```
   for k in 0..lookaheadSamples-1:
       attack_table[k] = 0.5 * (1 - cos(pi * (k + 1) / lookaheadSamples))
       // 0 at k=0 ramping smoothly to 1 at k=lookaheadSamples-1
   ```
   Use `attack_table` to interpolate between the past envelope value at
   `i - lookaheadSamples` (the "anchor") and `required[i]`. For each
   `k ∈ [1, lookaheadSamples]`:
   ```
   tentative = anchor + (required[i] - anchor) * attack_table[lookaheadSamples - k]
   env[i - k] = min(env[i - k], tentative)
   ```
   `min` is what makes it backward-propagating: a deeper future peak
   overrides any previously planned smoother envelope.

4. **Forward-propagated cascaded release.** After backward-propagation, the
   *output* sample at delay tail (envelope at the tail-most index of the
   buffer that has now "aged" past `lookaheadSamples`) is forward-smoothed
   when the envelope is rising back toward 1.0 (release direction):
   ```
   alpha = exp(-1 / (release_seconds * sampleRate))
   // Two cascaded one-poles:
   stage1 = alpha * stage1_prev + (1 - alpha) * env_at_tail
   stage2 = alpha * stage2_prev + (1 - alpha) * stage1
   gainOut[i] = stage2
   ```
   On the **falling** edge (attack), the envelope is already shaped by the
   raised cosine — apply it directly without release smoothing (or with a
   "minimum" operator that never lets the cascaded output rise faster than
   the env, but always lets it fall instantly to follow the attack ramp).

5. Track `lastBlockMaxGrDb_ = max over block of 20*log10(1 / max(gainOut[i], 1e-12))`.

Implementation notes:
- All buffers preallocated in `prepare`. No `std::vector::resize` in `process`.
- Threshold and release coefficients use `Smoother`-style atomic targets,
  or simple `std::atomic<float>` with per-sample (or per-block) linear
  interpolation. Choose per-block for simplicity in Slice 3.
- Reset clears all state, sets stages to 1.0 and env buffer to 1.0.

### 5.4 CMakeLists.txt update

Add `src/dynamics/LimiterEnvelope.cpp` to the `add_library(mdsp_dsp STATIC ...)`
target_sources block. No other CMake changes.

────────────────────────────────────────
6. PRODUCT REPO — `Source/PluginProcessor.{h,cpp}`
────────────────────────────────────────

### Header additions

In `PluginProcessor.h`, in private members:

```cpp
mdsp_dsp::LookaheadDelay<float> lookahead_;
mdsp_dsp::PeakDetector          peakDetector_;
mdsp_dsp::LimiterEnvelope       envelope_;

juce::AudioBuffer<float>        peakBuf_;       // mono peak-stream, preallocated
juce::AudioBuffer<float>        gainBuf_;       // mono gain-stream, preallocated

static constexpr int kLookaheadSamplesAt48k = 240;  // 5 ms @ 48 kHz

// Snapshot for future UI (Slice 8). Updated once per block.
std::atomic<float> currentGrDb_ { 0.0f };

// Cached APVTS parameter handles (raw pointers; safe after APVTS construction).
juce::AudioParameterFloat*  inputGainDb_   = nullptr;
juce::AudioParameterFloat*  ceilingDb_     = nullptr;
juce::AudioParameterFloat*  releaseMs_     = nullptr;
juce::AudioParameterFloat*  lookaheadMs_   = nullptr;
juce::AudioParameterChoice* ceilingMode_   = nullptr;
```

Public accessor:

```cpp
float getCurrentGrDb() const noexcept { return currentGrDb_.load(std::memory_order_relaxed); }
```

### prepareToPlay

```cpp
const int lookaheadSamples = static_cast<int>(std::round(5.0e-3 * sampleRate));
lookahead_.prepare (2, lookaheadSamples);
lookahead_.setDelaySamples (lookaheadSamples);
peakDetector_.prepare (2);

mdsp_dsp::LimiterEnvelope::Spec spec;
spec.sampleRate       = sampleRate;
spec.lookaheadSamples = lookaheadSamples;
spec.maxBlockSize     = samplesPerBlock;
envelope_.prepare (spec);

peakBuf_.setSize (1, samplesPerBlock, false, true, true);  // preallocated, no reallocation in process
gainBuf_.setSize (1, samplesPerBlock, false, true, true);

setLatencySamples (lookaheadSamples);
```

### processBlock

```cpp
juce::ScopedNoDenormals noDenormals;
juce::ignoreUnused (midi);

const int n = buffer.getNumSamples();
if (n <= 0) return;

// 1. Apply input gain (smooth-ish: per-block linear ramp from previous to current).
const float inGainLin = juce::Decibels::decibelsToGain (inputGainDb_->get());
for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    buffer.applyGain (ch, 0, n, inGainLin);

// 2. Push input into the detection bus AND the lookahead delay.
//    Compute mono peak per sample (undelayed).
auto* peak = peakBuf_.getWritePointer (0);
const float* const channelPtrs[2] = { buffer.getReadPointer (0), buffer.getReadPointer (1) };
for (int i = 0; i < n; ++i)
    peak[i] = peakDetector_.process (channelPtrs, 2, i);

// 3. Update envelope params (per-block).
const float thresholdLin = juce::Decibels::decibelsToGain (ceilingDb_->get());
envelope_.setThresholdLinear (thresholdLin);
envelope_.setReleaseMs       (releaseMs_->get());

// 4. Run envelope.
auto* gain = gainBuf_.getWritePointer (0);
envelope_.process (peak, gain, n);

// 5. Apply envelope to *delayed* audio.
for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
{
    auto* d = buffer.getWritePointer (ch);
    for (int i = 0; i < n; ++i)
    {
        const float delayed = lookahead_.pushPop (ch, d[i]);
        d[i] = delayed * gain[i];
    }
}

// 6. Publish GR snapshot for the block.
currentGrDb_.store (envelope_.getLastBlockMaxGrDb(), std::memory_order_relaxed);
```

### Latency

`getLatencySamples()` reflects the value set by `setLatencySamples(lookaheadSamples)`
during `prepareToPlay`. No override needed (JUCE returns the cached value).

### Cached param handle initialisation

After APVTS construction, cache pointers:

```cpp
inputGainDb_ = dynamic_cast<juce::AudioParameterFloat*> (apvts_.getParameter (juce::String (param::input_gain_db)));
// ...same for the others...
jassert (inputGainDb_ && ceilingDb_ && releaseMs_ && lookaheadMs_ && ceilingMode_);
```

────────────────────────────────────────
7. PRODUCT REPO — `Source/parameters/Parameters.cpp` (lookahead_ms fix)
────────────────────────────────────────

**Replace** the lambda-NormalisableRange block for `lookahead_ms` with a
clean range. Parameter ID and version hint stay frozen.

```cpp
layout.add (std::make_unique<AudioParameterFloat> (
    pid (lookahead_ms, 1),
    "Lookahead",
    NormalisableRange<float> (4.0f, 6.0f, 0.01f),
    5.0f,
    AudioParameterFloatAttributes().withLabel ("ms")));
```

Nominal range is 4–6 ms with default 5.0. The processor **always uses 5 ms
in v1** regardless of the param value (it reads the param but ignores it
for delay computation). When a future slice exposes variable lookahead,
the param's existing range carries forward; no ID migration needed.

No other parameter changes in this slice.

────────────────────────────────────────
8. BENCH UPGRADES
────────────────────────────────────────

### 8.1 `measure.py::measured_gr_db(dry, wet, sample_rate, latency_samples) -> float`

Returns GR in dB. Approach: gain-match dry to wet over a steady-state
window (skip the first 200 ms post-latency), then compute
`20*log10(rms_dry / rms_wet_after_attenuation)` — i.e. the average
attenuation the limiter applied. Robust on pink noise and steady signals.

For impulsive signals (drum loop, transient_train) this is less precise;
use peak-tracking max GR over the block as a fallback. Document the
two-mode behavior in the docstring.

### 8.2 `drivers/master_limiter.py::params_for_gr_db(target_gr_db)`

Return a starting param set with `ceiling_db = -1.0` and an *initial guess*
for `input_gain_db`. Initial guess heuristic (pink noise at −20 LUFS,
peak ≈ −8 dBFS):

```
initial_input_gain_db = target_gr_db + 7.0
```

Iteration is the bench's job, not the driver's (see 8.3).

### 8.3 `bench.py` iterative calibration loop

For every (signal, target_gr_db) pair where `target_gr_db > 0`:

1. Render once with the driver's initial param set.
2. Measure GR with `measured_gr_db(...)`.
3. If `|measured - target| <= 0.2 dB`, accept this render.
4. Else adjust `input_gain_db += (target - measured)` and re-render.
5. Cap at **5 iterations**. If still not within tolerance, mark this
   signal/GR pair as `calibration_failed` (but continue measuring the
   other criteria — they still produce useful diagnostic data).

Add a `"calibration"` block to `results.json` showing iterations used and
final input gain for each pair.

### 8.4 `criteria.py::SLICE_03_*`

Encode SPEC §5 absolute thresholds at the 3 dB GR column:

```python
SLICE_03_3DB = {
    "null_residual_kweighted_dbfs": {
        "pink_noise_60s":     -60.0,
        "drum_loop_dense":    -52.0,
        "full_mix_dense":     -60.0,
        "full_mix_dynamic":   -60.0,
        "vocal_solo":         -58.0,
    },
    "thd_plus_n_pct_pink_max":     0.10,
    "imd_smpte_pct_max":           0.08,
    "transient_crest_delta_db_min": -0.6,
    "true_peak_overs_max_sp":      0,    # sample-peak ceiling −1 dBFS; TP comes in Slice 4
    "latency_match_samples":       1,    # ±1 sample tolerance
    "calibration_failures_max":    0,
}
```

Add `evaluate_slice03(metrics, calibration)` that returns pass/fail rows
analogous to Slice 2's evaluator. Signals not present in the corpus are
skipped (warning row, not failure).

────────────────────────────────────────
9. BUILD & SELF-VERIFICATION (your responsibility)
────────────────────────────────────────

### Build both repos

```bash
# HQ first (mdsp_dsp library)
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
# (no top-level build target needed; mdsp_dsp builds via the product repo)

# Product repo
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
```

Both must produce zero new warnings from files in §4 scope.

### Bench self-test against the new limiter

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
./bench.py --driver master_limiter --slice 3 \
           --output-dir runs/SLICE03_SELFTEST
```

Expected outcome at 3 dB GR on synthetic-only corpus (no user-supplied
real-world WAVs at slice time):

| Signal              | Null K-w     | THD+N         | IMD         | Crest Δ      | TP overs (SP) |
|---------------------|--------------|---------------|-------------|--------------|---------------|
| pink_noise_60s      | ≤ −60 dBFS   | ≤ +0.10 %     | —           | —            | 0             |
| sine_sweep_log_30s  | (diagnostic) | —             | —           | —            | 0             |
| imd_smpte           | (diagnostic) | —             | ≤ +0.08 %   | —            | 0             |
| transient_train     | (diagnostic) | —             | —           | ≥ −0.6 dB    | 0             |
| dirac_impulse       | (diagnostic) | —             | —           | —            | (excluded)    |

Latency reported must equal `round(5e-3 * sample_rate)` samples ±1.

────────────────────────────────────────
10. GATE CHECKLIST (architect verifies)
────────────────────────────────────────

- [ ] HQ builds clean; new files match §4 exactly, no others touched.
- [ ] Product repo builds clean; new files match §4 exactly.
- [ ] No JUCE assertion spam during plugin scan (lookahead_ms fixed).
- [ ] Bench self-test PASSES the §8.4 SLICE_03_3DB criteria (synthetic corpus).
- [ ] Iterative calibration converges within ≤ 5 iterations for pink noise.
- [ ] Reported latency matches measured latency ±1 sample.
- [ ] `getCurrentGrDb()` returns a finite, non-negative value reflecting the
      block's max GR (verified by adding a 1-line `juce::Logger::writeToLog`
      in debug only — REMOVE before submitting).
- [ ] Audition in Live: load plugin, push pink noise at +10 dB input gain,
      should hear smooth limiting with no clicks/zipper, ~3 dB GR.

────────────────────────────────────────
11. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. RETRIEVAL LOG (§1 format).
2. File list with one-line purpose each — split by repo.
3. Build summary lines for HQ and product repo (warnings/errors).
4. Bench invocation + final stdout line ("PASS X/Y" or "FAIL ...").
5. Pasted `results.md` from the bench self-test.
6. Self-check against §10 gate — "verified" / "needs audition" / "blocked".
7. Open questions.

DO NOT skip the bench self-test. The gate hinges on it.

────────────────────────────────────────
12. POST-SLICE NOTES (architect only)
────────────────────────────────────────

When this slice passes:
- Architect copies bench `results.{json,md}` + `plots/` into
  `MasterLimiter/docs/bench/SLICE_03/<timestamp>/`.
- Architect commits HQ (mdsp_dsp + dsp_bench) and product repo.
- Slice 4 interview begins: true-peak detector + 4× polyphase oversampler
  on the output ceiling stage.
