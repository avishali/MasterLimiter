# Cursor Task — Slice 4: True-Peak Detector + 4× Polyphase Oversampler

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Two repos this slice.** New `mdsp_dsp/dynamics/` components in HQ
> (`TruePeakDetector`, `IspTrimStage`); product repo wires them up behind
> the existing `ceiling_mode` parameter. Bench gains a Slice 4 criteria block.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE (MANDATORY)
────────────────────────────────────────

Obey `melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- ONE task = ONE focused diff. Stay strictly within §4 file scope.
- RT-safety: no allocations in `processBlock`, no allocations inside
  `IspTrimStage::process` or `TruePeakDetector::process`. All buffers
  preallocated in `prepare()`. `juce::dsp::Oversampling` allocates inside
  `initProcessing`, so call that from `prepareToPlay` only.
- Engine/UI boundary preserved. No new APVTS parameters (`ceiling_mode`
  already exists — frozen ID).
- Parameter IDs frozen. No renames, no retypes.
- Minimal diff.

────────────────────────────────────────
1. TRINITY PROTOCOL (lightweight)
────────────────────────────────────────

Before any code:

1. `melech_internal_server` (or repo grep fallback) — confirm:
   - `shared/mdsp_dsp/dynamics/` already contains the Slice 3 components
     (`LookaheadDelay`, `PeakDetector`, `LimiterEnvelope`). Slice 4 adds
     two new files there, mirroring the existing pattern.
   - `shared/mdsp_dsp/CMakeLists.txt` needs a new src entry for
     `IspTrimStage.cpp`.
2. `juce_api_server` — verify for JUCE 8:
   - `juce::dsp::Oversampling<float>` constructor, `addOversamplingStage`,
     `initProcessing`, `processSamplesUp`, `processSamplesDown`,
     `getLatencyInSamples`. We want
     `filterHalfBandPolyphaseIIR` for low latency (≈3–6 base-rate samples
     vs ≈32 for `filterHalfBandFIREquiripple`).
   - `AudioProcessor::setLatencySamples` is safe to call from the audio
     thread (uses internal atomic; host PDC updates on next callback).
3. `melech_dsp_server` — confirm no existing TP detector / oversampler
   utility exists in `mdsp_dsp` (expected: none — these are new).

Output the RETRIEVAL LOG (format from `TEMPLATE_task.txt`). If MCP servers
are unavailable, repo-grep fallback is acceptable.

────────────────────────────────────────
2. TASK TITLE
────────────────────────────────────────

Add `mdsp_dsp::TruePeakDetector` and `mdsp_dsp::IspTrimStage` (per ADR-0004
Stage 6) on top of `juce::dsp::Oversampling` 4× polyphase IIR. Wire them
into the MasterLimiter product as a post-envelope output stage activated
when `ceiling_mode == TruePeak`. Reported latency tracks the active mode.
Bench gets a Slice 4 criteria block + driver default of `ceiling_mode =
TruePeak`. Pass Slice 4's gate at 3 dB GR.

────────────────────────────────────────
3. CONTEXT (decisions locked, do not relitigate)
────────────────────────────────────────

| # | Decision |
|---|----------|
| Q4-1 | Oversampling factor: **4×** (two cascaded 2× halfband stages). |
| Q4-2 | OS placement: **post-envelope, dedicated stage**. Sample-peak limiter unchanged; TP stage sits after it. |
| Q4-3 | OS engine: **`juce::dsp::Oversampling` polyphase IIR halfband**. ~30 LOC of glue. Revisit only if aliasing fails the bench. |
| Q4-4 | ISP enforcement: **linear per-sample gain trim at OS rate** (no waveshaper). Snap-attack, single-pole release. Stays fully linear; Slice 6 saturator can replace this later for smoother sound. |
| Q4-5 | Mode switch SP↔TP: dynamic. Plugin calls `setLatencySamples` from `processBlock` when `ceiling_mode` parameter changes. |
| Q4-6 | Two new `mdsp_dsp` components: `TruePeakDetector` (measurement only, used by bench-side measure later if desired) and `IspTrimStage` (the actual ISP-killing stage in the audio path). Split keeps responsibilities clean. |
| Q4-7 | Bench: `SLICE_04_3DB_SINGLEBAND` with `true_peak_overs_max=0` etc.; driver defaults `ceiling_mode = TruePeak` for Slice 4 runs. |
| Q4-8 | Gate: bench TP overs = 0, aliasing ≤ −90 dBFS @ 96 kHz, null/THD regression ≤ Slice 3 levels. |
| Q4-9 | Audition: SP↔TP switch clean; TP mode tighter on transients; 0 TP overs on dense mix per Insight. |
| Q4-10 | Bundle bench cosmetic fix: per-row pass column should reflect the active criteria mask, not stale SPEC-final thresholds. |

References:
- `melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md` (Stage 6)
- `MasterLimiter/docs/SPEC.md` §5
- Slice 3 component pattern: `shared/mdsp_dsp/dynamics/LimiterEnvelope.{h,cpp}`

────────────────────────────────────────
4. SCOPE — files you may CREATE / MODIFY
────────────────────────────────────────

### HQ (`melechdsp-hq/`)

**CREATE:**
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/TruePeakDetector.h`
- `shared/mdsp_dsp/src/dynamics/TruePeakDetector.cpp`
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/IspTrimStage.h`
- `shared/mdsp_dsp/src/dynamics/IspTrimStage.cpp`

**MODIFY:**
- `shared/mdsp_dsp/CMakeLists.txt` (register `TruePeakDetector.cpp` and `IspTrimStage.cpp`)
- `shared/dsp_bench/criteria.py` (add `SLICE_04_3DB_SINGLEBAND` + `evaluate_slice04`)
- `shared/dsp_bench/bench.py` (Slice 4 branch in default GR depths, signal selection unchanged; route to `evaluate_slice04`; per-row "pass" column reflects active criteria — cosmetic fix per Q4-10)
- `shared/dsp_bench/drivers/master_limiter.py` (default `ceiling_mode` becomes "TruePeak" when slice >= 4)

### Product repo (`MasterLimiter/`)

**MODIFY:**
- `Source/PluginProcessor.h` (add `oversampler_`, `ispTrim_`, cached `ceiling_mode` state, helper methods)
- `Source/PluginProcessor.cpp` (engage TP path conditionally; dynamic latency on mode change)

NON-GOALS in this slice:
- NO new APVTS parameters. Use existing `ceiling_mode`.
- NO saturator / waveshaper. Linear trim only.
- NO changes to `LimiterEnvelope`, `LookaheadDelay`, `PeakDetector`, `Parameters.{h,cpp}`, or UI.
- NO multiband, no transient/sustain split (Slice 5).
- NO new bench metrics beyond TP / aliasing / latency-on-mode-switch coverage.

────────────────────────────────────────
5. COMPONENT SPECS
────────────────────────────────────────

### 5.1 `mdsp_dsp::TruePeakDetector` (header + impl)

Measurement-only helper. Wraps a 4× polyphase IIR `juce::dsp::Oversampling`
chain configured for measurement (input → upsample → max-abs → no
downsample needed). Used by bench / future telemetry; NOT placed in the
audio path by the processor.

```cpp
// TruePeakDetector.h
class TruePeakDetector {
public:
    TruePeakDetector();
    ~TruePeakDetector() = default;

    struct Spec {
        int numChannels    = 2;
        int maxBlockSize   = 1024;
        int oversampling   = 4;   // fixed in v1
    };

    void prepare (const Spec& spec);
    void reset() noexcept;

    // Process a base-rate buffer and return max true-peak (linear) across all
    // channels for this block. Buffer is read-only — no in-place modification.
    float measure (const juce::AudioBuffer<float>& buffer) noexcept;

    int  getOversamplingFactor() const noexcept { return 4; }
    int  getLatencyInSamples()   const noexcept; // base-rate group delay
};
```

Implementation: two cascaded `filterHalfBandPolyphaseIIR` stages, normalised
transition width default, stereo. Uses a private `juce::AudioBuffer<float>`
preallocated in `prepare` for the OS-rate working data. `measure` is
RT-safe.

### 5.2 `mdsp_dsp::IspTrimStage` (header + impl)

The actual audio-path component. Takes a base-rate stereo buffer
*already limited at sample-peak by the main envelope*, applies 4×
oversampled ISP trim, returns the base-rate buffer with TP ≤ ceiling.

```cpp
// IspTrimStage.h
class IspTrimStage {
public:
    IspTrimStage();
    ~IspTrimStage() = default;

    struct Spec {
        double sampleRate  = 48000.0;
        int    numChannels = 2;
        int    maxBlockSize = 1024;
    };

    void prepare (const Spec& spec);
    void reset() noexcept;

    // Caller sets ceiling in linear (e.g. juce::Decibels::decibelsToGain(-1.0)).
    void setCeilingLinear (float ceilingLin) noexcept;

    // In-place processing. Buffer is replaced with TP-compliant audio.
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int   getLatencyInSamples() const noexcept; // OS group delay (~3–6)
    float getLastBlockMaxTpDbReduction() const noexcept; // for snapshots
};
```

Algorithm in `process`:

1. Wrap `buffer` in `juce::dsp::AudioBlock<float>`.
2. Upsample 4× via the internal `juce::dsp::Oversampling` instance →
   `osBlock = oversampler_.processSamplesUp(block)`.
3. Per-channel OS-rate loop:
   ```
   env = envState_[ch]
   for each os sample s:
       absS = |s|
       target = (absS > ceiling_) ? (ceiling_ / absS) : 1.0
       if (target < env)
           env = target            // snap attack
       else
           env = relA * env + (1 - relA) * target  // single-pole release
       s *= env
   envState_[ch] = env
   ```
   `relA` = `exp(-1.0 / (relaseSecondsAtOS * osSampleRate))`. Use a fixed
   short release of **0.5 ms at OS rate** in v1 (avoids audible modulation
   of HF content while keeping ISP transients short).
4. Downsample 4× via `oversampler_.processSamplesDown(block)`. Buffer now
   holds TP-compliant base-rate audio.
5. Track `lastBlockMaxTpDbReduction_` = `max over block of 20*log10(1/min_env)`
   for snapshots.

RT-safety:
- `juce::dsp::Oversampling::initProcessing(maxBlockSize)` allocates internally —
  call ONLY from `prepare()`. `processSamplesUp/Down` are RT-safe.
- `envState_` preallocated per channel in `prepare`.

### 5.3 CMakeLists.txt update

Add `src/dynamics/TruePeakDetector.cpp` and `src/dynamics/IspTrimStage.cpp`
to `add_library(mdsp_dsp STATIC ...)`.

────────────────────────────────────────
6. PRODUCT REPO — `Source/PluginProcessor.{h,cpp}` changes
────────────────────────────────────────

### Header additions

```cpp
mdsp_dsp::IspTrimStage ispTrim_;
int     baseLatencySamples_   = 0;      // lookahead samples
int     osLatencySamples_     = 0;      // ispTrim_.getLatencyInSamples()
int     cachedCeilingMode_    = -1;     // forces a setLatencySamples on first block
std::atomic<float> currentTpTrimDb_ { 0.0f };  // snapshot for future UI
```

Public accessor:
```cpp
float getCurrentTpTrimDb() const noexcept { return currentTpTrimDb_.load(std::memory_order_relaxed); }
```

### prepareToPlay additions

Right after the existing `envelope_.prepare(spec)` block, before
`setLatencySamples(...)`:

```cpp
mdsp_dsp::IspTrimStage::Spec tspec;
tspec.sampleRate  = sampleRate;
tspec.numChannels = 2;
tspec.maxBlockSize = samplesPerBlock;
ispTrim_.prepare (tspec);

baseLatencySamples_ = lookaheadSamples;
osLatencySamples_   = ispTrim_.getLatencyInSamples();

// Initial cached mode forces a latency update on first processBlock.
cachedCeilingMode_  = -1;
```

Replace the existing `setLatencySamples(lookaheadSamples);` call: leave it
as-is (sets the base latency for SP mode), but the actual reported latency
is updated dynamically in `processBlock` once we read the current
`ceiling_mode`.

### processBlock additions (immediately after envelope is applied)

After the existing block where envelope is applied to delayed audio and
`currentGrDb_` is stored, add:

```cpp
// Read ceiling_mode (0 = SamplePeak, 1 = TruePeak — verify match with
// the AudioParameterChoice declaration order in Parameters.cpp).
const int modeIdx = ceilingMode_->getIndex();

if (modeIdx != cachedCeilingMode_)
{
    const int newLatency = (modeIdx == 1) ? (baseLatencySamples_ + osLatencySamples_)
                                          :  baseLatencySamples_;
    setLatencySamples (newLatency);
    ispTrim_.reset();                   // clean state on mode switch
    cachedCeilingMode_ = modeIdx;
}

if (modeIdx == 1)
{
    ispTrim_.setCeilingLinear (thresholdLin); // same ceiling already computed
    ispTrim_.process (buffer);
    currentTpTrimDb_.store (ispTrim_.getLastBlockMaxTpDbReduction(),
                            std::memory_order_relaxed);
}
else
{
    currentTpTrimDb_.store (0.0f, std::memory_order_relaxed);
}
```

Notes:
- The `setLatencySamples` call from `processBlock` is RT-safe per JUCE
  internal atomics — verified in Trinity. JUCE hosts pick up the change
  on their next PDC scan.
- Mode-switch `reset()` on the OS chain clears half-band filter state to
  prevent the audible click that would otherwise come from a discontinuous
  delay path.

### ceilingMode_ pointer caching

In the constructor or `prepareToPlay`, cache the parameter pointer
alongside the existing input/ceiling/release pointers:

```cpp
ceilingMode_ = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("ceiling_mode"));
jassert (ceilingMode_);
```

Add `juce::AudioParameterChoice* ceilingMode_ = nullptr;` to the header
member list.

────────────────────────────────────────
7. BENCH UPGRADES
────────────────────────────────────────

### 7.1 `criteria.py`

Add a new block:

```python
SLICE_04_3DB_SINGLEBAND = dict(SLICE_03_3DB_SINGLEBAND, **{
    # Slice 4 enables TP enforcement; TP overs must now be zero.
    "true_peak_overs_max":          0,
    # Sample-peak overs may still appear sporadically (envelope's small TP
    # trim is at OS rate, not base rate — base-rate samples can still
    # bump the ceiling by tiny fractions). Keep tolerant.
    "sample_peak_overs_max":        200,
    # Aliasing at 96 kHz render: oversampling chain rejection must be tight.
    "aliasing_residual_db_max":    -90.0,
    # Latency expectations are MODE-dependent now; bench reads the reported
    # latency post-mode-set rather than hard-coding 240.
    "latency_reported_match_samples": 1,
    "latency_alignment_lag_max":      1,
})
```

Add `evaluate_slice04(metrics, calibration)` modeled on `evaluate_slice03`:
same per-signal null map, same calibration check, but `true_peak_overs_max`
threshold is now `0` and `aliasing_residual_db` is gated.

### 7.2 `bench.py`

- Default GR depths for slice 4: `[3.0]` (same as slice 3 — we're not
  re-baselining loudness, only TP enforcement).
- Route to `evaluate_slice04` when `args.slice == 4`.
- **Per-row "pass" column fix (Q4-10):** the table's `pass` flag for
  `true_peak_overs` should look up the active criterion (0 for slice 4,
  not-gated for slice 3) rather than the hard-coded `tp == 0` check.
  Concretely: pass the active criteria dict into `_compute_row_pass` (or
  inline the logic) so dirac/transient/sweep rows mark correctly per slice.

### 7.3 `drivers/master_limiter.py`

When `slice >= 4`, default `ceiling_mode = "TruePeak"`. Slice 3 still uses
`SamplePeak`. Add a slice-number argument or check via `params_for_gr_db`
(your call — keep it simple).

────────────────────────────────────────
8. BUILD & VERIFICATION
────────────────────────────────────────

### Build both repos

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```

Zero new warnings from §4 files.

### Bench self-test against Slice 4

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
python bench.py --driver master_limiter --slice 4 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE04_SELFTEST
```

### Cross-check: re-run slice 3 to confirm no regression

```bash
python bench.py --driver master_limiter --slice 3 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE04_REGRESSION_S3
```

Slice 3 path must still PASS — TP mode is selectable, SP mode behavior is
unchanged.

────────────────────────────────────────
9. GATE CHECKLIST
────────────────────────────────────────

- [ ] HQ builds clean; only §4 files touched.
- [ ] Product builds clean; only §4 files touched.
- [ ] Slice 4 bench: `true_peak_overs_max` aggregate = **0** on the gated
      signal set (pink + sweep + IMD).
- [ ] Aliasing residual ≥ 20 kHz on 96 kHz render: ≤ **−90 dBFS**.
- [ ] Null residual K-weighted on pink at 3 dB GR: **no worse** than
      Slice 3 (≤ −25 LUFS, ideally within 1 dB of Slice 3's −31 LUFS).
- [ ] Noise residual pct on pink: no worse than Slice 3.
- [ ] IMD: no worse than Slice 3.
- [ ] `latency_reported_samples` in TP mode: equals `base + os_group_delay`
      (≈ 243–246 @ 48 kHz, exact value reported by JUCE's oversampling).
- [ ] `latency_alignment_lag_samples`: 0 (pedalboard still PDCs correctly).
- [ ] Calibration failures: 0.
- [ ] Slice 3 regression run still PASSES.
- [ ] Cosmetic Q4-10 fix landed: per-row pass column reflects active
      criteria mask (no false ❌ on TP overs in TP mode).

────────────────────────────────────────
10. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. RETRIEVAL LOG (§1 format).
2. File list created / modified, with one-line purpose each, split by repo.
3. Build summary lines (HQ + product, both Debug and Release).
4. Slice 4 bench invocation, exit code, stdout final line.
5. Pasted `results.md` from the Slice 4 run.
6. Slice 3 regression invocation, exit code, stdout final line, brief
   confirmation it still PASSES.
7. Self-check against §9 — "verified" / "needs audition" / "blocked".
8. Open questions.

DO NOT skip the Slice 3 regression run. Mode-switching is a real risk
vector; confirming SP behavior is unchanged is part of the gate.

────────────────────────────────────────
11. POST-SLICE NOTES (architect only)
────────────────────────────────────────

When this slice passes:
- Architect copies bench `results.{json,md}` + `plots/` into
  `MasterLimiter/docs/bench/SLICE_04/<timestamp>/`.
- Architect updates PROGRESS, commits HQ + product.
- Slice 5 interview begins: transient/sustain split (ADR-0005 to be
  written then). This is the slice that closes the loudness gap to
  Ozone IRC.
