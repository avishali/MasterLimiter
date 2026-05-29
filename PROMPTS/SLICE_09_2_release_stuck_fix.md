# Cursor Task — Slice 9.2: LimiterEnvelope — fix stuck release in Tight/Aggressive modes

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ DSP fix only.** Slice 9's narrowed attack window in Tight and
> Aggressive modes exposed a long-standing edge case in the attack-ramp
> loop: the trailing-edge write at `k == attackSamples` propagates the
> current anchor (`ext_[i]`) forward by `(lookaheadSamples - attackSamples)`
> samples. In Clean mode this was a no-op because the attack window equals
> the lookahead window (`idx == i`, anchor writes to itself). In
> Tight/Aggressive (attack << lookahead), each input writes a stale low
> anchor into far-future ext_ positions, which become the next anchor,
> which writes again, etc. Result: once GR engages, ext_ never returns to
> 1.0, and avishali sees "infinite release — GR only resets when I turn
> off the limiter."
>
> Fix is one line: skip the trailing-edge iteration. That iteration only
> carries information when `idx == i` (the no-op case), so removing it is
> identity for Clean and unblocks release for Tight/Aggressive. Slice
> 3/4/5 bench regression must remain bit-identical.
>
> **HQ-only. Do NOT touch product. Do NOT push HQ. Do NOT amend prior
> HQ commits.** Separate HQ commit on top of `3dfee43`.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ DSP file
only. No product edits. No bench edits. No HQ push. No amend of
`bbd4293` or `3dfee43`.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

In `LimiterEnvelope::process`, the per-input ramp loop is:

```cpp
for (int k = 0; k <= attackSamples; ++k)
{
    const int idx = i + la - k;
    if (idx < 0 || idx >= n + la) continue;

    const float shape = (k == attackSamples) ? 0.0f
                                              : attackTable_[(size_t) (attackSamples - 1 - k)];
    const float tentative = anchor + (required - anchor) * shape;

    if (tentative < ext_[(size_t) idx])
        ext_[(size_t) idx] = tentative;
}
```

At `k == attackSamples`, `shape = 0`, so `tentative = anchor = ext_[i]`.

- **Clean** (`attackSamples_ == lookaheadSamples_`): `idx = i + la - la =
  i`, and the candidate write is `ext_[i] < ext_[i]` → false → no-op.
- **Tight / Aggressive** (`attackSamples_ << lookaheadSamples_`):
  `idx = i + (la - attackSamples)`, far ahead of `i`. If `anchor < 1.0`
  (recent reduction), the write succeeds and propagates that anchor
  value forward. The next input's anchor (`ext_[i+1]`) reads a still-low
  value when the read position reaches that far-future write — and the
  cycle continues into `historyPost_` and across blocks. Release is
  permanently pinned.

avishali's audition repro: push Drive until GR engages → release Drive →
GR meter stays pinned indefinitely. Toggling `limiter_active` off (which
skips the entire envelope chain) is the only way to reset it. Confirms
the cycle is internal to `ext_` propagation, not the smoothing stage.

The exponential smoothing (`s2 = alpha * s2 + (1 - alpha) * inp`) is
correct; it can't release because `inp = ext_[j]` never returns to 1.0.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ only):**
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` — `process()`: change
  the inner ramp loop bound from `k <= attackSamples` to `k <
  attackSamples` and drop the `(k == attackSamples ? 0.0f : ...)`
  conditional. Anything else stays.

**DO NOT TOUCH:**
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h`
- Any product file
- Any bench file
- Any other HQ source

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

In `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`, replace:

```cpp
for (int k = 0; k <= attackSamples; ++k)
{
    const int idx = i + la - k;
    if (idx < 0 || idx >= n + la)
        continue;

    const float shape = (k == attackSamples) ? 0.0f : attackTable_[(size_t) (attackSamples - 1 - k)];
    const float tentative = anchor + (required - anchor) * shape;

    if (tentative < ext_[(size_t) idx])
        ext_[(size_t) idx] = tentative;
}
```

with:

```cpp
for (int k = 0; k < attackSamples; ++k)
{
    const int idx = i + la - k;
    if (idx < 0 || idx >= n + la)
        continue;

    const float shape = attackTable_[(size_t) (attackSamples - 1 - k)];
    const float tentative = anchor + (required - anchor) * shape;

    if (tentative < ext_[(size_t) idx])
        ext_[(size_t) idx] = tentative;
}
```

Two changes:
1. Loop bound `k <= attackSamples` → `k < attackSamples`.
2. The `(k == attackSamples) ? 0.0f : ...` ternary collapses to the
   straight `attackTable_[attackSamples - 1 - k]` lookup, because the
   guarded case can no longer occur.

No header changes. No new fields. No new methods.

────────────────────────────────────────
4. BUILD + BENCH
────────────────────────────────────────

```bash
# Build HQ-consuming product (Release VST3 + AU — for avishali's audition):
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build --config Debug
```

Bench regression — Slice 3, 4, 5 must remain **bit-identical PASS** because
the dropped iteration was a no-op in Clean mode (the bench driver pins
`character = "Clean"` after Slice 9.1):

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE09_2_REGRESSION_S$SLICE"
done
```

All three must end `PASS N/N` (Slice 3: 13/13, Slice 4: 14/14, Slice 5:
25/25). If any fails, STOP and report — that would mean the change is
not actually a no-op in Clean and we need to revisit.

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
LimiterEnvelope: fix stuck release in Tight/Aggressive (Slice 9 fallout)

The per-input attack-ramp loop wrote the trailing-edge iteration
(k == attackSamples, shape = 0) as tentative = anchor into ext_[i + la
- attackSamples]. In Clean mode, attackSamples_ == lookaheadSamples_,
so idx == i and the write was a self-comparison (no-op). In Tight and
Aggressive (Slice 9), attackSamples_ << lookaheadSamples_, so the same
write propagated the current anchor forward by (la - attackSamples)
samples. Once anchor was below 1.0 (after any real reduction), this
chained into a self-perpetuating low value carried across blocks via
historyPost_, pinning ext_ below 1.0 forever and making the exponential
release smoothing unable to recover.

Fix: drop the trailing-edge iteration entirely (k < attackSamples). The
loop now writes only the strictly-shaping portion of the raised-cosine
attack ramp. Bit-identical to the previous behaviour in Clean (the
dropped iteration was a no-op there) and lets ext_ return to 1.0 once
peaks subside in Tight/Aggressive, so the s2 release coefficient
actually acts.

Slice 3/4/5 bench regression unchanged (Clean): PASS 13/13, 14/14, 25/25.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push HQ. Do NOT amend `bbd4293` or `3dfee43`.** Stack of three
local HQ commits is now: this fix on top of `3dfee43` (bench driver pin)
on top of `bbd4293` (ADR-0006 + Mode enum) on top of `276c397`.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. The exact before/after of the inner ramp loop.
2. Build success line for both Release and Debug.
3. Bench regression: invocation + final `PASS N/N` for Slice 3 / 4 / 5.
4. HQ `git log --oneline -4` showing the new commit on top of `3dfee43`
   on top of `bbd4293` on top of `276c397`.
5. Confirmation: HQ NOT pushed; product repo NOT touched; no other HQ
   files modified.
6. Open questions.

────────────────────────────────────────
7. AFTER THIS
────────────────────────────────────────

avishali re-auditions Slice 9 in Live (Release VST3):
- Push Drive until GR engages, release Drive → GR returns to 0 within
  ~ release_ms × few (default 100 ms release → recovery in ~300–500 ms).
- Tight default feel = brickwall, no sticking.
- Aggressive feel = snappier than Tight, no sticking.
- Clean feel = unchanged from Slice 5 (transparent T/S).
- Clipper Drive 0 → +6 → +12 dB stacks correctly, GR responsive.
- limiter_active toggle still cleanly bypasses.

On approval, the Slice 9 close prompt runs (single clean product commit),
then I FF main + push.
