# Cursor Task — Slice 3.1: Fix LimiterEnvelope indexing

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **One file in scope.** `melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`.
> Rationale: the Slice 3 envelope algorithm is correct in concept but the
> indexing is off by `la` samples and `historyPost_` is sourced from the wrong
> buffer. Bench is correct — DSP needs fixing.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Highlights:

- ONE file in scope. STOP if a fix requires touching another file.
- RT-safety: no allocations in `process()`. All buffers preallocated in
  `prepare()` — DO NOT change `prepare()`'s allocation logic unless §3
  explicitly requires it (it does not; allocations are already correct).
- Minimal diff. No new public methods, no logging, no `Smoother` changes.

────────────────────────────────────────
1. TRINITY PROTOCOL (lightweight — one file)
────────────────────────────────────────

Re-read these before editing:

- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h` (header is fine, do not modify)
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` (the file you will rewrite)
- `MasterLimiter/Source/PluginProcessor.cpp` — context: read how `processBlock`
  calls `envelope_.process(peak, gain, n)` and applies `gain[i]` to delayed audio
  (`lookahead_.pushPop`). DO NOT modify this file.

Output the retrieval log briefly (which files were re-read) before producing the diff.

────────────────────────────────────────
2. THE BUG (read in full — do not skim)
────────────────────────────────────────

### Bug 1 — Backward ramp is `la` samples too early

`peakIn[i]` is detected from input at time `i` (undelayed). The audio path is
delayed by `la`, so the audio that produced this peak **outputs** at time
`i + la`. Therefore:

- The envelope must reach `required = thresholdLin / peakIn[i]` at output
  index **`i + la`** (the actual peak's output time).
- The envelope ramps from `anchor` at output index **`i`** down to `required`
  at output index **`i + la`** — a forward-in-time ramp of length `la`.
- This translates to writing `ext_[i .. i+la]` (inclusive on both ends).

The current loop writes `ext_[i+la-1 .. i]` (k=1..la) — short by one slot,
and the `i+la` slot (the actual peak) is never constrained. Peaks slip
through unattenuated.

### Bug 2 — Anchor reads from the wrong region

`ext_` size is `la + n` with layout `[history | current]`. `anchor = ext_[i]`
reads from history for `i < la` and from "current" for `i ≥ la`, but neither
is the start-of-ramp slot for the peak at input index `i`. The anchor must be
the envelope value at output index `i` itself.

The fix is to flatten the indexing: `ext_[j]` = planned envelope at output
index `j` directly. No more "history vs current" sub-regions inside `ext_`.

### Bug 3 — `historyPost_` saves the smoothed `gainOut`, not pre-smoothing `ext_`

Lines 137–146 copy `gainOut[]` (after the cascaded one-pole release) into
`historyPost_`. Next block uses these as anchors. Cross-block ramps are
corrupted because the carried-in anchor is already partially released back
toward 1.0.

`historyPost_` must carry forward the **pre-smoothing planned envelope** —
specifically the `la` samples that extend beyond the current block (the
"future planning" the current block did because peaks near the end of the
block wrote to indices `> n - 1`).

────────────────────────────────────────
3. CORRECTED ALGORITHM (rewrite `process()` to this exactly)
────────────────────────────────────────

### Buffer semantics

`ext_` has size `(maxBlockSize_ + lookaheadSamples_)`. In each block:

- `ext_[0 .. la-1]` = carried-in from previous block (`historyPost_`).
  These represent the previous block's planning that reaches into our
  first `la` samples.
- `ext_[la .. n+la-1]` = fresh, initialized to `1.0f` at block start.
  These represent samples beyond the current block's first `la` — i.e.,
  output indices `la .. n+la-1`. (Yes, this includes some samples that
  will be in the next block: `ext_[n .. n+la-1]` will be saved as
  `historyPost_`.)

Wait, restate cleanly. `ext_[j]` for `j ∈ [0, n + la - 1]` directly represents
the planned envelope at output index `j` relative to the current block's start.
The block produces `gainOut[0 .. n-1]`. The trailing `ext_[n .. n+la-1]` is
"future planning" that becomes the next block's `ext_[0 .. la-1]`.

### prepare()

No changes needed except confirming sizes:

- `ext_` resized to `(maxBlockSize_ + lookaheadSamples_)`, initialized to `1.0f`.
- `historyPost_` size `lookaheadSamples_`, initialized to `1.0f`.
- `attackTable_` size `lookaheadSamples_` (unchanged: `attack_table[k] = 0.5 * (1 - cos(pi * (k+1) / la))` for `k = 0..la-1`).

If the existing `prepare()` body matches these (it currently does), do not modify it.
**Only modify `process()`, `reset()` if needed, and any private helpers.**

### reset()

Set all `ext_[]` and all `historyPost_[]` to `1.0f`. `stage1_ = stage2_ = 1.0f`.
`lastBlockMaxGrDb_ = 0.0f`. (Current code is close; verify.)

### process(peakIn, gainOut, numSamples)

```cpp
void LimiterEnvelope::process (const float* peakIn, float* gainOut, int numSamples) noexcept
{
    if (peakIn == nullptr || gainOut == nullptr || numSamples <= 0)
        return;

    const int n  = std::min (numSamples, maxBlockSize_);
    const int la = lookaheadSamples_;

    if ((int) ext_.size() < n + la)
        return;

    recomputeAlpha();

    // 1. Seed ext_[0..la-1] from carried-in history.
    for (int k = 0; k < la; ++k)
        ext_[(size_t) k] = historyPost_[(size_t) k];

    // 2. Initialise ext_[la..n+la-1] to 1.0 (no reduction planned yet).
    for (int j = la; j < n + la; ++j)
        ext_[(size_t) j] = 1.0f;

    const float thr = thresholdLinear_;

    // 3. Backward-propagated raised-cosine attack.
    //    peakIn[i] (input index i) constrains gain at OUTPUT index i+la,
    //    ramping back from anchor=ext_[i] at output index i.
    for (int i = 0; i < n; ++i)
    {
        const float pk       = std::max (0.0f, peakIn[i]);
        const float required = (pk > thr) ? (thr / pk) : 1.0f;
        const float anchor   = ext_[(size_t) i];   // envelope at output index i

        for (int k = 0; k <= la; ++k)
        {
            const int idx = i + la - k;
            if (idx < 0 || idx >= n + la)
                continue;

            // shape: 0 at the anchor end (k = la), 1 at the peak end (k = 0).
            const float shape = (k == la) ? 0.0f
                                          : attackTable_[(size_t) (la - 1 - k)];
            const float tentative = anchor + (required - anchor) * shape;

            if (tentative < ext_[(size_t) idx])
                ext_[(size_t) idx] = tentative;
        }
    }

    // 4. Forward release smoothing — output range [0, n-1].
    float maxGr = 0.0f;
    float s1 = stage1_;
    float s2 = stage2_;

    for (int j = 0; j < n; ++j)
    {
        const float inp = ext_[(size_t) j];

        if (inp < s2 - 1.0e-12f)
        {
            // Attack: snap to the planned envelope (already shaped by raised cosine).
            s1 = inp;
            s2 = inp;
        }
        else
        {
            // Release: two cascaded one-poles toward inp.
            s1 = alpha_ * s1 + (1.0f - alpha_) * inp;
            s2 = alpha_ * s2 + (1.0f - alpha_) * s1;
        }

        const float g = std::clamp (s2, 1.0e-12f, 1.0f);
        gainOut[j] = g;

        const float gr = 20.0f * std::log10 (std::max (1.0e-12f, 1.0f / g));
        if (gr > maxGr) maxGr = gr;
    }

    stage1_ = s1;
    stage2_ = s2;
    lastBlockMaxGrDb_ = maxGr;

    // 5. Save the trailing la samples of PRE-SMOOTHING ext_ as next-block history.
    //    These are the "future planning" that extended beyond the current block.
    for (int k = 0; k < la; ++k)
        historyPost_[(size_t) k] = ext_[(size_t) (n + k)];
}
```

### Key differences from the current implementation (must all change)

| Current (buggy)                              | New (correct)                                  |
|----------------------------------------------|------------------------------------------------|
| `for k = 1..la: idx = i - k + la`            | `for k = 0..la: idx = i + la - k`              |
| `tentative` uses `attackTable_[la - k]` (1-based k) | `shape = attackTable_[la-1-k]` for `k<la`, else `0` |
| `ext_` layout: `[history(la) | current(n)]` | `ext_` layout: `[carry-in(la) | extend-out(n)]`, indexed directly as output index |
| Output reads `ext_[j]` (for j ∈ [0,n-1])     | Output reads `ext_[j]` (semantics identical: output index j) |
| `historyPost_[k] = gainOut[n - la + k]`      | `historyPost_[k] = ext_[n + k]` (pre-smoothing) |

Note: the *expression* `ext_[j]` for output reading is unchanged textually,
but its meaning is fixed — `ext_[j]` now directly represents output index `j`.

────────────────────────────────────────
4. SCOPE
────────────────────────────────────────

**MODIFY:**
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`

**DO NOT MODIFY:**
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h`
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LookaheadDelay.h`
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/PeakDetector.h`
- Anything in the product repo
- Anything in `shared/dsp_bench/`

If you believe another file needs touching, STOP and ask.

────────────────────────────────────────
5. BUILD & RUN
────────────────────────────────────────

```bash
# Rebuild product (links HQ mdsp_dsp).
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j   # bench targets Release per Slice 3 follow-up note

# Run the bench self-test (Release VST3, --quick).
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
python bench.py --driver master_limiter --slice 3 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE03_AFTER_ENVFIX
```

────────────────────────────────────────
6. GATE
────────────────────────────────────────

After your fix, the bench against the Release VST3 at 3 dB GR (pink-noise-calibrated drive) should produce:

- Sample-peak overs at −1 dBFS ceiling = **0** on pink_noise_60s, sine_sweep_log_30s, imd_smpte.
  (dirac/transient still excluded from aggregate per Slice 2 design.)
- Null residual K-weighted on pink_noise_60s ≤ **−60 dBFS** (per SPEC §5 3 dB column).
- THD+N proxy (`noise_residual_pct`) on pink ≤ **0.10 %**.
- IMD on imd_smpte ≤ **0.08 %**.
- Latency reported = lookahead samples (~240 @ 48 kHz); measured matches ±1 sample.

If any of those still fail after this fix, STOP and report — that indicates a
second-order issue (e.g. release coefficient, threshold smoothing) that
needs its own analysis. Do not loosen criteria.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Brief retrieval note (files re-read).
2. The full new contents of `LimiterEnvelope.cpp` after your edit (so the
   architect can review without git access). Or a clean unified diff —
   whichever is smaller.
3. Build summary lines (HQ + product, both Debug and Release).
4. Bench invocation, exit code, and stdout final line.
5. The numeric values for: null_residual_kweighted_db (pink), noise_residual_pct
   (pink), imd_smpte_pct, true_peak_overs_max (aggregate), latency_samples_measured
   vs latency_samples_reported.
6. If any §6 criterion fails, stop and report — do not iterate.
