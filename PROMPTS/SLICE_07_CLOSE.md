# Cursor Task — Slice 7 Close

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Slice 7 close: push HQ (ADR-0008), finalize product docs (PROGRESS
> + PLAN), archive Slice 7 prompts, push product.** The DSP, UI, and
> bench work is done and PASS at default. avishali audition confirmed
> bit-exact identity at link = 100% and progressive width preservation
> on the sweep.
>
> No code changes. No bench reruns. No DSP source touches. Docs,
> archives, pushes.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Verify
current branch in each repo before pushing. **Do not force-push.** Do
not skip hooks. Do not bypass signing. If `git push` requires
authentication and the credential helper is not configured for the
session, STOP and report.

────────────────────────────────────────
1. SCOPE — files
────────────────────────────────────────

**HQ — push only, no edits.**

**Product — edits:**
- `docs/PROGRESS.md` — new Slice 7 entry at top of the slice list
  (reverse-chronological).
- `PROMPTS/PLAN.md` — Slice 7 row marked shipped; next-slice marker
  promoted to **Slice 11b2** (Auto/Track + Learn + Bypass-with-match,
  deferred since the Slice 11b1 close); multiband detection added/
  confirmed as backlog under ADR-0009 (TBD).
- `PROMPTS/SLICE_07_*.md` — design prompts archived (Slice 7 prompt
  and Slice 7 close prompt itself).

**DO NOT TOUCH:**
- Any HQ DSP source.
- Any product DSP / processor / UI source.
- `criteria.py`, `bench.py`, or any bench file.
- `ms_link_pct` param ID — explicitly left as a deferred placeholder
  per avishali decision; documented in §3.1 below.

────────────────────────────────────────
2. PHASE 1 — HQ push
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# Existing unrelated MCP dirt remains; leave it.

git rev-parse --abbrev-ref HEAD
# Confirm: master

git log --oneline origin/master..HEAD
# Should show ONE commit:
#   b64f7ae ADR-0008: MasterLimiter stereo unlink (per-channel parallel envelopes)

git push origin master

git log --oneline -5
```

Report the post-push log. If push requires interactive auth, STOP and
report.

────────────────────────────────────────
3. PHASE 2 — Product docs (PROGRESS + PLAN)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect:
#    ?? PROMPTS/SLICE_07_stereo_unlink.md
#    ?? PROMPTS/SLICE_07_CLOSE.md
# Should not include any source file changes.
```

### 3.1 PROGRESS.md entry

Open `docs/PROGRESS.md`. Insert the following Slice 7 entry at the
**top** of the slice list, above the most recent existing entry (which
should be the Slice 9 close entry):

```markdown
## Slice 7 — Stereo Link % (per-channel parallel envelopes) (2026-05-29)

**Shipped:**

Product (`MasterLimiter`):
- New param: `stereo_link_pct` (float, 0..100%, step 0.1, default 100,
  frozen ID). Displayed as "XX%" in UI.
- Second `mdsp_dsp::LimiterEnvelope` instance (`envelope_R_`) prepared
  identically to the existing envelope. Per-channel peak / gain
  buffers sized to OS block.
- `processBlock` fast-path at link ≥ 99.95: bit-exact Slice 9.6
  behaviour (single envelope on `max(|L|, |R|)`, shared gain). Default
  100 lands here — Slice 3/4/5 bench PASS unchanged (13/13, 14/14,
  25/25).
- `processBlock` full path at link < 99.95: per-channel envelopes,
  gain blend
  ```
  g_linked = min(g_L, g_R)
  g_out_L  = link · g_linked + (1 − link) · g_L
  g_out_R  = link · g_linked + (1 − link) · g_R
  ```
  ~2× envelope CPU when unlinked (one-time opt-in cost).
- GR meter (`currentGrDb_`) reports max across both envelopes —
  whichever channel hit the deeper reduction. At default 100 /
  fast-path, equals single-envelope GR as before.
- UI: existing "Stereo Link" knob (from earlier UI shell work)
  relabeled to "Link" and APVTS-attached to the new param. Placed in
  the Maximizer panel knob row alongside Character / Clipper Drive.
  "XX%" host display formatter on the param.

HQ:
- ADR-0008 documents the detection topology decision: per-channel
  parallel envelopes, gain blend math, fast-path at link ≥ 99.95 for
  bit-exact default, single continuous param, constant latency,
  detection-bus only (no audio-path crossover; mirrors ADR-0005's
  T/S split reasoning).
- No HQ DSP source edits. `mdsp_dsp::LimiterEnvelope` is reused as a
  stateful component via two product-side instances.

**ADRs:**
- ADR-0008 (Stereo unlink / per-channel parallel envelopes) — accepted
  2026-05-29.

**Gate status:**
- Builds clean (Release + Debug).
- avishali audition: default 100% sounds identical to Slice 9.6
  (fast-path bit-exactness confirmed perceptually). Sweep 100 → 50 →
  0 on wide-stereo source produces progressive width preservation.
  A/B vs Ozone Stereo Unlink: width gap closed into family.
- Constant latency across all link values (301 SP / 301 TP at 48k,
  unchanged from Slice 9.6). No PDC change on Link % automation.
- Bench: Slice 3/4/5 PASS 13/13, 14/14, 25/25 — bit-exact at default
  via the fast-path. No criteria recalibration needed.

**Deferred / placeholders noted:**
- `ms_link_pct` param ID exists in `ParameterIDs.h` from earlier UI
  shell work but is not wired. Left as-is per Slice 7 close decision;
  reserved for a future Slice 7b (M/S detection) if avishali ever
  promotes M/S from backlog. Frozen-ID rule applies — the ID stays.
- M/S detection itself remains deferred per ADR-0008 §Alternatives
  (orthogonal stereo axis, separate user mental model, doubled scope
  if bundled).

**Followups noted in PLAN backlog:**
- Multiband detection (new ADR-0009 TBD) — promoted in importance as
  the remaining lever for the "open" gap vs Ozone IRC IV
  (~7 dB null-residual gap from Slice 9.6c reference).
- Slice 11b2 (Auto/Track + Learn + Bypass-with-match) promoted to
  next active slice — was deferred since Slice 11b1.
- Slice 7b (M/S detection) — deferred placeholder, no active demand.
- Per-channel envelope SIMD optimisation — open if heavy unlinked-
  CPU becomes a real-world issue.
```

If the existing PROGRESS uses different heading levels or date
formats, adapt to match. Content above is authoritative for shipped
state.

### 3.2 PLAN.md update

Open `PROMPTS/PLAN.md`. Find the Slice 7 row in the table. Update its
status column to shipped (matching whatever symbol the table uses for
done — same convention as the Slice 9 close).

Replace the prior provisional gate-headline wording for Slice 7 with:

```
Shipped 2026-05-29. Per-channel parallel envelopes with continuous
Link % blend (frozen ID `stereo_link_pct`, default 100% = bit-exact
fast-path = Slice 9 behaviour). Constant latency. Bench Slice 3/4/5
PASS 13/13, 14/14, 25/25 unchanged. ADR-0008. M/S detection deferred.
```

Find the "next slice" / sequence marker — promote **Slice 11b2** to
the immediate next position. The PLAN row for 11b2 already exists in
the Slice 11 family.

Add or update backlog rows below the active list (preserving prior
Slice 9 close backlog entries; only adding new):

```
| Backlog | Multiband detection (detection-bus only) | new ADR-0009 |
new product params | Closes the remaining ~7 dB null-residual /
"open" gap vs Ozone IRC IV documented in Slice 9.6c reference run.
Avoids ADR-0005's LR4 audio-path phase concerns by keeping the split
detection-only. Promoted in importance after Slice 7 since stereo
unlink (the "wide" lever) is now shipped; multiband is the remaining
"open" lever. |
| Backlog | Slice 7b: M/S detection | ADR-0008 §Alternatives | new
mode/param wiring | `ms_link_pct` placeholder ID already exists in
`ParameterIDs.h`. Orthogonal stereo axis to per-channel unlink. No
active demand — promote only if avishali surfaces a use case M/S
solves better than the Slice 7 continuous Link %. |
| Backlog | Per-channel envelope SIMD optimisation | `mdsp_dsp::
LimiterEnvelope` | none | Slice 7 unlinked path runs ~2× envelope
CPU. If real-world mix sessions surface this as a workflow blocker,
SIMD the ramp inner loop or short-circuit second envelope on
identical peak streams. Open only if measured as a problem. |
```

Match the existing table column structure.

### 3.3 Product close docs commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git add docs/PROGRESS.md PROMPTS/PLAN.md
git status --short
# expect ONLY:
#    M  docs/PROGRESS.md
#    M  PROMPTS/PLAN.md
#    ?? PROMPTS/SLICE_07_*.md   (handled in Phase 3)

git commit -m "$(cat <<'EOF'
Slice 7 close: PROGRESS entry + PLAN update

Documents the shipped state: stereo_link_pct param + per-channel
parallel envelopes + gain blend, bit-exact at default 100 via
fast-path. ADR-0008 covers the detection topology decision.

PLAN: Slice 7 marked shipped, Slice 11b2 (Auto/Track + Learn)
promoted to next. Backlog rows added/updated for:
- Multiband detection (new ADR-0009) — promoted as the remaining
  "open" gap lever after stereo unlink closed the "wide" gap.
- Slice 7b M/S detection — placeholder param `ms_link_pct` retained
  in ParameterIDs.h for future use; no active demand.
- Per-channel envelope SIMD optimisation — open if unlinked CPU
  surfaces as a workflow issue.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

────────────────────────────────────────
4. PHASE 3 — Slice 7 prompts archive
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect ONLY:
#    ?? PROMPTS/SLICE_07_stereo_unlink.md
#    ?? PROMPTS/SLICE_07_CLOSE.md

git add PROMPTS/SLICE_07_*.md

git commit -m "$(cat <<'EOF'
Slice 7: archive design prompts

Slice 7 implementation prompt and close prompt archived as paper
trail. Stereo unlink design (ADR-0008), per-channel parallel
envelope wiring, fast-path bit-exactness rationale, and close
sequence.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

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
#   f05c062 MasterLimiter Slice 7: Stereo Link % (per-channel parallel envelopes)
#   7ec771c Slice 9: archive design prompts
#   3365e68 Slice 9 close: PROGRESS entry + PLAN update
#   4e394e5 MasterLimiter: consolidate oversampling ...
#   ... etc

# If on a feature branch and main FF needed:
#   git checkout main
#   git merge --ff-only <branch>
#   git push origin main
# If already on main:
#   git push origin main

# Cursor picks the right path based on current branch.
```

If a non-FF merge would be required, STOP and report.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. HQ branch name, post-push `git log --oneline -5`, confirmation
   push succeeded.
2. Product PROGRESS.md diff summary (Slice 7 block placed at top).
3. Product PLAN.md diff summary (Slice 7 marked shipped, Slice 11b2
   promoted, backlog rows added).
4. Product commit SHAs + titles for the two new commits (docs +
   prompts archive).
5. Product branch name, post-push `git log --oneline -8`, confirmation
   push succeeded.
6. Remaining dirty state in either repo (expect: HQ MCP unrelated
   dirt only; product clean).
7. Any auth / non-FF issues → reported, not force-resolved.
8. Open questions.

────────────────────────────────────────
7. AFTER THIS
────────────────────────────────────────

Slice 7 closed and shipped to both remotes. Next session:

- I open the **Slice 11b2 (Auto/Track + Learn) interview** to scope
  the implementation. PLAN entry exists. Pin: one-shot Learn vs
  continuous Track, snapshot window length, UI placement, frozen-ID
  param names.
- Optional parallel: ADR-0009 (multiband detection) sketch as a
  backlog exploration — design only, no implementation. Promote
  to active slice after 11b2 closes.
- avishali audits real-world mix sessions with the Slice 7 build —
  stereo unlinking is the kind of feature that only reveals its
  full character over long mixing time, not short audition.
