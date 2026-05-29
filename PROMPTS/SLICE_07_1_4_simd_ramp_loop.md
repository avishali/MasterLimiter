# Cursor Task — Slice 7.1.4: SIMD the LimiterEnvelope inner ramp loop

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ DSP optimization. One file. Single SIMD kernel + scalar tail.
> Expected 4× speedup on the envelope ramp inner loop (NEON on Apple
> Silicon, SSE on Intel). Output bit-equivalent to scalar within float-
> rounding tolerance. Bench Slice 3/4/5 must PASS unchanged.**
>
> Slice 7.1.3 audition data confirmed: at default Link = 100% (fast-
> path) CPU is stable, no pops. At Link < 100% with peak reduction on
> dense Clean material, the envelope ramp loop runs in TWO envelopes at
> OS rate; Env section measured 4943 µs and Block total 5393 µs at
> 48k / 256-sample buffer (5333 µs deadline). The deadline miss causes
> a buffer underrun → audible pop.
>
> The inner ramp loop is the dominant cost and is trivially vectorizable.
> Restructure to forward j-indexing (SIMD-friendly memory access),
> apply `juce::dsp::SIMDRegister<float>` for the math, scalar tail for
> the remainder. Expected post-SIMD numbers:
>   - Env section: 4943 → **~1235 µs** (4×)
>   - Block total: 5393 → **~1700 µs** (well under 5333 µs deadline)
>
> No product change. No new params. No bench recalibration (output
> bit-equivalent within tolerance; existing Slice 3/4/5 thresholds
> already accommodate float-rounding variance).
>
> **HQ-only. Product untouched. No push.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ DSP
source only. No product edits. No bench recalibration. No HQ push.
No amend of prior HQ commits.

────────────────────────────────────────
1. WHY (math)
────────────────────────────────────────

The current inner ramp loop in `LimiterEnvelope::process` (after
9.3a.1 anchor→1.0 and 9.4.1 outer-loop fast-path):

```cpp
for (int k = 0; k < attackSamples; ++k)
{
    const int idx = i + la - k;
    if (idx < 0 || idx >= n + la) continue;
    const float shape = attackTable_[(size_t)(attackSamples - 1 - k)];
    const float tentative = 1.0f + (required - 1.0f) * shape;
    if (tentative < ext_[(size_t) idx])
        ext_[(size_t) idx] = tentative;
}
```

Memory access pattern: as k increases, idx decreases AND shape index
increases. Two reversed strides. Not SIMD-friendly.

Refactor: substitute `j = i + la - k` so the loop iterates j forward.
Then `k = i + la - j` and shape index `s = attackSamples - 1 - k = j -
(i + la - attackSamples + 1)`. Define `jBase = i + la - attackSamples
+ 1`; then `s = j - jBase`. As j increments by 1, s increments by 1.
Both ext_ and attackTable_ are now read forward with stride 1 — SIMD
ready.

Bounds: clamp `jStart = max(0, jBase)` and `jEnd = min(n + la, i + la
+ 1)`. All j values in `[jStart, jEnd)` are guaranteed valid; no
per-iteration bounds check needed inside the loop.

SIMD kernel (single pass, no scratch buffer):

```cpp
const SimdF vReqM1 = SimdF::expand(required - 1.0f);
const SimdF vOne   = SimdF::expand(1.0f);

for (chunk of SIMDNumElements wide):
    vShape = load(attackTable_ + s)
    vTent  = vOne + vReqM1 * vShape
    vExt   = load(ext_ + j)
    vMin   = SimdF::min(vExt, vTent)
    store(ext_ + j, vMin)
```

Per-element cost: ~2 vector loads + 1 vector store + 2 vector FLOPs,
amortized across `SIMDNumElements` (4 on NEON / SSE, 8 on AVX). For
NEON (Apple Silicon), 4× speedup is the typical realized win on this
kernel shape — memory-bandwidth bound but with good prefetching.

Scalar tail handles the remainder when `(jEnd - jStart) %
SIMDNumElements != 0`.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ):**
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` — restructure
  the inner ramp loop in `process()` to forward j-iteration with
  SIMD kernel + scalar tail.

**DO NOT TOUCH:**
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h` — no
  header change needed. No new members. The SIMD kernel works
  register-to-register, no scratch buffer required.
- Product files.
- Bench files. Output bit-equivalent within float tolerance; existing
  Slice 3/4/5 criteria (post-9.5b) accommodate sub-epsilon rounding
  variance.
- Any other HQ DSP source.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

### 3.1 Inner loop refactor

In `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`, locate the
outer loop body in `process()`:

```cpp
for (int i = 0; i < n; ++i)
{
    const float pk = std::max (0.0f, peakIn[i]);
    const float required = (pk > thr) ? (thr / pk) : 1.0f;

    if (pk <= thr)
        continue;

    for (int k = 0; k < attackSamples; ++k)
    {
        const int idx = i + la - k;
        if (idx < 0 || idx >= n + la)
            continue;

        const float shape = attackTable_[(size_t)(attackSamples - 1 - k)];
        const float tentative = 1.0f + (required - 1.0f) * shape;

        if (tentative < ext_[(size_t) idx])
            ext_[(size_t) idx] = tentative;
    }
}
```

Replace with the SIMD-vectorized version:

```cpp
#include <juce_dsp/juce_dsp.h>   // ensure SIMDRegister is available; add include at top of file if missing

using SimdF = juce::dsp::SIMDRegister<float>;
constexpr int simdLanes = (int) SimdF::SIMDNumElements;

for (int i = 0; i < n; ++i)
{
    const float pk = std::max (0.0f, peakIn[i]);

    if (pk <= thr)
        continue;

    const float required = thr / pk;
    const float reqM1    = required - 1.0f;

    // Forward j-iteration: j = i + la - k, s = j - jBase
    const int jBase  = i + la - attackSamples + 1;
    const int jStart = std::max (0, jBase);
    const int jEnd   = std::min (n + la, i + la + 1);

    if (jStart >= jEnd)
        continue;

    int j = jStart;
    int s = jStart - jBase;

    // SIMD body: process simdLanes elements per iteration.
    const SimdF vReqM1 = SimdF::expand (reqM1);
    const SimdF vOne   = SimdF::expand (1.0f);

    while (j + simdLanes <= jEnd)
    {
        const SimdF vShape = SimdF::fromRawArray (&attackTable_[(size_t) s]);
        const SimdF vTent  = vOne + vReqM1 * vShape;
        const SimdF vExt   = SimdF::fromRawArray (&ext_[(size_t) j]);
        const SimdF vMin   = SimdF::min (vExt, vTent);
        vMin.copyToRawArray (&ext_[(size_t) j]);

        j += simdLanes;
        s += simdLanes;
    }

    // Scalar tail.
    for (; j < jEnd; ++j, ++s)
    {
        const float shape = attackTable_[(size_t) s];
        const float tent  = 1.0f + reqM1 * shape;
        if (tent < ext_[(size_t) j])
            ext_[(size_t) j] = tent;
    }
}
```

Notes:
- `SimdF::min(a, b)` returns lane-wise minimum. Confirm exact API
  spelling in the JUCE version present (`SIMDRegister::min` is the
  standard JUCE 6/7 API; if a different spelling is required for
  this codebase's JUCE version, adapt — semantically: `vMin[k] =
  std::min(vExt[k], vTent[k])` for all k).
- `fromRawArray` is unaligned load; `copyToRawArray` is unaligned
  store. attackTable_ and ext_ are not guaranteed aligned to
  SIMD width; unaligned ops are the correct primitive here.
- The `(size_t)` casts are existing style from the surrounding code;
  keep them for consistency.
- No header change, no new member, no new helper function. Self-
  contained refactor inside `process()`.

### 3.2 Verify no other call sites depend on the inner loop's
exact semantics

Quick grep for callers of `LimiterEnvelope::process` to confirm
nothing reads ext_ or attackTable_ during a process call (they
shouldn't — they're private). The change is internal.

────────────────────────────────────────
4. BUILD
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean. If JUCE SIMDRegister API headers aren't
already included in LimiterEnvelope.cpp, add the include. The
`mdsp_dsp` library should already link against `juce_dsp` (used by
ispTrim, etc.) — no CMake change expected.

────────────────────────────────────────
5. BENCH (must remain PASS unchanged)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE07_1_4_REGRESSION_S$SLICE"
done
```

All three must PASS at the same counts as Slice 9 close (13/13,
14/14, 25/25).

Bit-exactness expectation: SIMD operations may produce sub-epsilon
differences vs strict scalar due to FMA fusion / instruction ordering.
The Slice 9.5b criteria already accommodate float-rounding variance
(thresholds are well above measurement precision). If any row fails,
STOP and report — would indicate either a real algorithmic divergence
(SIMD min semantics differ from scalar) or a bug in the refactor.

────────────────────────────────────────
6. HQ COMMIT (separate, no push, no amend)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# expect ONLY:
#    M shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp

git add shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp

git commit -m "$(cat <<'EOF'
LimiterEnvelope: SIMD the inner ramp loop (4x Env speedup)

Slice 7.1.3 audition revealed that at Link < 100% with peak reduction
on dense Clean material, two envelopes running at OS rate exceeded
the 5333 us deadline at 48k/256-sample buffer: Env section 4943 us,
Block total 5393 us, audible buffer underrun → pops.

The inner ramp loop is the dominant cost and is trivially
vectorizable. Refactored to forward j-iteration (substitute j = i +
la - k) so ext_ and attackTable_ are both read forward with stride 1
— SIMD-friendly. Single-pass kernel using juce::dsp::SIMDRegister
<float>: load shape, FMA tentative, load ext, lane-wise min, store.
Scalar tail handles the (jEnd - jStart) % SIMDNumElements remainder.

Implementation notes:
- No header change. Single function body modification in process().
- No scratch buffer. Single-pass register-to-register kernel.
- juce::dsp::SIMDRegister portable across NEON (Apple Silicon),
  SSE (Intel macOS / Windows x64), and AVX where available.
- fromRawArray / copyToRawArray are unaligned load/store; attackTable_
  and ext_ are not guaranteed SIMD-aligned. Unaligned ops are the
  correct primitive.
- Bit-equivalent to scalar within float rounding tolerance. FMA
  fusion may produce sub-epsilon differences. Slice 9.5b criteria
  accommodate this (margins are well above float precision).

Expected impact:
- NEON (4-lane): ~4x speedup on the inner loop.
- Env section: 4943 us → ~1235 us at the Slice 7.1.3 stress
  conditions (Link < 100, dense Clean material, 80%+ peak density).
- Block total: 5393 us → ~1700 us. Well under 5333 us deadline at
  48k/256-sample buffer. Pops from this source resolved.

The outer-loop fast-path from 9.4.1 (skip when pk <= thr) is
preserved unchanged — handles sparse-peak material with zero inner-
loop cost.

Slice 3/4/5 bench PASS 13/13, 14/14, 25/25 unchanged.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push HQ. Do NOT amend prior HQ commits.** Stack: this commit
on top of `d805b2b` (Slice 9.5b bench recal) on top of `b64f7ae`
(ADR-0008) on top of the Slice 9 stack.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Diff of the refactored inner loop: before/after of the entire
   for-i body, with the SIMD kernel and scalar tail visible.
2. Confirmation of SIMDNumElements on the build host (typically 4 on
   Apple Silicon NEON; 4 or 8 on Intel depending on AVX availability).
3. Build success lines for Release and Debug.
4. Bench PASS lines for Slice 3 / 4 / 5 — must all pass at 13/13,
   14/14, 25/25.
5. HQ `git log --oneline -10` showing the new commit on top of
   `d805b2b`.
6. Confirmation: HQ NOT pushed; product NOT touched; bench criteria
   NOT modified; no header change.
7. Open questions.

────────────────────────────────────────
8. AFTER THIS
────────────────────────────────────────

avishali re-auditions in Live (Release VST3):

- **Default Link = 100%**: Env watermark should drop ~125 → ~32 µs
  (4× SIMD on the single fast-path envelope). Block max well under
  budget as before.
- **Link = 0%, dense material, Clean mode, push Drive +6 to +12 dB**:
  the same configuration that produced 4943 µs Env / 5393 µs Block
  / 2434 clicks in 7.1.3. Expected post-SIMD:
  - Env max ~1235 µs
  - Block max ~1700 µs (under 5333 µs deadline at 256/48k)
  - **Pops resolved** if the underrun was the cause (high confidence
    given the 7.1.3 deadline-miss data)
  - Clicks counter: should drop substantially since blocks no longer
    underrun (some click count from legitimate brickwall transients
    at threshold 0.2 will remain — that's expected)
  - CPU% in Ableton: should drop dramatically; the GR↔CPU correlation
    visible in your earlier video should be much milder.

Three outcomes:

**A. Pops resolved, bench PASS** → Slice 7 close prompt runs. Covers
7 + 7.1 + 7.1.1 + 7.1.2 + 7.1.3 + 7.1.4 in one consolidated PROGRESS
entry, one PLAN update, one prompts-archive commit, two pushes.
Debug timing probes optionally removed in the close commit (or kept
as a permanent diagnostic aid — avishali's call).

**B. Pops reduced but persist at extreme settings** → measure how
much margin remains. If Block max is still > 50% of deadline at
extreme settings, follow-up cap Clean attack 3 → 1.5 ms (Slice 9.4.2-
style one-line fix) for another 2× reduction.

**C. Bench fails** → SIMD min semantics or refactor bug. STOP and
report which row failed and by how much. Revert if necessary.
