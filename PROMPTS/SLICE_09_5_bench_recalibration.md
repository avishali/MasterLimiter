# Cursor Task — Slice 9.5: consolidated bench recalibration (Slice 3/4/5 against new envelope + 4× OS)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ bench-only update. Consolidates the bench recalibration deferred
> from 9.2 + 9.3a + 9.3a.1 + 9.4 + 9.4.1 + 9.4.2.** Every one of those
> slices was audition-only; the bench has been stale since 9.1. Now we
> bring it back to PASS so we can close Slice 9 and push.
>
> **HQ-only. Product untouched. No product push from this slice. HQ may
> be pushed at the end of the Slice 9 close, which happens as a SEPARATE
> step after this slice passes.**
>
> No code changes to DSP. Bench criteria/expectations only.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ bench
files only. No DSP source edits in this slice. No product edits. No
HQ push from this slice (Slice 9 close runs separately).

────────────────────────────────────────
1. WHY
────────────────────────────────────────

Cumulative changes since the bench last passed (Slice 9.1):

- **9.2**: ext_ trailing-edge propagation fixed (`k < attackSamples`).
- **9.3a**: T/S combine flipped `min → max` in LimiterEnvelope.
- **9.3a**: `release_ms` default lowered `100 → 30 ms` in product.
- **9.3a.1**: ramp tentative formula uses 1.0 baseline instead of
  `anchor = ext_[i]` (kills the second-order propagation chain).
- **9.4**: 4× oversampling around the limiter chain in product
  processBlock. New reported latency at 48k: SP **301 samples**, TP
  **362 samples** (was 240 SP / ~301 TP).
- **9.4.1**: outer loop fast-path skips inner ramp loop when
  `pk <= thr`.
- **9.4.2**: Clean attack capped at 3 ms (was = lookaheadSamples_).

Expected bench impact:

| Metric                    | Expected direction vs prior pass     |
|---------------------------|--------------------------------------|
| Reported latency          | **Larger** (SP +61, TP +61 at 48k)  |
| Null residual (K-weighted)| **Tighter** (4× OS suppresses alias)|
| Broadband null            | **Tighter / similar**                |
| IMD                       | **Tighter** (4× OS = less IMD)      |
| Loudness gap vs reference | **Similar or tighter**               |
| GR target hit accuracy    | **Same or better** (faster release)  |
| T/S split behaviour       | **Inert** under `max()` — Slice 5    |
|                           |   ratio=1.0 vs ratio=4.0 should now  |
|                           |   produce identical output           |

The two genuine framing shifts:

1. **Latency**: every alignment offset in the bench needs to come from
   the plugin's reported `getLatencySamples()` at run time, not a
   hard-coded constant. If it's already pulled at run time, the bench
   self-adjusts. If hard-coded anywhere, update.

2. **Slice 5's T/S assertions**: Slice 5 was specifically calibrated to
   exercise the T/S split's slow-release behaviour under
   `release_sustain_ratio = 4.0`. Under `max()`, ratio is mathematically
   inert (the slow envelope is always overridden by the fast one). So
   any Slice 5 assertion that depended on a *measurable* difference
   between ratio=1.0 and ratio=4.0 will fail because both now produce
   the same output.

   Treatment: rewrite the affected Slice 5 assertions as smoke tests
   that confirm the limiter hits its 5 dB GR target on the same corpus
   with the same K-weighted null residual / IMD / loudness gap
   thresholds as Slice 4 (single-band TP) — the new envelope is now
   functionally single-band on release. Document the ratio inertness in
   the criteria file comments so the bench file itself records the new
   reality.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ bench only):**
- `shared/dsp_bench/criteria.py` — update Slice 3 / 4 / 5 thresholds
  and expected-latency entries per the methodology in §3. Slice 5's
  T/S-specific assertions reframed per the rationale above.
- `shared/dsp_bench/bench.py` — only if a hard-coded latency constant
  exists anywhere; otherwise untouched.
- Any helper / utility under `shared/dsp_bench/` that consumes a
  hard-coded latency or threshold — same rule, only if needed.

**DO NOT TOUCH:**
- `shared/mdsp_dsp/...` — HQ DSP source unchanged this slice.
- `shared/dsp_bench/drivers/master_limiter.py` — driver already pins
  `character = "Clean"` since 9.1. No change.
- Any product file.
- Any UI / processor / parameters file.

────────────────────────────────────────
3. WHAT TO DO — methodology
────────────────────────────────────────

### 3.1 Discovery phase — run bench cold first

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release   # ensure latest VST3

cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE09_5_BASELINE_S$SLICE"
done
```

Capture the full per-row output. For each FAIL, note:
- Which metric (latency / null residual K-weighted / broadband null /
  IMD / loudness gap / GR depth / TP overs).
- Measured value vs expected threshold.
- Direction: measured **better** than expected, **worse** in a known
  direction, or **worse** in an unexplained direction.

### 3.2 Per-metric treatment

For each failing metric apply ONE of:

- **(A) Measured is better than the old threshold** → tighten the
  threshold to e.g. `measured × 1.5` (allow 50% headroom for run-to-run
  jitter on synthetic corpora). Comment in `criteria.py` with the
  measured value and the slice that caused the tightening.

- **(B) Measured is worse in a known direction** (latency increased
  because we added OS; Slice 5 T/S ratio assertions failed because
  combinator flipped to `max()`) → update the expectation to the new
  reality. Comment **why** with a one-line reference to the slice that
  caused the change (e.g. `# 9.4: +61 samples OS latency at 48k`).

- **(C) Measured is worse and the direction is not explained by the
  9.2 / 9.3a / 9.3a.1 / 9.4 / 9.4.1 / 9.4.2 change list above** → STOP.
  Report the failing metric and measured value. Do NOT update the
  threshold. We investigate first; the bench is the safety net.

Treatment (A) tightenings should be conservative — we want the bench
to catch real regressions, not pass everything by being slack, but
also not fail on harmless run-to-run drift. 50% headroom on measured
is a reasonable starting point; Cursor may adjust per metric if the
synthetic corpus has known variance.

### 3.3 Slice 5 T/S inertness — explicit handling

Find every Slice 5 assertion (in `criteria.py` and any helper code) that
compares output between `release_sustain_ratio = 1.0` and `4.0`, or that
expects measurable T/S behaviour. For each:

- If the assertion was "ratio=4.0 differs from ratio=1.0 by X dB on
  metric Y": remove or rewrite as "ratio=any produces limiting that
  hits the 5 dB GR target with the same per-metric thresholds as Slice
  4." Comment the rationale: `# 9.3a: T/S combinator min -> max makes
  release_sustain_ratio mathematically inert; ratio knob is a frozen-ID
  param reserved for future real T/S split slice.`
- If the assertion was just "5 dB GR limiter works on corpus X with
  null residual ≤ Y" without depending on ratio: keep as-is, treat
  metric per §3.2.

### 3.4 Validation phase — re-run cold against updated criteria

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE09_5_FINAL_S$SLICE"
done
```

All three must end:
- Slice 3: PASS 13/13
- Slice 4: PASS 14/14
- Slice 5: PASS 25/25 (or whatever the new count is if any Slice 5
  rows were retired due to T/S inertness — document the count change
  in the commit message)

If any slice still fails after step 3.2 treatment, that's a treatment
(C) failure — STOP and report.

────────────────────────────────────────
4. BUILD
────────────────────────────────────────

Already built in §3.1 setup. No further build needed in this slice
(bench-only changes).

────────────────────────────────────────
5. HQ COMMIT (separate, no push)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# expect ONLY:
#    M shared/dsp_bench/criteria.py
#    (possibly M shared/dsp_bench/bench.py if a hard-coded latency
#     constant existed and needed updating)

git add shared/dsp_bench/criteria.py
# git add shared/dsp_bench/bench.py     # only if modified

git commit -m "$(cat <<'EOF'
dsp_bench: recalibrate Slice 3/4/5 criteria against new envelope + 4x OS

Consolidated bench recalibration covering the audition-only DSP slices
shipped since 9.1:

- 9.2: ext_ trailing-edge propagation fix (k < attackSamples)
- 9.3a: T/S combine min -> max + release_ms default 100 -> 30 ms
- 9.3a.1: ramp tentative formula uses 1.0 baseline (kills second-
  order propagation chain)
- 9.4: 4x oversample around limiter chain (Clipper + Envelope + Gain)
- 9.4.1: outer-loop fast-path (skip inner ramp when pk <= thr)
- 9.4.2: Clean attack capped at 3 ms

Updates:

- Latency expectations: SP 301 samples, TP 362 samples at 48 kHz
  (was SP 240, TP ~301). Slice 3/4/5 alignment offsets updated.
- Null residual / IMD thresholds: tightened where 4x OS measurably
  improved the synthetic-corpus result; same margin where unchanged.
- Slice 5 T/S assertions: reframed. T/S combinator min -> max makes
  release_sustain_ratio mathematically inert (slow envelope always
  overridden by fast envelope under max). Ratio knob remains as a
  frozen-ID parameter reserved for a future real T/S split slice
  with program-dependent transient detection. Slice 5 retained as a
  5 dB GR depth smoke against the same metric thresholds as Slice 4,
  documenting the ratio-inert reality in inline comments.

Driver unchanged (character pinned to "Clean" since 9.1).

Final pass:
  Slice 3: PASS 13/13
  Slice 4: PASS 14/14
  Slice 5: PASS 25/25  (adjust count in commit if rows retired)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Adjust the Slice 5 row count in the final-pass section of the message
if any rows were retired due to T/S inertness.

**Do NOT push HQ from this slice.** Slice 9 close runs as a separate
step after this passes.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Discovery-phase output: for each Slice 3 / 4 / 5 row that failed
   on the baseline run, list metric / measured / old threshold /
   treatment applied (A/B/C).
2. Any (C) treatment encountered → STOP report and we investigate
   before continuing. Otherwise:
3. Final-phase output: PASS line per slice (3 / 4 / 5).
4. Diff summary of `criteria.py` (and `bench.py` if touched) —
   number of thresholds tightened, expectations updated, rows
   retired.
5. HQ `git log --oneline -9`.
6. Confirmation: HQ NOT pushed; product repo NOT touched; DSP
   source NOT touched.
7. Open questions.

────────────────────────────────────────
7. AFTER THIS — Slice 9 close (separate session)
────────────────────────────────────────

Once Slice 9.5 PASSes, we close Slice 9 as a SEPARATE step:

1. **HQ push.** 8 commits (276c397 … this 9.5 commit) become public.
   `git push origin <branch>`.
2. **Product push.** 2 product commits (`ce9a478` + `64db6e7`) FF onto
   `main` and push.
3. **PROGRESS.md update** — single entry for Slice 9 covering
   character + clipper + on/off + release fixes + OS + bench recal.
4. **PLAN.md** — mark Slice 9 done; next slice TBD (likely Slice 7
   Stereo Unlink per the "wide" gap discussion).
5. **PROMPTS/** — uncommitted slice prompts from tonight's session
   either bundled into a housekeeping commit or kept local; decide
   per existing convention.

The close prompt will be its own short Cursor task, written after
9.5 PASSes.
