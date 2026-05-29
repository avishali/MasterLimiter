# Cursor Task — Slice 7.1.4.1: actually engage SIMD in LimiterEnvelope ramp loop

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ-only fix. Slice 7.1.4 shipped a refactor whose "SIMDRegister
> wrapper around unaligned native loads/stores" produced
> mathematically-correct output (bench PASS) but did not deliver the
> expected speedup. avishali audition: Env still 2000–5000 µs, CPU
> behaviour unchanged from pre-7.1.4, pops persist in Clean mode at
> all Link values (mode-scaled: Aggressive none, Tight moderate,
> Clean many).**
>
> Replace the current SIMD body with one that actually vectorizes,
> using whichever of two approaches Cursor can verify engages SIMD
> instructions on this build (Apple Silicon NEON primary target):
>
> **Path A — manual NEON / SSE intrinsics** with `__ARM_NEON` /
> `__SSE2__` guards and a scalar fallback. Direct, predictable,
> single-pass kernel.
>
> **Path B — juce::FloatVectorOperations multi-pass** with a member
> scratch buffer. Multi-pass over chunk data but each pass is
> vectorized; simpler code, header change for the scratch buffer.
>
> Cursor verifies the chosen path actually produces NEON instructions
> by inspecting the generated assembly OR by an A/B timing measurement
> in a quick standalone test. Bench Slice 3/4/5 must PASS unchanged.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ DSP
source only. No product edits. No bench recalibration. No HQ push.
No amend of prior HQ commits (including `52b5e07` Slice 7.1.4 SIMD).
This slice stacks on top.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

Audit Cursor's 7.1.4 implementation. Likely cause of non-engagement:
- `SimdF::fromRawArray` asserted on alignment in the available JUCE
  version, so Cursor swapped in scalar loads and stores around the
  SIMD math. The math (`vOne + vReqM1 * vShape`, `SimdF::min`) runs
  on SIMDRegister objects but if the load/store ops are scalar, the
  compiler may not actually emit SIMD instructions for the whole
  kernel.
- Or the scalar load fills SIMDRegister lanes one at a time, defeating
  the win.

avishali audition numbers post-7.1.4:
- Env still in 2000–5000 µs range under stress (target was ~600 µs at
  Link=100 Clean, ~1235 µs at Link=0 Clean).
- CPU bar in Ableton spikes the same as pre-7.1.4 on heavy GR.
- Pops persist; mode-scaled (Aggressive 0, Tight moderate, Clean
  many) — matches scalar-loop-per-attackSamples cost.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ):**
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` — replace the
  7.1.4 SIMD body with a verified-working SIMD kernel.
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h` —
  only if Path B is chosen (scratch buffer member). If Path A
  (intrinsics) is chosen, no header change.

**DO NOT TOUCH:**
- Product files.
- Bench files.
- Any other HQ DSP source.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

### 3.1 Choose Path A or Path B

**Path A — manual NEON / SSE intrinsics.** Preferred when:
- juce::dsp::SIMDRegister's load/store APIs assert on alignment and
  the data isn't easily alignable.
- A single-pass kernel is desired (no scratch buffer).

Implementation:

```cpp
#include <cstdint>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  #include <arm_neon.h>
  #define MDSP_HAVE_NEON 1
#endif

#if defined(__SSE2__)
  #include <emmintrin.h>
  #define MDSP_HAVE_SSE2 1
#endif

// ... inside process(), inner loop ...

const float reqM1 = required - 1.0f;
int j = jStart;
int s = jStart - jBase;

#if MDSP_HAVE_NEON
    const float32x4_t vReqM1 = vdupq_n_f32 (reqM1);
    const float32x4_t vOne   = vdupq_n_f32 (1.0f);
    while (j + 4 <= jEnd)
    {
        const float32x4_t vShape = vld1q_f32 (&attackTable_[(size_t) s]);
        const float32x4_t vTent  = vfmaq_f32 (vOne, vReqM1, vShape);   // 1 + reqM1*shape
        const float32x4_t vExt   = vld1q_f32 (&ext_[(size_t) j]);
        const float32x4_t vMin   = vminq_f32 (vExt, vTent);
        vst1q_f32 (&ext_[(size_t) j], vMin);
        j += 4;
        s += 4;
    }
#elif MDSP_HAVE_SSE2
    const __m128 vReqM1 = _mm_set1_ps (reqM1);
    const __m128 vOne   = _mm_set1_ps (1.0f);
    while (j + 4 <= jEnd)
    {
        const __m128 vShape = _mm_loadu_ps (&attackTable_[(size_t) s]);
        const __m128 vMul   = _mm_mul_ps (vReqM1, vShape);
        const __m128 vTent  = _mm_add_ps (vOne, vMul);
        const __m128 vExt   = _mm_loadu_ps (&ext_[(size_t) j]);
        const __m128 vMin   = _mm_min_ps (vExt, vTent);
        _mm_storeu_ps (&ext_[(size_t) j], vMin);
        j += 4;
        s += 4;
    }
#endif

// Scalar tail (always runs after vector body, handles remainder).
for (; j < jEnd; ++j, ++s)
{
    const float shape = attackTable_[(size_t) s];
    const float tent  = 1.0f + reqM1 * shape;
    if (tent < ext_[(size_t) j])
        ext_[(size_t) j] = tent;
}
```

Notes:
- `vld1q_f32` / `vst1q_f32` are unaligned NEON load/store (NEON has
  no separate aligned/unaligned distinction; same instruction).
- `vfmaq_f32` is fused multiply-add (single instruction on ARMv8).
- `vminq_f32` is lane-wise min.
- `_mm_loadu_ps` / `_mm_storeu_ps` are SSE unaligned load/store.
- SSE doesn't have FMA without AVX; using mul + add is fine
  (sub-epsilon difference from FMA, well within bench tolerance).
- No header change needed.

**Path B — juce::FloatVectorOperations multi-pass with scratch.**
Preferred when:
- intrinsics feel too invasive.
- A header change for the scratch buffer is acceptable.

Implementation:

```cpp
// In header (LimiterEnvelope.h), private members:
std::vector<float> scratchTent_;   // sized to lookaheadSamples_ in prepare()

// In prepare():
scratchTent_.assign ((size_t) lookaheadSamples_, 0.0f);

// In process() inner loop body:
const int chunkLen = jEnd - jStart;
const int sStart   = jStart - jBase;

if (chunkLen > 0)
{
    juce::FloatVectorOperations::copy     (scratchTent_.data(),
                                           &attackTable_[(size_t) sStart],
                                           chunkLen);
    juce::FloatVectorOperations::multiply (scratchTent_.data(), reqM1, chunkLen);
    juce::FloatVectorOperations::add      (scratchTent_.data(), 1.0f,  chunkLen);
    juce::FloatVectorOperations::min      (&ext_[(size_t) jStart],
                                           scratchTent_.data(),
                                           chunkLen);
}
```

Path B is multi-pass (4 passes over the chunk) but each pass is
vectorized by JUCE's FloatVectorOperations. Likely ~2× speedup vs
scalar (memory-bandwidth bound), less than Path A's ~4× but
simpler code.

**Decision rule:** Cursor tries Path A first. If `arm_neon.h` /
`emmintrin.h` aren't reachable in the build environment, fall back
to Path B. Either way, verify SIMD actually engages (§3.2).

### 3.2 Verify SIMD engages

After the implementation change, verify NEON instructions appear in
the generated assembly OR demonstrate runtime speedup via a quick
A/B test.

**Option 1 — assembly inspection** (cleanest):

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
# Locate the built object file for LimiterEnvelope.cpp.o, then:
objdump -d build-release/.../LimiterEnvelope.cpp.o | grep -E "fmla|fmul|fadd|fmin" | head -20
# Expect to see vector forms like fmla v?.4s, v?.4s, v?.4s on arm64.
```

If `fmla.4s`, `fmin.4s`, etc. appear, SIMD engaged.

**Option 2 — runtime timing** (cruder but works without assembly
tools):

Add a TEMPORARY test in a Debug-only main or test harness:
- Construct a `LimiterEnvelope`, prepare for OS rate, Clean mode.
- Run 1000 process() calls on dense peak input (every sample
  exceeds threshold).
- Time wall-clock.

Compare against Slice 7.1.4 commit `52b5e07` (which avishali
confirmed didn't engage). The new implementation should be measurably
faster — at least 2× for Path B, ~4× for Path A.

This test code can be tossed after verification, not committed.

────────────────────────────────────────
4. BUILD
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean. If on arm64 macOS, expect NEON intrinsic
headers (`arm_neon.h`) to be available — JUCE projects routinely use
them.

────────────────────────────────────────
5. BENCH (must remain PASS unchanged)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE07_1_4_1_REGRESSION_S$SLICE"
done
```

All three must PASS 13/13, 14/14, 25/25. SIMD math should produce
sub-epsilon differences vs scalar; bench thresholds accommodate.

────────────────────────────────────────
6. HQ COMMIT (separate, no push, no amend)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# expect:
#    M shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp
#    (M shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h if Path B)

git add shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp
# git add shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h   # Path B only

git commit -m "$(cat <<'EOF'
LimiterEnvelope: actually engage SIMD in inner ramp loop

Slice 7.1.4 (commit 52b5e07) refactored the inner loop to a
SIMDRegister-based body but wrapped unaligned native loads/stores
around the SIMD math because the available JUCE SIMDRegister API
asserted on alignment. The math was correct (bench PASS) but the
compiler did not emit vectorized instructions across the load /
math / store sequence — performance remained scalar.

avishali audition post-7.1.4 confirmed: Env section still in 2000-
5000 us range under stress (target ~600 us at Link=100 Clean),
CPU behaviour unchanged from pre-SIMD, audible pops persist in
Clean mode at all Link values, mode-scaled (Aggressive 0, Tight
moderate, Clean many) — direct evidence the inner-loop cost still
scales linearly with attackSamples.

Replaced with [Path A: direct NEON intrinsics (vld1q_f32, vfmaq_f32,
vminq_f32, vst1q_f32) with __ARM_NEON guard, SSE2 (_mm_loadu_ps,
_mm_mul_ps, _mm_add_ps, _mm_min_ps, _mm_storeu_ps) fallback for
Intel macOS, scalar tail handles remainder] / [Path B: juce::
FloatVectorOperations multi-pass through a member scratchTent_
buffer (copy + multiply + add + min)].

Cursor verified SIMD engagement via [assembly inspection: fmla.4s,
fmin.4s, etc. emitted in arm64 build / runtime timing showing
~Nx speedup vs commit 52b5e07].

Expected impact under stress (Link=100, Clean, dense material, 80%+
peak density at 48k/256-sample buffer):
- Env section: 2000-5000 us → ~600 us (Path A) / ~1500 us (Path B)
- Block max: well under 5333 us deadline
- Pops resolved

Slice 3/4/5 bench PASS 13/13, 14/14, 25/25 unchanged. SIMD math
produces sub-epsilon differences vs strict scalar (FMA fusion,
instruction ordering); bench thresholds accommodate.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Which path Cursor chose (A or B) and why.
2. SIMD engagement verification: either assembly output excerpt
   showing vector instructions OR runtime A/B timing showing the
   speedup.
3. Diff of the inner-loop refactor.
4. Header diff (if Path B).
5. Build success lines for Release and Debug.
6. Bench PASS lines for Slice 3/4/5.
7. HQ `git log --oneline -6` showing this commit on top of `52b5e07`.
8. Confirmation: HQ NOT pushed; product NOT touched; bench criteria
   NOT modified.
9. Open questions.

────────────────────────────────────────
8. AFTER THIS
────────────────────────────────────────

avishali re-auditions in Live (Release VST3), same stress condition
as the 7.1.4 audition that produced the "no speedup" finding:

- **Clean mode at Link = 100%, heavy GR**: Env watermark should
  drop substantially (target ~600 µs Path A, ~1500 µs Path B).
  Block max well under 5333 µs deadline. Pops should be resolved.
- **CPU bar in Ableton**: should drop dramatically; the GR↔CPU
  spike correlation should be much milder.
- **Tight and Aggressive**: same speedup applies but lower absolute
  values; already weren't popping.
- **Link = 0%, Clean, heavy GR**: the original 7.1.4 target case.
  Env max should drop ~4× from 4943 → ~1235 (Path A) or ~2× from
  4943 → ~2500 (Path B). Either way well under deadline at
  256/48k.

Three outcomes:

**A. Pops resolved, CPU drops, bench PASS** → Slice 7 close. Covers
the entire 7.x family (7 + 7.1 + 7.1.1 + 7.1.2 + 7.1.3 + 7.1.4 +
7.1.4.1) in one consolidated ship.

**B. Pops reduced but persist at extreme settings (Clean + dense +
heavy GR)** → Slice 7.1.5 caps Clean attack 3 → 1.5 ms for another
2× margin (Slice 9.4.2-style one-line change).

**C. SIMD still doesn't engage** → STOP and report verification
output. We'd debug the build configuration (compile flags,
optimization level) before further DSP changes.
