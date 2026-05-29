# Cursor Task — Slice 9.4.1: LimiterEnvelope — fast-path non-peak samples to fix CPU explosion

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ DSP fix only. One-line guard. Audition-only — no bench this slice;
> bench recalibration still consolidated into Slice 9.5.**
>
> Slice 9.4 added 4× oversampling around the limiter chain. CPU in
> Ableton jumped to ~88% and avishali reports pops when adjusting
> parameters (classic host buffer-underrun symptom at high CPU load).
>
> Root cause: `LimiterEnvelope::process` has a nested loop
> `for i in [0,n) × for k in [0, attackSamples)`. Both bounds scale with
> sample rate. At 4× OS, n is 4× larger AND attackSamples is 4× larger
> (it's derived from sampleRate in `recomputeAttackSamples`). Total
> ramp-loop work is **16×** the pre-9.4 cost, not 4×.
>
> Fix exploits a property of the Slice 9.3a.1 ramp formula. After 9.3a.1
> the tentative value is `1.0f + (required - 1.0f) * shape`. When
> `required == 1.0f` (no peak excess - i.e., pk <= thr), tentative is
> `1.0f` for every shape value, and the min-write check
> `if (tentative < ext_[idx])` is never true (ext_ values are bounded
> at 1.0 from above). The entire inner loop becomes a guaranteed no-op.
>
> Skipping the inner loop when `pk <= thr` typically drops envelope CPU
> by 70-95% depending on signal density. For most mastering material the
> peak-sample density is well under 30%; the inner loop runs only on
> samples that actually need reduction.
>
> **HQ-only. Product untouched. No push. No amend.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ DSP file
only. No product edits. No bench edits. No HQ push. No amend of
`7ca50ac` or any earlier HQ commit.

────────────────────────────────────────
1. WHY (math)
────────────────────────────────────────

After 9.3a.1, the inner ramp loop is:

```cpp
for (int k = 0; k < attackSamples; ++k)
{
    const int idx = i + la - k;
    if (idx < 0 || idx >= n + la) continue;
    const float shape = attackTable_[(size_t) (attackSamples - 1 - k)];
    const float tentative = 1.0f + (required - 1.0f) * shape;
    if (tentative < ext_[(size_t) idx]) ext_[(size_t) idx] = tentative;
}
```

When `required = 1.0f`:
- `tentative = 1.0f + 0.0f * shape = 1.0f` for every k.
- `ext_[idx]` is always `<= 1.0f` (initialised to 1.0, only ever
  overwritten with smaller values by the min-write check).
- `if (1.0f < ext_[idx])` is false in every case.
- The entire inner loop produces zero side effects.

So we can short-circuit the entire inner loop with an early-continue at
the outer level when the input sample doesn't exceed threshold:

```cpp
if (pk <= thr) continue;
```

The peak detector and metering work upstream of this loop are unchanged.
The smoothing stage downstream is unchanged. ext_ retains the values it
already has at those positions (from prior ramp writes or initial 1.0),
which is exactly the correct behaviour for non-peak samples.

CPU impact: pre-fix the inner loop runs N×attackSamples times per block.
Post-fix it runs N_peak × attackSamples where N_peak is the count of
samples exceeding threshold. For typical mastering material at 3-5 dB
GR, N_peak / N is between 5% and 30%, giving a 3×-20× speedup of the
envelope stage alone.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ):**
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` — `process()`: add
  one `if (pk <= thr) continue;` line after the `required` computation
  in the outer loop. Nothing else.

**DO NOT TOUCH:**
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h`
- Any product file
- Any bench file
- Any other HQ source

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

In `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`, in `process()`,
locate:

```cpp
for (int i = 0; i < n; ++i)
{
    const float pk = std::max (0.0f, peakIn[i]);
    const float required = (pk > thr) ? (thr / pk) : 1.0f;

    for (int k = 0; k < attackSamples; ++k)
    {
        ...
    }
}
```

Replace with:

```cpp
for (int i = 0; i < n; ++i)
{
    const float pk = std::max (0.0f, peakIn[i]);
    const float required = (pk > thr) ? (thr / pk) : 1.0f;

    if (pk <= thr)
        continue;

    for (int k = 0; k < attackSamples; ++k)
    {
        ...
    }
}
```

Single added line: `if (pk <= thr) continue;`. The `required`
computation stays as-is (it's not used in the skip branch but the
existing structure is fine — Cursor may choose to lift the `continue`
above `required` if cleaner, equivalent behaviour).

────────────────────────────────────────
4. BUILD (no bench)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean.

**Do NOT run the bench this slice.** Bench recalibration is Slice 9.5
consolidated.

────────────────────────────────────────
5. HQ COMMIT (separate, no push, no amend)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# expect ONLY:
#    M shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp

git add shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp

git commit -m "$(cat <<'EOF'
LimiterEnvelope: fast-path non-peak samples in ramp loop (16x CPU win at 4x OS)

After Slice 9.3a.1 the inner ramp loop's tentative formula became
tentative = 1.0f + (required - 1.0f) * shape. When required == 1.0f (no
peak excess; i.e., pk <= thr), tentative is 1.0f for every shape value
and the min-write check `if (tentative < ext_[idx])` is never true
because ext_[idx] is bounded at 1.0f from above. The entire inner loop
is a guaranteed no-op for any non-peak input sample.

Slice 9.4 introduced 4x oversampling around the limiter chain. Both
the outer loop bound (n) and the inner loop bound (attackSamples) scale
with sample rate, so total ramp-loop work jumped by 16x, not 4x.
Ableton showed ~88% CPU and parameter adjustments produced audio pops
characteristic of host buffer underrun under load.

Fix: short-circuit the outer loop with `if (pk <= thr) continue;`
immediately after computing pk and required. The inner loop now only
runs for samples that actually need reduction. On typical mastering
material the peak-sample density is 5-30%, yielding a 3x-20x speedup
of the envelope stage. ext_ values for non-peak positions are preserved
unchanged (which is correct - the smoothing stage handles release
exponentially without any new ramp writes needed when no peak is
present).

No behavioural change for samples that do exceed threshold. Slice 3/4/5
bench unchanged conceptually; recalibration still consolidated into
Slice 9.5.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push HQ. Do NOT amend any prior HQ commit.** Stack is now:
this fix on top of `7ca50ac` on top of `8f53371` on top of `763604e`
on top of `3dfee43` on top of `bbd4293` on top of `276c397`.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Exact before/after of the changed lines (the one-line continue
   added).
2. Build success lines for Release and Debug.
3. HQ `git log --oneline -7` showing the new commit on top of the stack.
4. Confirmation: HQ NOT pushed; product repo NOT touched; bench NOT run.
5. Open questions.

────────────────────────────────────────
7. AFTER THIS
────────────────────────────────────────

avishali re-auditions in Live (Release VST3):

- CPU at default settings (Tight, release 30 ms, Clipper 0, SP ceiling
  -1 dB) on the same dense mix that was at 88%. Should drop to a
  fraction of that. Pre-9.4 the build was probably around 5-10% CPU; we
  expect 9.4.1 to land at maybe 15-25% (4x OS overhead remains, but
  the envelope ramp loop is no longer the hot path).
- Parameter adjustment (turning Drive, Ceiling, Release, Character)
  should no longer produce pops. If pops persist, they're a separate
  RT-safety issue (zipper noise from un-smoothed param reads, or a
  prepare-cycle being triggered by something other than sample-rate
  change) and we'll investigate.
- Limiting character (the 9.4 distortion-reduction win) should be
  unchanged — this slice only short-circuits a no-op code path.
- 3-5 dB GR push should still sound clean (the 9.4 aliasing-suppression
  improvement is preserved).

On pass → Slice 9.5 (consolidated bench recalibration).

On partial pass (CPU much better but pops remain) → next investigation
is RT-safety in processBlock or param-listener code. Suspects:
  - `setLatencySamples(...)` being called on any param change (should
    only fire on ceiling-mode SP/TP transition).
  - APVTS listeners doing non-trivial work on the audio thread.
  - Allocation in processBlock (none expected; the AudioBlock wrapping
    is stack-only).
We instrument with a one-shot lock-free flag if needed.
