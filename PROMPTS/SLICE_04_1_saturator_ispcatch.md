# Cursor Task — Slice 4.1: Replace linear ISP trim with soft-knee saturator

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Scope is tight: two files.** `IspTrimStage.cpp` (algorithm body) and
> `Parameters.cpp` (default mode flip). Plus an optional bench-side artifact
> reset. No headers, no public APIs, no other DSP, no new components.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- ONE task = ONE focused diff. Stay strictly within §4 file scope.
- RT-safety: no allocations in `IspTrimStage::process`. No new state members
  unless absolutely necessary (the algorithm replacement does not need any).
- Engine/UI boundary preserved.
- Minimal diff.

────────────────────────────────────────
1. WHY (read once, do not skim)
────────────────────────────────────────

The Slice 4 TP path passes `true_peak_overs = 0` but regresses null/IMD/
aliasing badly vs Slice 3 SP. Bench data:

| Metric | Slice 3 SP | Slice 4 TP (current) | Slice 3 target |
|---|---:|---:|---:|
| Pink null K-weighted | −31.3 LUFS | **−13.3 LUFS** | ≤ −25 |
| Pink noise residual % | 9.6 % | **41.0 %** | ≤ 12 |
| IMD | 0.081 % | **2.21 %** | ≤ 1.0 |
| Aliasing ≥ 20 kHz (96 k) | n/a | **−1.5 to −13.6 dB** | ≤ −90 |
| TP overs aggregate | n/a | 0 ✓ | 0 |

The bench is correct. The DSP is broken. The current algorithm — **linear
per-sample gain trim with snap-attack at OS rate** — produces broadband
AM sidebands from every gain-step discontinuity. The IIR halfband downsample
filter has finite stopband rejection, so the AM noise folds back into the
audible band as aliasing. The `kHeadroomLin = 0.888f` workaround makes the
gain modulation deeper, which worsens the artifact.

The architecturally-correct fix is **ADR-0004 Stage 4 (soft-knee saturator)**,
originally planned for Slice 6. Pulled forward here because the linear trim
is unshippable.

**Why a saturator is the right tool:**

1. Preserves waveform continuity — no gain-step discontinuities, no broadband
   AM noise from gain modulation.
2. Harmonics from waveshaping land at multiples of input frequency, mostly
   above 20 kHz at OS rate. The halfband filter rejects most of them on
   downsample.
3. With a tight knee (engaging only in the last ~0.5 dB before ceiling),
   the **bulk of the audio passes through bit-exact unchanged**. Sub-audible
   THD by construction.

────────────────────────────────────────
2. TRINITY (very light)
────────────────────────────────────────

Re-read before editing (no MCP needed):

- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/IspTrimStage.h` — public API,
  do NOT modify. Same `process()` signature carries over.
- `shared/mdsp_dsp/src/dynamics/IspTrimStage.cpp` — the file you replace.
- `MasterLimiter/Source/parameters/Parameters.cpp` — for the default flip
  (§5 below).

Output a brief retrieval note in your reply (files re-read).

────────────────────────────────────────
3. CORRECTED ALGORITHM
────────────────────────────────────────

### Buffer semantics

Same as before. `IspTrimStage::process(buffer)` takes a base-rate stereo
`juce::AudioBuffer<float>` post-envelope, returns it with true peak ≤ ceiling.

### prepare() — no changes required

`prepare()` already allocates the OS chain via
`juce::dsp::Oversampling::initProcessing`. Keep as-is. **Delete any
`kHeadroomLin` or analogous magic-number constant** introduced for the old
algorithm. Same for any per-channel envelope state (`envState_`) that was
exclusive to the snap-attack release approach — if it's no longer used,
remove it cleanly to keep the diff honest. (If the state was reused for
something else, leave it.)

### process() — replace with this exactly

```cpp
void IspTrimStage::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumSamples() <= 0)
        return;

    juce::dsp::AudioBlock<float> block (buffer);

    // 1. Upsample 4× to OS rate.
    auto osBlock = oversampler_.processSamplesUp (block);

    const int   nch     = (int) osBlock.getNumChannels();
    const int   nsOs    = (int) osBlock.getNumSamples();
    const float ceiling = ceilingLin_;
    const float kneeStart = 0.95f * ceiling;            // engage in last ~0.45 dB
    const float kneeRange = ceiling - kneeStart;
    const float invKneeRange = (kneeRange > 1.0e-12f) ? (1.0f / kneeRange) : 0.0f;

    float maxReductionDb = 0.0f;

    // 2. Soft-knee saturator at OS rate, per channel.
    //    Below kneeStart: bit-exact pass-through (no math at all).
    //    Above kneeStart: smooth asymptote toward ceiling.
    for (int ch = 0; ch < nch; ++ch)
    {
        auto* d = osBlock.getChannelPointer ((size_t) ch);
        for (int i = 0; i < nsOs; ++i)
        {
            const float x    = d[i];
            const float absX = std::fabs (x);
            if (absX <= kneeStart)
                continue;                                // unchanged

            // Smooth asymptotic approach: as absX → ∞, absY → ceiling.
            // dY/dX continuous at kneeStart (= 1.0), so no waveform kink.
            const float over = (absX - kneeStart) * invKneeRange;
            const float absY = kneeStart + kneeRange * (1.0f - std::exp (-over));
            d[i] = std::copysign (absY, x);

            const float redDb = 20.0f * std::log10 (std::max (1.0e-12f, absX / absY));
            if (redDb > maxReductionDb)
                maxReductionDb = redDb;
        }
    }

    lastBlockMaxTpDbReduction_ = maxReductionDb;

    // 3. Downsample 4× back to base rate, in-place into `block`.
    oversampler_.processSamplesDown (block);
}
```

### Key properties of this algorithm

- For any sample with `|x| ≤ 0.95 · ceiling`: **literally untouched**, no
  multiply, no math. The bulk of the audio at 3 dB GR sits well below this.
- For `|x| > 0.95 · ceiling`: smooth polynomial-style asymptote. As `|x|`
  grows, `|y|` approaches `ceiling` but never exceeds it. ISP eliminated.
- `dY/dX = 1` at `kneeStart` and decreases continuously above it — no kink
  in the waveform's derivative, no discontinuity, no broadband AM.
- Harmonic content from the asymptote is bounded and frequency-localised
  around the (rare) saturated samples. Most ends up above OS Nyquist /
  attenuated by the halfband.

### Reset() and state members

If your previous implementation had per-channel envelope state for the
old release-smoothing path, that state is no longer used. Remove it from
the header *only if cleanly removable*; if its removal would touch other
files or require API churn, leave it as dead code and add a one-line
comment saying it's unused as of Slice 4.1.

The new algorithm is **stateless across blocks** (apart from the OS chain's
internal halfband state, which `oversampler_.reset()` handles). `reset()`
just calls `oversampler_.reset()`. `lastBlockMaxTpDbReduction_` resets to 0.

### Header

`IspTrimStage.h` is **NOT in scope for modification**. The public API
(`prepare`, `reset`, `setCeilingLinear`, `process`, `getLatencyInSamples`,
`getLastBlockMaxTpDbReduction`) stays identical.

────────────────────────────────────────
4. SCOPE — files you may MODIFY
────────────────────────────────────────

- `shared/mdsp_dsp/src/dynamics/IspTrimStage.cpp` (algorithm replacement)
- `MasterLimiter/Source/parameters/Parameters.cpp` (§5 below: default mode)

If you find that removing dead state members forces a header change, STOP
and ask. Otherwise leave the header alone.

**Do not modify:** `IspTrimStage.h`, any other `dynamics/` component,
`PluginProcessor.{h,cpp}`, `host.py`, `bench.py`, `criteria.py`.

────────────────────────────────────────
5. PRODUCT REPO — `Parameters.cpp` default flip
────────────────────────────────────────

The `ceiling_mode` param currently defaults to index 1 (TruePeak). Flip
the default to **index 0 (SamplePeak)**.

Locate the `AudioParameterChoice` for `ceiling_mode` in `Parameters.cpp`
and change the default-index argument from `1` to `0`. **Do not** change
the choice list ordering — `SamplePeak` must stay at index 0 and
`TruePeak` at index 1. Parameter ID is frozen.

This is a single literal change. After this, the factory state opens in
SP mode; the user enables TP explicitly.

────────────────────────────────────────
6. BUILD & VERIFY
────────────────────────────────────────

### Build

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```

Zero new warnings from §4 files.

### Bench — Slice 4 TP-mode run

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
python bench.py --driver master_limiter --slice 4 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE04_1_AFTER_SATURATOR
```

### Bench — Slice 3 regression (SP unchanged)

```bash
python bench.py --driver master_limiter --slice 3 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE04_1_REGRESSION_S3
```

────────────────────────────────────────
7. EXPECTED OUTCOME (architect prediction)
────────────────────────────────────────

Slice 4 TP-mode run, at 3 dB GR on the synthetic corpus:

| Metric | Predicted | Threshold |
|---|---:|---:|
| `true_peak_overs_max` aggregate | 0 | 0 |
| `sample_peak_overs_max` aggregate | small (≤ 100) | ≤ 200 |
| `null_residual_kweighted_db` pink | between Slice 3's −31 and −20 LUFS | ≤ −25 |
| `noise_residual_pct` pink | close to Slice 3's 9.6 % (10–15 %) | ≤ 12 |
| `imd_smpte_pct` | well under 1 % (probably ≤ 0.5 %) | ≤ 1.0 |
| `aliasing_residual_db` | **substantially better** than the current −1.5 dB; expect somewhere between −20 and −60 dB | ≤ −90 |
| `latency_reported_samples` | 243 (or whatever JUCE reports) | match expected |
| `latency_alignment_lag_samples` | 0 or 1 | ≤ 1 |

**Two outcomes acceptable as gate pass:**

- (Best case) every threshold above is met → SLICE 4.1 PASS, slice closes.
- (Realistic case) all met **except aliasing** still > −90 dB → STOP and
  report. We may need to tighten the saturator (narrower knee, different
  shape) or revisit Q4-3 (FIR halfband). Don't loosen thresholds.

If any other regression appears (null worse than current, IMD worse, TP
overs > 0), STOP and report — it indicates the new algorithm has a bug.

Slice 3 regression run must continue to PASS unconditionally.

────────────────────────────────────────
8. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Brief retrieval note.
2. Diff for `IspTrimStage.cpp` (full new file content if cleaner than diff).
3. One-line note for the `Parameters.cpp` change (line number + before/after).
4. Build summary (HQ + product, both Debug and Release; warnings count).
5. Slice 4 bench: invocation, exit code, stdout final line.
6. Pasted full contents of `results.md` from the Slice 4 run.
7. Slice 3 regression: invocation, exit code, stdout final line.
8. Self-check vs §7 prediction table — value per row, pass/fail vs threshold.
9. Any open questions.

DO NOT iterate further if §7's aliasing criterion fails. Report the numbers
and STOP — architect will decide between tightening the knee or revisiting
the OS chain.
