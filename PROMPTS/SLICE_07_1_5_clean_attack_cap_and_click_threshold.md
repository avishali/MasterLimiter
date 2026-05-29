# Cursor Task — Slice 7.1.5: cap Clean attack to 1.5 ms + raise click threshold back to 0.5

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Two small fixes bundled. Two separate commits (HQ + product).**
>
> 1. **HQ**: cap Clean attack 3 ms → 1.5 ms. Same shape as Slice 9.4.2's
>    5 → 3 ms cap. Halves the Clean inner-loop work, reducing the
>    visible CPU↔GR correlation avishali noted (less spiky than pre-
>    fix; closer to Ozone aesthetic). Env max under stress should
>    drop from ~4400 µs to ~2200 µs.
>
> 2. **Product**: raise output click detector threshold 0.2 → 0.5.
>    Slice 7.1.3 lowered to 0.2 to chase subtle clicks during the pop
>    investigation. With pops resolved (7.1.4.1) and the audible
>    issue gone, the 0.2 threshold catches legitimate brickwall
>    transients as "clicks", making the counter look alarming when
>    nothing's wrong. Back to 0.5 so the counter idles at 0 unless
>    something is genuinely off.
>
> Bench Slice 3/4/5 must PASS after the Clean attack change. Float-
> rounding tolerance and Slice 9.5b's "fast-release character" margin
> already accommodate small shifts in null residual / IMD.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ DSP
one-line change + product one-line change. No bench recalibration.
No push. No amend of any prior commit.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

### 1.1 Cap Clean attack 1.5 ms

Slice 9.4.2 capped Clean attack at 3 ms (from `lookaheadSamples_`)
specifically to reduce CPU on dense material; rationale was "Pro-L
Smooth/Transparent styles use 2–3 ms even with longer lookahead;
audible difference below threshold for mastering material."

avishali's 7.1.4.1 audition confirmed pops resolved across all modes
but the visible CPU↔GR correlation in Ableton's CPU meter is still
noticeable on heavy Clean reduction (Env section ~4400 µs at stress).
Ozone 11 IRC IV doesn't show this correlation — partly because of
multiband detection (architectural, future ADR-0009 backlog), partly
because their Modern attack windows are closer to 1–1.5 ms.

Tightening Clean from 3 ms to 1.5 ms is a one-line change in
`recomputeAttackSamples()` (same place as 9.4.2). Cuts inner-loop
work by 2×. Expected impact:
- Env max under stress: ~4400 µs → ~2200 µs.
- CPU bar correlation with GR: visibly milder.
- Character: tiny perceptual delta. Raised-cosine over 1.5 ms is
  still soft. Pro-L "Smooth" is ~1.5 ms. avishali's earlier
  audition of the 3 ms version was approved as transparent.

If post-audit the new Clean character feels too sharp, revert to
3 ms is one line — addressable in a quick follow-up.

### 1.2 Click threshold 0.2 → 0.5

Slice 7.1.3 lowered the output-click detector threshold from 0.5 to
0.2 specifically to catch subtle clicks during pop investigation
("the 0.45 D high-water-mark almost-but-not-quite caught something").

Now that 7.1.4.1 resolved the actual pop source (SIMD-deficient
inner loop hitting deadline at Clean unlinked), the underlying
audible clicks are gone. But the 0.2 threshold catches **legitimate
brickwall transients** at the ceiling as "clicks" — sample-to-sample
deltas in the 0.2–0.5 range are normal limiter behavior on transient
material.

Result: avishali's 7.1.4.1 screenshot shows Clicks 1278 with audio
perceptually clean. The counter is noise, not signal.

Raising back to 0.5 makes the counter idle at 0 during normal
limiting and non-zero only when something is genuinely off. Better
signal-to-noise for diagnosing future regressions.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ):**
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` — Clean case in
  `recomputeAttackSamples()`: `0.003` → `0.0015`.

**MODIFY (product):**
- `Source/PluginProcessor.cpp` — `kClickThreshold`: `0.2f` → `0.5f`.

**DO NOT TOUCH:**
- `shared/mdsp_dsp/include/...` — no header change.
- `Source/PluginProcessor.h` — no atomic / getter change.
- `Source/ui/*` — no UI change (debug readout still shows the
  counter, just with the higher threshold).
- Bench source.
- Any other DSP / product file.
- Any param.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

### 3.1 HQ — Clean attack cap

In `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`, in
`recomputeAttackSamples()`, locate the Clean case:

```cpp
case Mode::Clean:
    attackSamples_ = std::min (lookaheadSamples_,
                               std::max (1, (int) std::llround (0.003 * sampleRate_)));
    break;
```

Change `0.003` to `0.0015`:

```cpp
case Mode::Clean:
    attackSamples_ = std::min (lookaheadSamples_,
                               std::max (1, (int) std::llround (0.0015 * sampleRate_)));
    break;
```

Tight and Aggressive cases unchanged. The defensive `std::min` against
`lookaheadSamples_` remains as in 9.4.2.

### 3.2 Product — raise click threshold

In `Source/PluginProcessor.cpp`, locate the click threshold constant
introduced in Slice 7.1.2 and lowered in 7.1.3:

```cpp
constexpr float kClickThreshold = 0.2f;
```

Change to:

```cpp
constexpr float kClickThreshold = 0.5f;
```

All other click-detection logic stays identical. The atomic
`outputClickCount_`, the per-block scan, the `lastClick*` snapshot,
and the UI display are unchanged.

────────────────────────────────────────
4. BUILD
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean.

────────────────────────────────────────
5. BENCH (must remain PASS)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE07_1_5_REGRESSION_S$SLICE"
done
```

All three must PASS 13/13, 14/14, 25/25.

The Clean attack change shifts envelope behaviour slightly — shorter
attack ramp, slightly tighter transient response. Slice 9.5b's
treatment-B thresholds for null residual / IMD / SP overs were set
with margin for fast-release character; a tighter attack may improve
some metrics (tighter null) and not affect others. Pass-rows should
stay pass.

If any row fails, STOP and report which metric. Most likely
candidate would be an IMD or noise residual tightening that pushes
into a different number, but the margins should be ample.

────────────────────────────────────────
6. COMMITS — two separate, no push
────────────────────────────────────────

### 6.1 HQ commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# expect ONLY:
#    M shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp

git add shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp

git commit -m "$(cat <<'EOF'
LimiterEnvelope: cap Clean attack 3 ms -> 1.5 ms (CPU correlation reduction)

Slice 7.1.4.1 resolved the audible pops via NEON SIMD in the inner
ramp loop. avishali audition confirmed pops gone across all
Character modes and Link values. Remaining concern: visible CPU↔GR
correlation in Ableton's CPU meter on heavy Clean reduction (Env
section ~4400 us under stress, 80%+ of 5333 us deadline at 48k/256).
Ozone 11 IRC IV reference doesn't show this correlation, partly due
to multiband detection (future ADR-0009 backlog) and partly due to
shorter attack windows in Modern modes (~1-1.5 ms).

Cap Clean's attack window at 1.5 ms (was 3 ms after Slice 9.4.2,
originally lookaheadSamples_). Same single-line shape as 9.4.2.
Halves Clean inner-loop work per peak sample:
- Env max under stress: ~4400 us -> ~2200 us.
- CPU bar correlation visibly milder.
- Character delta tiny: raised-cosine over 1.5 ms is still soft,
  matches Pro-L "Smooth" style attack window.

Tight (1 ms) and Aggressive (0.3 ms) cases unchanged. Defensive
std::min against lookaheadSamples_ retained.

Slice 3/4/5 bench PASS 13/13, 14/14, 25/25 unchanged.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### 6.2 Product commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect ONLY:
#    M Source/PluginProcessor.cpp

git add Source/PluginProcessor.cpp

git commit -m "$(cat <<'EOF'
MasterLimiter Slice 7.1.5: click threshold 0.2 -> 0.5

Slice 7.1.3 lowered the output click detector threshold from 0.5 to
0.2 to chase subtle clicks during the pop investigation. Slice
7.1.4.1 resolved the actual pop source (SIMD-deficient inner loop
hitting deadline at Clean unlinked); audible pops are gone.

The 0.2 threshold catches legitimate brickwall transients at the
ceiling as "clicks" — sample-to-sample deltas in the 0.2-0.5 range
are normal limiter behaviour on transient material. avishali's
7.1.4.1 audition showed Clicks 1278 with audio perceptually clean —
counter noise, not signal.

Raise back to 0.5: counter idles at 0 during normal limiting,
non-zero only when something is genuinely off. Better signal-to-
noise for diagnosing future regressions.

Atomic, per-block scan, lastClick snapshot, and UI display all
unchanged. Just the comparison constant.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push HQ. Do NOT push product. Do NOT amend any prior commit.**

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. HQ diff: Clean case in `recomputeAttackSamples()` before/after.
2. Product diff: `kClickThreshold` constant before/after.
3. Build success lines for Release and Debug.
4. Bench PASS lines for Slice 3 / 4 / 5.
5. HQ `git log --oneline -8` showing the new commit on top of
   `643ac50`.
6. Product `git log --oneline -8` showing the new commit on top of
   the prior Slice 7.x stack.
7. Confirmation: neither repo pushed; no other files modified.
8. Open questions.

────────────────────────────────────────
8. AFTER THIS
────────────────────────────────────────

avishali audition checklist:

- **Clean mode A/B** vs prior build at moderate GR (1–3 dB): tighter
  attack should be barely audible. If clearly more aggressive, we
  back off to 2 ms in a small follow-up.
- **Heavy GR stress** (Drive +10–17 dB on dense source, Link any
  value): Env max should now show ~2200 µs vs the ~4400 µs in the
  7.1.4.1 screenshot. Block max comfortably under deadline.
- **CPU bar in Ableton** during heavy Clean GR: should be visibly
  less spiky than 7.1.4.1, closer to Ozone aesthetic.
- **Pops**: still none (didn't change pop topology, just CPU
  margin).
- **Clicks counter**: should idle at 0 during normal limiting, only
  rise if something genuinely glitches. Brickwall transient
  snapping no longer counts.

Three outcomes:

**A. Audition passes everything** → I write the **consolidated Slice
7 close prompt** covering the entire 7.x family:
  - Slice 7 (stereo unlink) — `f05c062`
  - 7.1 (2ch meter + latched-max) — `9ca9d61`
  - 7.1.1 (pop diag + em-dash + on/off) — `fb13dba`
  - 7.1.2 (numeric readouts + Clipper LED + click detector) — `b0240d7`
  - 7.1.3 (warm-keep revert + tighter threshold + last-click snapshot) — `724c441`
  - 7.1.4 (SIMD scaffolding) — `52b5e07`
  - 7.1.4.1 (actually-engaged NEON) — `643ac50`
  - 7.1.5 product (this commit)

  Plus HQ:
  - b64f7ae (ADR-0008)
  - d805b2b (Slice 9.5b bench recal + Ozone driver)
  - 52b5e07 (SIMD scaffolding)
  - 643ac50 (NEON engagement)
  - 7.1.5 HQ (this commit, Clean attack cap)

  Single consolidated PROGRESS entry, PLAN update marking Slice 7
  closed and promoting Slice 11b2 to next. Optionally remove the
  debug timing probes in the close commit (avishali's call —
  they're useful diagnostic aids but add small per-block overhead).

**B. Clean character feels too sharp at 1.5 ms** → 7.1.6 backs off to
2 ms (one-line revert). Then close.

**C. Bench fails** → STOP and report. Slice 5's Clean-specific
metrics (used by Slice 5's null residual / IMD rows after 9.5b
treatment-B) may shift more than the margin accommodates. We adjust
thresholds (treatment-B again) or back off the cap.
