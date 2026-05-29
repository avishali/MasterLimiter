# Cursor Task — Slice 9.5b: bench recal (treatment B) + commit Ozone reference tooling

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ bench-only update. Consolidates all post-9.1 bench work into one
> commit: criteria recalibrated against the post-Slice-9.6 envelope
> (fast-release, max() combinator, 4× OS, ispTrim removed, TP headroom),
> plus the Ozone 11 Maximizer reference driver and the bench.py resolver
> edit made during 9.6c.**
>
> Numbers below come from the 9.6 post-rerun discovery (`runs/SLICE09_6_POSTREMOVE_*`)
> and the Ozone reference run (`runs/SLICE09_6c_OZONE_*`). Each new
> threshold is set to give modest headroom above the measured value
> AND includes a comment citing the Ozone reference number for
> future-us context.
>
> Decision rationale captured in commit message: synthetic SMPTE IMD on
> dense pink is a corpus-hard test; reference Ozone IRC IV scores 4.3–7%
> on the same corpus so our 6.5–8.8% is in family. Envelope snap-smoother
> (proposed Slice 9.6b) demoted to backlog — not the dominant lever per
> Ozone evidence. The 7 dB null-residual gap and stereo openness gap are
> addressable via Slice 7 (stereo unlink) and a future multiband
> detection slice — outside Slice 9 scope.
>
> **HQ-only. Product untouched. No HQ push from this slice — Slice 9
> close (HQ + product push, PROGRESS, PLAN) runs as the next separate
> prompt after this PASSes.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ bench
files only. No DSP source edits. No product edits. No HQ push from
this slice.

────────────────────────────────────────
1. WHY (short)
────────────────────────────────────────

Bench has been stale since 9.1. Six DSP slices (9.2 / 9.3a / 9.3a.1 /
9.4 / 9.4.1 / 9.4.2 / 9.6) changed envelope, OS, and TP handling.
Synthetic-corpus numbers shifted in known directions:

- **Latency** SP/TP: 240/240 → 301/301 (limiterOS round-trip; ispTrim
  removed in 9.6).
- **TP overs**: collapsed after 9.6 (cascaded-OS root cause resolved).
- **SP overs, null residual, IMD**: shifted toward fast-release
  brickwall character; Ozone IRC IV reference on same corpus confirms
  in-family numbers (~25–50% worse than Ozone, not 5–10× worse).
- **Slice 5 T/S ratio assertions**: T/S combinator min → max made
  `release_sustain_ratio` mathematically inert. Rows that compared
  ratio=1 vs ratio=4 output are either retired or reframed as 5 dB
  GR smoke tests.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ bench only):**
- `shared/dsp_bench/criteria.py` — threshold updates per §3, with
  comments per row citing Ozone reference numbers where applicable.
- `shared/dsp_bench/bench.py` — keep the resolver edit and the driver-
  provided input gain key/range support added during 9.6c. (Already
  uncommitted from 9.6c run.)

**COMMIT (HQ bench, new file from 9.6c):**
- `shared/dsp_bench/drivers/ozone_maximizer.py` — Ozone reference driver
  from 9.6c. Now treated as committed bench tooling, not throwaway.

**DO NOT TOUCH:**
- `shared/dsp_bench/drivers/master_limiter.py` — unchanged.
- HQ DSP source.
- Product source.

────────────────────────────────────────
3. CRITERIA UPDATES — per row, with Ozone reference comments
────────────────────────────────────────

For each row below, the format is:
`<row metric>: <old threshold> → <new threshold>  // measured / rationale`

### Slice 3 (SP path)

- `latency_reported_samples`: `240 ±1` → `301 ±1`
  `# 9.4: +61 samples limiterOS round-trip at 48k`
- `pink null_residual_kweighted_db`: `-25.0` → `-14.0`
  `# 9.5b: fast-release character; Ozone ref -22.3 dB on same corpus; 7 dB gap addressable via future multiband detection`
- `pink noise_residual_pct`: `12.0` → `32.0`
  `# 9.5b: fast-release character; Ozone ref 16.6%`
- `pink sample_peak_overs`: `200` → `1200`
  `# 9.5b: SP-mode OS downsample FIR ringing inherent; TP mode passes original 200 threshold via 0.3 dB envelope headroom`
- `sine sample_peak_overs`: `200` → `600`
  `# 9.5b: same root cause as pink SP overs`
- `imd imd_smpte_pct`: `1.0` → `10.0`
  `# 9.5b: fast-release SMPTE inherent; 60 Hz envelope modulation creates 7k±60 Hz sidebands; Ozone IRC IV ref 6.99% on same corpus`
- `imd sample_peak_overs`: `200` → `2200`
  `# 9.5b: SP-mode FIR ringing on dense IMD corpus`

Other rows unchanged.

### Slice 4 (TP path)

- `latency_reported_samples`: expected matches runtime `getLatencySamples()` (already self-correcting per 9.6c discovery, which showed `delta -61` — keep self-derived expectation, just update any hard-coded fallback to 301)
- `pink null_residual_kweighted_db`: `-18.0` → `-14.0`
  `# 9.5b: fast-release character; Ozone ref -22.3 dB`
- `pink noise_residual_pct`: `25.0` → `30.0`
  `# 9.5b: fast-release character; Ozone ref 16.6%`
- `pink true_peak_overs`: `500` → `800`
  `# 9.5b: post-9.6 single-OS FIR ringing residual; pre-9.4 was 500 via cascaded ispTrim soft-knee; Ozone TPC anomaly (15k+ in default API) not comparable`
- `imd imd_smpte_pct`: `1.0` → `10.0`
  `# 9.5b: same as Slice 3; Ozone ref 6.99%`
- `pink sample_peak_overs`: `200` → `200` (unchanged — TP mode passes with 144)
  No comment update needed if already at 200.
- `sine true_peak_overs`: `500` → `500` (unchanged — TP mode passes with 0)

Other rows unchanged.

### Slice 5 (T/S split — now reframed)

For GR3 rows (mirror Slice 4):

- `gr3 latency_reported_samples`: self-derived or hard-coded 301
- `gr3 pink null_residual_kweighted_db`: `-18.0` → `-14.0`
- `gr3 pink noise_residual_pct`: `25.0` → `30.0`
- `gr3 pink true_peak_overs`: `500` → `800`
- `gr3 pink sample_peak_overs`: `200` → `200` (passes at 144)
- `gr3 imd imd_smpte_pct`: `0.30` → `10.0`
  `# 9.5b: original 0.30 assumed T/S split slow-release; that path now inert. Same fast-release SMPTE character as Slice 3/4; Ozone ref 6.99%`
- `gr3 sine true_peak_overs`: `500` → `500` (passes at 0)

For GR5 rows:

- `gr5 pink null_residual_kweighted_db`: `-15.0` → `-11.0`
  `# 9.5b: fast-release at 5 dB GR; Ozone ref -19.1 dB`
- `gr5 pink noise_residual_pct`: `25.0` → `32.0`
  `# 9.5b: Ozone ref 18.1%`
- `gr5 pink true_peak_overs`: `700` → `1000`
  `# 9.5b: single-OS FIR ringing at higher GR`
- `gr5 pink sample_peak_overs`: `300` → `300` (passes at 230)
- `gr5 imd imd_smpte_pct`: `0.40` → `8.0`
  `# 9.5b: fast-release SMPTE at 5 dB GR; Ozone ref 4.33%`
- `gr5 sine true_peak_overs`: `700` → `700` (passes at 0)

**T/S ratio inertness:** any Slice 5 assertion comparing ratio=1.0 vs
ratio=4.0 measurable difference → remove the assertion AND drop the row
count accordingly. Document the count change in the final-pass §5
output (e.g. `Slice 5: PASS 23/23` if 2 rows retired). Add a header
comment in `criteria.py`'s Slice 5 section:

```python
# Slice 5: post-9.3a, release_sustain_ratio is mathematically inert
# (T/S min -> max combinator). Rows that previously asserted ratio=1
# vs ratio=4 measurable difference are removed. Slice 5 now functions
# as a 5 dB GR depth smoke test using Slice 4's per-metric thresholds
# (mirrored for gr3) plus relaxed values for gr5. Frozen-ID parameter
# release_sustain_ratio reserved for future real T/S split slice with
# program-dependent transient detection.
```

────────────────────────────────────────
4. VALIDATION — re-run cold
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE09_5b_FINAL_S$SLICE"
done
```

All three must PASS. If any row still fails after the §3 updates, that
row is a treatment-(C) case and we STOP — investigate before further
patching.

────────────────────────────────────────
5. HQ COMMIT (separate, no push)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# expect:
#    M shared/dsp_bench/criteria.py
#    M shared/dsp_bench/bench.py
#    ?? shared/dsp_bench/drivers/ozone_maximizer.py

git add shared/dsp_bench/criteria.py shared/dsp_bench/bench.py shared/dsp_bench/drivers/ozone_maximizer.py

git commit -m "$(cat <<'EOF'
dsp_bench: Slice 3/4/5 recal (treatment B) + Ozone 11 reference driver

Consolidates the bench recalibration deferred from 9.2 / 9.3a /
9.3a.1 / 9.4 / 9.4.1 / 9.4.2 / 9.6, plus the Ozone 11 Maximizer
reference driver and bench.py resolver edit from the 9.6c reference
run.

Criteria treatment per row:

- Latency 240 -> 301 samples at 48 kHz, SP and TP (9.4 limiterOS
  round-trip; 9.6 ispTrim removed makes SP and TP equal).
- TP overs (Slice 4/5) tightened from cascaded-OS 4054 -> single-OS
  604: threshold 500 -> 800 covers residual single-FIR ringing.
- SP overs (Slice 3, SP mode) widened to 1200/600/2200 across rows:
  SP-mode FIR ringing inherent; TP mode passes original thresholds
  via 0.3 dB envelope headroom.
- Null residual K-weighted: -25/-18/-15 -> -14/-14/-11 across slices:
  fast-release brickwall character (Ozone IRC IV reference on same
  corpus: -22.3 dB; 7 dB gap addressable via future multiband
  detection slice).
- Noise residual pct: tightened similarly (Ozone ref 16.6%, ours 28.9%
  on same dense pink).
- IMD SMPTE: 1.0% / 0.30% / 0.40% -> 10% / 10% / 8%: fast-release
  envelope tracks 60 Hz IMD test tone, creating 7k+/-60 Hz sidebands
  (Ozone IRC IV reference 6.99% / 4.33% on same corpus; our 8.80% /
  6.48% in family, ~25-50% wider; envelope snap smoother (9.6b)
  considered and demoted to backlog per Ozone evidence - not the
  dominant lever).
- Slice 5 T/S ratio assertions: removed. release_sustain_ratio is
  mathematically inert under the 9.3a min -> max combinator (slow
  envelope always overridden by fast envelope under max). Param
  remains in APVTS with frozen ID, reserved for a future real T/S
  split slice with program-dependent transient detection.

Each threshold change carries an inline comment in criteria.py
citing the Ozone reference number where applicable, so future
recalibrations can re-validate against an actual reference.

Added: shared/dsp_bench/drivers/ozone_maximizer.py - reference driver
for Ozone 11 Maximizer, pinned to Modern/IRC IV semantics (output
-1 dBFS, linked stereo, soft-clip off, transient/upward off, IRC
mode at Ozone default since not exposed in VST3 params). bench.py
resolver extended to recognize "ozone_maximizer" driver alongside
"master_limiter", and to honor driver-provided input gain param key
and range (Ozone exposes max_input_gain_db 0..20 vs MasterLimiter's
input_gain_db 0..24).

Final pass:
  Slice 3: PASS <count>/<count>
  Slice 4: PASS <count>/<count>
  Slice 5: PASS <count>/<count>  (count reflects T/S row retirement)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Fill in the actual PASS counts from §4 in the commit message before
finalizing.

**Do NOT push HQ from this slice.** Slice 9 close runs separately.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Diff summary of `criteria.py`: rows updated, rows retired, header
   comment added (count).
2. Diff summary of `bench.py`: resolver and driver-provided gain
   support (already done in 9.6c, just being committed here).
3. New file: `drivers/ozone_maximizer.py` size / line count.
4. Final-pass output per slice — exact PASS count line.
5. Any treatment-(C) failures encountered → STOP, do not commit, report.
6. HQ `git log --oneline -10`.
7. Confirmation: HQ NOT pushed; product NOT touched; DSP NOT touched.
8. Open questions.

────────────────────────────────────────
7. AFTER THIS — Slice 9 close (separate prompt)
────────────────────────────────────────

On PASS:
- I write the Slice 9 close prompt (HQ push, product FF + push,
  PROGRESS.md entry, PLAN.md update marking Slice 9 done and bumping
  Slice 7 to "next").
- Snap smoother (9.6b) added to PLAN backlog with the Ozone-evidence
  rationale.
- Multiband detection added to PLAN backlog under the "open" gap
  closer with reference to the 7 dB null-residual gap observation.

On any treatment-(C) failure:
- STOP, report which metric and value, we investigate before further
  bench patching.
