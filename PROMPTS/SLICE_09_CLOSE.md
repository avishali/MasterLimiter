# Cursor Task — Slice 9 Close

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Slice 9 close: push HQ, finalize product docs (PROGRESS + PLAN),
> archive Slice 9 design prompts, push product.** The DSP and bench
> work is done and PASS. This slice is paperwork + publish.
>
> No code changes. No bench reruns. No DSP source touches. Just docs,
> archives, and pushes.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Verify
current branch in each repo before pushing. **Do not force-push.** Do
not skip hooks. Do not bypass signing. If `git push` requires
authentication and the credential helper is not configured for the
session, STOP and report — avishali will run the push manually.

────────────────────────────────────────
1. SCOPE — files
────────────────────────────────────────

**HQ — push only, no edits.**

**Product — edits:**
- `docs/PROGRESS.md` — new Slice 9 entry (reverse-chronological,
  most-recent at top per existing convention).
- `PROMPTS/PLAN.md` — Slice 9 row marked done; Slice 7 promoted to
  "next"; backlog rows added for envelope snap smoother and
  multiband detection (with Slice 9.6c rationale references).
- `PROMPTS/SLICE_09_*.md` — design prompts from this session, currently
  uncommitted, archived as part of the close paper trail.

**DO NOT TOUCH:**
- Any HQ DSP source.
- Any product DSP / processor / UI source.
- `criteria.py`, `bench.py`, or any bench file.

────────────────────────────────────────
2. PHASE 1 — HQ push
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# Existing uncommitted MCP dirt is unrelated to this slice — leave it.

git rev-parse --abbrev-ref HEAD
# Capture the branch name.

git log --oneline origin/$(git rev-parse --abbrev-ref HEAD)..HEAD
# Should show the 8 commits from this Slice 9 session, top to bottom:
#   d805b2b dsp_bench: Slice 3/4/5 recal (treatment B) + Ozone 11 reference driver
#   11fdb70 LimiterEnvelope: cap Clean attack at 3 ms (CPU win after 4x OS)
#   fcbe9db LimiterEnvelope: fast-path non-peak samples in ramp loop (16x CPU win at 4x OS)
#   7ca50ac LimiterEnvelope: ramp anchor 1.0 (kill ext_ release propagation)
#   8f53371 LimiterEnvelope: T/S combine min -> max (fast release dominates)
#   763604e LimiterEnvelope: fix stuck release in Tight/Aggressive (Slice 9 fallout)
#   3dfee43 dsp_bench/master_limiter driver: pin character by name ("Clean")
#   bbd4293 ADR-0006 + LimiterEnvelope::Mode (Clean/Tight/Aggressive)

git push origin HEAD
# Standard push (no force, no skip-hooks).

git log --oneline -10
```

Report the post-push log. If any push step requires interactive auth,
STOP and report which step.

────────────────────────────────────────
3. PHASE 2 — Product docs (PROGRESS + PLAN)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect:
#    ?? PROMPTS/SLICE_09_*.md
#    ?? PROMPTS/SLICE_09_character_and_onoff.md  (if also uncommitted)
# Should not include any source file changes.
```

### 3.1 PROGRESS.md entry

Open `docs/PROGRESS.md`. Find the existing reverse-chronological list
(most-recent entry first). Insert the following Slice 9 entry **at the
top** of the slice list, above the most recent existing entry:

```markdown
## Slice 9 — Limiter character + Clipper Drive + on/off (2026-05-29)

**Shipped:**

Product (`MasterLimiter`):
- New params: `limiter_active` (bool, default true), `clipper_drive_db`
  (float, 0..+12 dB, default 0).
- Character (`character`) expanded from 1 option to 3: Clean, Tight,
  Aggressive. Default Tight.
- Clipper Drive stage: pre-envelope hard-clip at ±1.0 with per-block
  pre-clip excess (dB) and LED indicator in UI.
- 4× oversampling around the entire limiter chain (Clipper + Peak +
  Envelope + Lookahead + Gain). Reuses `juce::dsp::Oversampling<float>`
  with halfband FIR equiripple (proven primitive from
  `TruePeakDetector`/`IspTrimStage`).
- ispTrim removed from the audio path; TP mode now uses a 0.3 dB
  envelope-side headroom (`ceiling × 0.965`) so the limiter's single
  OS downsample FIR ringing stays below ceiling. Latency unified to
  301 samples at 48k (was 240 SP / 301 TP pre-9.4, then 301 SP / 362
  TP post-9.4).
- `release_ms` default lowered 100 → 30 ms to match the new fast-
  brickwall character.

HQ (`mdsp_dsp::LimiterEnvelope`):
- `Mode` enum (Clean/Tight/Aggressive) with mode-specific attack
  windows (Clean = min(lookahead, 3 ms) raised cosine; Tight = 1 ms;
  Aggressive = 0.3 ms). Constant latency across modes.
- Ramp algorithm rewritten over several iterations:
  - 9.2: trailing-edge propagation fix (`k < attackSamples`).
  - 9.3a.1: ramp tentative formula uses 1.0 baseline instead of
    `anchor = ext_[i]` — kills second-order propagation chain that
    was producing 200 ms – 4 s effective release in Tight/Aggressive.
  - 9.4.1: outer-loop fast-path skips inner ramp when `pk <= thr`
    (the dominant CPU win after 4× OS).
- T/S split combinator flipped `min → max` (9.3a). Fast envelope now
  dominates; release tracks `release_ms` knob directly.
  `release_sustain_ratio` is mathematically inert under `max()` —
  retained as a frozen-ID param reserved for a future real T/S split
  slice with program-dependent transient detection.

HQ (`dsp_bench`):
- `master_limiter` driver pins character by name (`"Clean"`) for
  Slice 3/4/5 regression so future choice-list expansions can't break
  pinning by index.
- Slice 3/4/5 criteria recalibrated against new envelope semantics and
  4× OS:
  - Latency 240 → 301 at 48 kHz (SP and TP equal post-9.6).
  - TP overs threshold loosened modestly (cascaded-OS removed; small
    single-FIR ringing residual).
  - SP overs in SP mode widened (OS downsample FIR ringing inherent;
    TP mode passes original thresholds).
  - Null residual and SMPTE IMD widened: fast-release brickwall
    inherently shows more sidebands than slow release. Reference
    Ozone 11 Maximizer IRC IV measured on the same synthetic corpus
    scores 4.3–7% IMD and −19 to −22 dB null residual; our 6.5–8.8%
    IMD and −11 to −15 dB null residual are in family
    (~25–50% wider, not 5–10× wider). Each new threshold carries an
    inline Ozone reference comment in `criteria.py`.
  - Slice 5 T/S ratio assertions removed (combinator-inert).
- New `ozone_maximizer` driver added as bench tooling for future
  reference runs. Pinned to Modern/IRC IV semantics (output −1 dBFS,
  linked stereo, soft-clip off). IRC mode is not exposed in Ozone 11's
  VST3 param API — driver relies on Ozone's default and notes the
  caveat in code.

**ADRs:**
- ADR-0006 (Limiter character + Clipper Drive + on/off) — accepted
  2026-05-28.

**Gate status:**
- Builds clean (Release + Debug) at every iteration.
- All three Character modes audition-approved by avishali in Live
  (Release VST3). Default Tight reads as brickwall, Clean reads as
  transparent, Aggressive reads as snappy. Clipper Drive 0..+12 dB
  stacks with each mode; Clip readout/LED tracks pre-clip excess.
- Limiter on/off toggle is byte-equivalent bypass when off (no OS
  call), no glitches on toggle.
- CPU on a dense mix at default settings: ~25% in Tight,
  ~40–50% in Clean (after 9.4.1 + 9.4.2 optimizations from the post-
  9.4 88% CPU regression).
- Bench: Slice 3 PASS 13/13, Slice 4 PASS 14/14, Slice 5 PASS 25/25.
- Reference Ozone comparison documented; remaining gap addressable
  via Slice 7 (stereo unlink) and future multiband detection
  (estimated ~7 dB null residual closer to reference).

**Followups noted in PLAN backlog:**
- Slice 7 (Stereo Unlink) — promoted to next, primary lever for the
  "wide" gap vs Ozone.
- Multiband detection (new slice TBD, ADR-0008 first) — primary lever
  for the "open" gap vs Ozone IRC IV.
- Envelope snap-event smoother (proposed during this slice as 9.6b) —
  Ozone reference evidence showed it's only a ~2 percentage-point IMD
  lever, not the dominant gap; demoted to backlog.
- ispTrim 4× FIR primitive retained in HQ for any future product that
  wants it, unhooked from MasterLimiter's processBlock.
```

If the existing PROGRESS.md uses a different heading level or date
format, adapt the new entry to match. The content above is the
authoritative version of what shipped.

### 3.2 PLAN.md update

Open `PROMPTS/PLAN.md`. Find the Slice 9 row in the table. Update its
status to **shipped** per the existing convention (whatever symbol the
table uses for done — e.g. ✅, "Done", or similar). The gate-headline
column should reflect the actual ship state: replace the prior
provisional wording with:

```
Shipped 2026-05-29. 3 Character modes, Clipper Drive stage, limiter
on/off, 4× OS limiter chain, ispTrim removed, TP headroom in envelope.
Bench Slice 3/4/5 PASS via treatment-B recalibration; Ozone IRC IV
reference run documents the in-family character. ADR-0006 + ADR-0007.
```

Find the "next slice" / sequence marker — promote **Slice 7 (Stereo
Unlink)** to the immediate next position. The PLAN row for Slice 7
already exists; just update any "next slice" pointer to point at it.

Add two new backlog rows (or a backlog section if the file has one)
below the active slice list:

```
| Backlog | Multiband detection (detection-bus only, no audio-path
crossover phase issues) | new ADR-0008 | new product params | Closes
the ~7 dB null-residual gap and "open" perception gap vs Ozone IRC
IV documented in Slice 9.6c reference run. Avoids ADR-0005's LR4
phase concerns by keeping the split detection-only. |
| Backlog | Envelope snap-event smoother (was proposed Slice 9.6b) |
`mdsp_dsp::LimiterEnvelope` | none | Demoted from Slice 9 close per
Ozone evidence: only ~2 percentage-point IMD lever vs the 4–7 percent
gap to in-family numbers. Revisit if a different future motivation
surfaces. |
| Backlog | Optional SP-mode envelope headroom (parity with TP) |
product processBlock | none | Slice 9 SP mode accepts small 1× SP
overs from OS downsample ringing; TP mode bounds via 0.3 dB headroom.
If users want bulletproof SP output, add a tiny headroom in SP too.
Low priority. |
```

Match the existing table column structure exactly (column count,
header order). If the PLAN's backlog isn't tabular, append as a
"Backlog" bullet list in whatever style the file uses.

### 3.3 Product close docs commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git add docs/PROGRESS.md PROMPTS/PLAN.md
git status --short
# expect ONLY:
#    M  docs/PROGRESS.md
#    M  PROMPTS/PLAN.md
#    ?? PROMPTS/SLICE_09_*.md   (handled in Phase 3)

git commit -m "$(cat <<'EOF'
Slice 9 close: PROGRESS entry + PLAN update

Documents the shipped state: 3 Character modes, Clipper Drive stage,
limiter on/off, 4x OS limiter chain, ispTrim removed with TP envelope
headroom. Bench Slice 3/4/5 PASS via treatment-B recalibration;
reference Ozone 11 Maximizer IRC IV measured on the same corpus
documented in criteria comments.

PLAN: Slice 9 marked shipped, Slice 7 (Stereo Unlink) promoted to
next. Backlog rows added for:
- Multiband detection (closes ~7 dB null-residual gap; new ADR-0008).
- Envelope snap-event smoother (demoted per Ozone evidence: ~2pp
  IMD lever, not the dominant gap).
- Optional SP-mode envelope headroom (parity with TP).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

────────────────────────────────────────
4. PHASE 3 — Slice 9 prompts archive
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect ONLY:
#    ?? PROMPTS/SLICE_09_character_and_onoff.md         (if uncommitted)
#    ?? PROMPTS/SLICE_09_1_*.md
#    ?? PROMPTS/SLICE_09_2_*.md
#    ?? PROMPTS/SLICE_09_3a_*.md
#    ?? PROMPTS/SLICE_09_3a_1_*.md
#    ?? PROMPTS/SLICE_09_4_*.md
#    ?? PROMPTS/SLICE_09_4_1_*.md
#    ?? PROMPTS/SLICE_09_4_2_*.md
#    ?? PROMPTS/SLICE_09_5_*.md
#    ?? PROMPTS/SLICE_09_5b_*.md
#    ?? PROMPTS/SLICE_09_6_*.md
#    ?? PROMPTS/SLICE_09_6c_*.md
#    ?? PROMPTS/SLICE_09_CLOSE.md   (this file)

git add PROMPTS/SLICE_09_*.md
git status --short
# expect ONLY the above as A (staged).

git commit -m "$(cat <<'EOF'
Slice 9: archive design prompts

Substantial design history for Slice 9 (character + Clipper + on/off
through OS consolidation and bench recalibration). Twelve prompt
files spanning ADR refinement, iterative DSP fixes, CPU optimization
after 4x OS, and the Ozone reference run that informed treatment-B
closure.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

If any of the listed files don't exist (e.g. the original
`SLICE_09_character_and_onoff.md` is already committed from earlier),
omit it from the `git add` glob and adjust accordingly. The
`PROMPTS/SLICE_09_*.md` glob should pick up everything Slice-9-related.

────────────────────────────────────────
5. PHASE 4 — Product push
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git rev-parse --abbrev-ref HEAD
# Capture branch.

git log --oneline -8
# Expect (top to bottom):
#   <prompts archive commit>
#   <PROGRESS+PLAN commit>
#   4e394e5 MasterLimiter: consolidate oversampling - remove ispTrim, TP headroom in envelope
#   64db6e7 MasterLimiter: 4x oversample the limiter chain (Clipper + Envelope + Gain)
#   ce9a478 MasterLimiter Slice 9 character/on-off controls + clip meter polish
#   645779d MasterLimiter Slice 11b1: ...

# If on a branch and a main FF is needed:
#   git checkout main
#   git merge --ff-only <branch>
#   git push origin main
# If already on main:
#   git push origin main

# Cursor picks the right path based on current branch.
```

If a non-FF merge is required (main has commits not in current branch),
STOP and report — that's an unexpected state for this workflow and
avishali decides how to reconcile.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. HQ branch name, `git log --oneline -10` after push, confirmation
   push succeeded.
2. Product PROGRESS.md diff summary (lines added; placement at top of
   list confirmed).
3. Product PLAN.md diff summary (Slice 9 row updated; Slice 7
   promoted; backlog rows added).
4. Product commits: SHA + title for the two new commits (docs commit,
   prompts archive commit).
5. Product branch name, `git log --oneline -8` after push, confirmation
   push succeeded.
6. Remaining dirty state in either repo (expect: HQ MCP unrelated
   dirt; product clean).
7. Any auth/permission/non-FF issues encountered → reported, not
   force-resolved.
8. Open questions.

────────────────────────────────────────
7. AFTER THIS
────────────────────────────────────────

Slice 9 is closed and shipped to both remotes. Next session:

- I open the **Slice 7 (Stereo Unlink) interview** to scope the
  implementation. PLAN entry already exists; we just need to pin
  param names, default values, and the detection-side topology
  decision (per-channel envelopes vs M/S detection vs continuous
  link % blend).
- avishali audits the live product in mix sessions to surface any
  Slice-9-related issues that didn't show in audition (real-world
  material can expose what synthetic corpora miss).
- Multiband detection ADR-0008 sketched in parallel as a backlog
  exploration; not on the critical path until Slice 7 closes.
