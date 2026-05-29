# Cursor Task — Slice 9.4.2: LimiterEnvelope — cap Clean attack to 3 ms (CPU win in Clean mode)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ DSP fix only. One-line change. Audition-only — no bench this slice.**
>
> Slice 9.4 introduced 4× oversampling; Slice 9.4.1 fast-pathed the
> inner ramp loop for non-peak samples. CPU is now ~25% in Tight at
> default settings, but **switching to Clean spikes CPU to 80%+**.
>
> Root cause: Clean's attackSamples_ is tied to lookaheadSamples_
> (currently 960 at 4× OS), so each peak-sample iteration of the inner
> ramp loop does 960 writes vs ~192 in Tight or ~58 in Aggressive.
> Combined with the second smoothing cascade (s2s in T/S split, Clean-
> only), Clean's per-block cost is ~5–8× the other modes on dense
> material.
>
> ADR-0006 said "raised-cosine attack over the lookahead window" but
> didn't require attack window == full lookahead. Pro-L 2's
> Transparent/Smooth styles use ~2–3 ms attack even with longer
> lookahead. Audible difference between 3 ms and 5 ms raised-cosine
> attack on mastering material is below threshold for most listeners,
> and 4× OS further smooths the attack via the downsample FIR.
>
> Fix: cap Clean attack at 3 ms (0.003 × sampleRate, same time-units
> formula Tight/Aggressive already use). Inner loop drops 960 → 576 at
> OS rate; CPU in Clean should fall to ~40–50%.
>
> **HQ-only. Product untouched. No push. No amend.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ DSP file
only. No product edits. No bench edits. No HQ push. No amend of any
prior HQ commit.

────────────────────────────────────────
1. WHY (short)
────────────────────────────────────────

Clean mode currently uses `attackSamples_ = lookaheadSamples_` (the full
5 ms lookahead window in samples). At 4× OS this is 960 samples per
peak input iteration. Combined with the always-on T/S split smoothing
(s2 + s2s cascades), Clean is the hot mode on the CPU profile.

Capping Clean's attack at 3 ms gives a 40% reduction in the inner loop
length (960 → 576 at OS). The raised-cosine shape is preserved — just
shorter window. 5 ms vs 3 ms raised-cosine attack is perceptually very
close for mastering use; both read as "soft attack" well above the
~1.5 ms threshold where attack character becomes audibly distinct.

The smoothing stages (s2, s2s) are unaffected by this change. The min-
write semantic in the ramp loop is unaffected. ext_ behaviour at
positions outside the new shorter attack window is unchanged — those
positions stay at 1.0 (no reduction) until a peak's ramp writes them.
The smoothing stage handles release exponentially as designed.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ):**
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` —
  `recomputeAttackSamples()`: change the Clean case from
  `attackSamples_ = lookaheadSamples_;` to
  `attackSamples_ = std::min (lookaheadSamples_, (int) std::llround (0.003 * sampleRate_));`

**DO NOT TOUCH:**
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h`
- Any product file
- Any bench file
- Tight or Aggressive case in `recomputeAttackSamples()`
- Any other HQ source

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

In `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`, in
`recomputeAttackSamples()`, locate:

```cpp
switch (mode_)
{
    case Mode::Clean:
        attackSamples_ = lookaheadSamples_;
        break;

    case Mode::Tight:
        attackSamples_ = std::max (1, (int) std::llround (0.001 * sampleRate_));
        break;

    case Mode::Aggressive:
        attackSamples_ = std::max (1, (int) std::llround (0.0003 * sampleRate_));
        break;
}
```

Replace with:

```cpp
switch (mode_)
{
    case Mode::Clean:
        attackSamples_ = std::min (lookaheadSamples_,
                                   std::max (1, (int) std::llround (0.003 * sampleRate_)));
        break;

    case Mode::Tight:
        attackSamples_ = std::max (1, (int) std::llround (0.001 * sampleRate_));
        break;

    case Mode::Aggressive:
        attackSamples_ = std::max (1, (int) std::llround (0.0003 * sampleRate_));
        break;
}
```

Clean now picks the smaller of `lookaheadSamples_` (in case the user
configures a very short lookahead) and 3 ms expressed in samples at the
current sample rate. Tight and Aggressive unchanged.

The trailing `attackSamples_ = std::clamp (attackSamples_, 1,
lookaheadSamples_);` line further down in the function stays as-is —
defensive clamp that's now slack in Clean too.

────────────────────────────────────────
4. BUILD (no bench)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean.

**Do NOT run the bench this slice.** Bench recalibration still
consolidated into Slice 9.5.

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
LimiterEnvelope: cap Clean attack at 3 ms (CPU win after 4x OS)

Slice 9.4 introduced 4x oversampling; Slice 9.4.1 fast-pathed the inner
ramp loop for non-peak samples. CPU in Tight is now ~25% at default
settings, but switching to Clean spikes to 80%+ because Clean uses
attackSamples_ = lookaheadSamples_ (960 at 4x OS, vs 192 in Tight and
58 in Aggressive). Combined with the second smoothing cascade (s2s,
Clean-only via T/S split), Clean is the hot mode on the CPU profile.

ADR-0006 specified "raised-cosine attack over the lookahead window"
but did not require attack window == full lookahead. Reference
implementations (Pro-L 2 Smooth/Transparent styles) use ~2-3 ms attack
even with longer lookahead; audible difference between 3 ms and 5 ms
raised-cosine attack is below threshold for typical mastering material,
and the 4x OS downsample FIR further smooths the attack profile.

Cap Clean's attack at 3 ms (= 0.003 * sampleRate_, same time-units
formula Tight/Aggressive already use). Inner-loop length drops 960 ->
576 at 4x OS (-40%). Combined with the 9.4.1 non-peak fast-path,
Clean's ramp CPU should now sit in a similar range to Tight.

Defensive min vs lookaheadSamples_ retained in case a future
configuration sets lookahead shorter than 3 ms (edge case but cheap to
cover).

Tight (1 ms) and Aggressive (0.3 ms) unchanged. ext_ semantics, T/S
smoothing, release behaviour all unchanged. Slice 3/4/5 bench
recalibration still consolidated into Slice 9.5.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push HQ. Do NOT amend any prior HQ commit.** Stack now adds
this commit on top of the 9.4.1 fast-path commit.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Exact before/after of the Clean case in `recomputeAttackSamples()`.
2. Build success lines for Release and Debug.
3. HQ `git log --oneline -8`.
4. Confirmation: HQ NOT pushed; product repo NOT touched; bench NOT run.
5. Open questions.

────────────────────────────────────────
7. AFTER THIS
────────────────────────────────────────

avishali re-auditions in Live (Release VST3):

- CPU in Clean at default settings on the same dense mix that was
  hitting 80%+ should drop to ~40–50%, closer to Tight's ~25%.
- Clean character A/B: 3 ms attack should still read as "soft" and
  "transparent" — the perceptual distinction from the prior 5 ms is
  small, and the 4× OS downsample further softens the attack edge.
  If avishali can hear an unacceptable difference, fall back to Option
  B (SIMD the inner ramp loop) which preserves 5 ms character at
  similar CPU.
- Pops on parameter adjust should be fully gone now that CPU has
  headroom.
- Tight and Aggressive character: unchanged.
- 3-5 dB GR push: distortion behaviour unchanged from 9.4 (still clean,
  the 4× OS aliasing-suppression win is preserved).

On pass → Slice 9.5 (consolidated bench recalibration covering 9.2 +
9.3a + 9.3a.1 + 9.4 + 9.4.1 + 9.4.2). Then Slice 9 close.

On Clean-character fail → Slice 9.4.3 (SIMD inner ramp loop using
juce::FloatVectorOperations or platform intrinsics). Preserves 5 ms
Clean attack, ~3-4× speedup on the per-peak inner work. Higher
implementation complexity but architecturally clean.
