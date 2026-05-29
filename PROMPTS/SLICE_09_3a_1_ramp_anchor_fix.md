# Cursor Task — Slice 9.3a.1: LimiterEnvelope — ramp anchor → 1.0 (kill ext_ release propagation)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ DSP fix only. Audition continuation of 9.3a — no bench run this
> slice; bench recalibration is still Slice 9.3b after audition pass.**
>
> 9.2 fixed the worst case of the ext_ propagation (trailing-edge
> k == attackSamples writing anchor into idx = i + la - attackSamples).
> 9.3a flipped min → max in the T/S smoothing combinator so release_ms
> would actually drive release time. avishali's audition shows the
> release is **still slow** — Tight especially, ~4 seconds intrinsic.
>
> Root cause traced to the same ramp formula, one layer deeper. The ramp
> writes `tentative = anchor + (required - anchor) * shape`. At small
> shape values (the trailing portion of the ramp we kept in 9.2), this
> approximates `anchor`. When anchor is a deep reduction (because
> ext_[i] was reduced by an earlier peak), the ramp propagates that deep
> value forward into `ext_[i + la - k]` for small k. Each subsequent
> input picks up the propagated value as its own anchor and propagates
> again. ext_ never returns to 1.0; the smoothing stage just tracks it.
>
> Per-mode predicted intrinsic release: Clean ~240 ms, Aggressive ~370
> ms, Tight ~4 s. Matches avishali's audition exactly (Tight slowest,
> others faster but still slow).
>
> Fix: stop using anchor in the tentative formula. Use 1.0 as the
> "no-reduction" baseline so each input contributes only its OWN ramp,
> never propagates a previous reduction forward. The min-write guard
> preserves earlier-written deeper values; the smoothing stage handles
> release with release_ms as designed.
>
> **HQ-only. No product touch. No push. No amend.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ DSP file
only. No product edits. No bench edits. No HQ push. No amend of
`8f53371`, `763604e`, `3dfee43`, `bbd4293`, or any earlier HQ commit.

────────────────────────────────────────
1. WHY (short version — full math in the preamble above)
────────────────────────────────────────

In `LimiterEnvelope::process`, the ramp uses `anchor = ext_[i]` as the
start point: `tentative = anchor + (required - anchor) * shape`. At small
shape values, tentative ≈ anchor. When anchor is a deep reduction from
an earlier peak, this writes the deep value forward into `ext_[idx]`
positions that no peak has reached. Next iteration's anchor reads from
those propagated positions and writes them forward again. Result: a
"chain" of slowly-relaxing reductions that takes hundreds of ms to
seconds to clear, independent of release_ms.

Replacing anchor with 1.0 in the tentative formula breaks the chain.
Each input's ramp contributes only its own reduction (required) at the
deep end, fading to 1.0 (no reduction) at the shallow end. ext_ returns
to 1.0 wherever no peak's ramp wrote a deep value. The min-write guard
(`if (tentative < ext_[idx])`) is unchanged, so genuine overlapping
peaks still write the deeper of two competing reductions, and the
smoothing stage (s2 with alpha derived from release_ms) handles release
as documented.

The audible difference from the old anchor-based ramp: when a new deep
peak follows an existing reduction, the old code would write a smooth
ramp from "current state" down to the new required value. The new code
writes only the portion below 1.0 down to required; the existing
reduction holds at its prior value and the new ramp meets it where they
cross. For a brickwall this is correct — and for the Slice 5 bench's
T/S split case (which is Clean only, and Clean has T/S now inert under
the 9.3a max() flip), the behaviour change is moot.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ):**
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` — `process()`: replace
  the `anchor`-based tentative formula with a `1.0f`-based one. The
  `const float anchor = ext_[(size_t) i];` line becomes unused and is
  removed.

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
    const float anchor = ext_[(size_t) i];

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
}
```

Replace with:

```cpp
for (int i = 0; i < n; ++i)
{
    const float pk = std::max (0.0f, peakIn[i]);
    const float required = (pk > thr) ? (thr / pk) : 1.0f;

    for (int k = 0; k < attackSamples; ++k)
    {
        const int idx = i + la - k;
        if (idx < 0 || idx >= n + la)
            continue;

        const float shape = attackTable_[(size_t) (attackSamples - 1 - k)];
        const float tentative = 1.0f + (required - 1.0f) * shape;

        if (tentative < ext_[(size_t) idx])
            ext_[(size_t) idx] = tentative;
    }
}
```

Two changes:
1. Removed `const float anchor = ext_[(size_t) i];` line.
2. `tentative = anchor + (required - anchor) * shape` →
   `tentative = 1.0f + (required - 1.0f) * shape`.

No header changes. No new fields. No new methods. No changes outside
the `process()` function.

────────────────────────────────────────
4. BUILD (no bench)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean, no new warnings.

**Do NOT run the bench this slice.** Slice 5 criteria are still
calibrated to pre-9.3a behaviour. Bench recalibration is Slice 9.3b
after avishali signs off on audition feel.

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
LimiterEnvelope: ramp anchor 1.0 (kill ext_ release propagation)

Slice 9.2 fixed the worst case of the attack-ramp propagation (trailing-
edge k == attackSamples writing anchor into idx = i + la - attackSamples).
A second-order propagation remained: at small but nonzero shape values
(the still-kept k = attackSamples - 1 iteration in particular), tentative
= anchor + (required - anchor) * shape approximates anchor. When anchor
is a deep reduction (ext_[i] reduced by an earlier peak's ramp), the
ramp writes that deep value forward into ext_[idx] for the small-shape
positions. The next iteration reads the propagated value as its anchor
and propagates it further, forming a chain that takes hundreds of ms to
seconds to clear via tiny per-hop relaxation. release_ms and the s2
smoothing cannot release faster than ext_ feeds them, so the audible
release is dominated by the ext_ chain rather than the configured release
time.

Per-mode intrinsic release predicted by the chain math (la = 240,
release_ms irrelevant):
- Clean (attackSamples = 240, 1-sample hops):      ~240 ms
- Aggressive (attackSamples = 14, 226-sample hops): ~370 ms
- Tight (attackSamples = 48, 193-sample hops):     ~4 s

avishali's audition matches: Tight slowest, Aggressive/Clean faster but
all still slow. The 9.3a max() flip exposed release_ms to the smoothing
output but smoothing input (ext_) was the actual bottleneck.

Fix: stop using anchor in the ramp tentative formula. tentative = 1.0f +
(required - 1.0f) * shape. Each input contributes only its own ramp,
from 1.0 (no reduction) at the shallow end to required at the deep end.
ext_[idx] retains a prior deeper reduction wherever one was genuinely
written, via the unchanged min-write guard. No propagation. ext_ returns
to 1.0 wherever no input wrote a deep value. Release is then governed
by the s2 smoothing with alpha from release_ms, as designed.

Audible behaviour change for overlapping peaks: where the old code
ramped smoothly from current-state down to the new required value, the
new code writes only the below-1.0 portion of the new ramp; the existing
reduction holds at its prior value and the new ramp meets it where they
cross. For a brickwall this is correct - reduction stays put until a
new attack ramp dives below it. The smoothing stage continues to handle
release exponentially.

Slice 3/4/5 bench not run this slice; criteria were calibrated to the
old anchor-propagation behaviour and need recalibration in 9.3b after
avishali audition pass on the new feel.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push HQ. Do NOT amend any prior HQ commit.** Stack is now:
this fix on top of `8f53371` (min→max) on top of `763604e` (trailing-edge
fix) on top of `3dfee43` (bench pin) on top of `bbd4293` (mode enum) on
top of `276c397`.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Exact before/after of the two changed lines (anchor removal + tentative
   reformulation).
2. Build success lines for Release and Debug.
3. HQ `git log --oneline -6` showing the new commit on top of `8f53371`
   on top of `763604e` on top of `3dfee43` on top of `bbd4293` on top of
   `276c397`.
4. Confirmation: HQ NOT pushed; product repo NOT touched; bench NOT run.
5. Open questions.

────────────────────────────────────────
7. AFTER THIS
────────────────────────────────────────

avishali re-auditions in Live (Release VST3):
- Tight with release_ms = 30 ms should now release in ~30–60 ms after
  GR engages. Snappy brickwall feel. No multi-second hangover.
- Clean and Aggressive should also release at ~release_ms scale, snap
  back cleanly after transients.
- Sweep release_ms 10 → 300 ms in each mode and confirm release time
  audibly tracks the knob across the full range.
- Clipper Drive + Character stacking unchanged; Clip readout/LED
  unchanged.

On pass → Slice 9.3b: recalibrate Slice 5 bench criteria against the
new (faster, propagation-free) envelope, re-run Slice 3/4/5 regression.
Then Slice 9 close (single product commit already mostly captured in
`ce9a478`; we may amend or stack a small finalizer commit), FF main,
push.

On fail → escalate to Option B as previously discussed: drop the
parallel-cascade T/S topology entirely, implement program-dependent
release detection in its own slice.
