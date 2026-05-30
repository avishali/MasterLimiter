# Cursor Task — Slice 13 close (LUFS calibration)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ commit + product submodule bump + docs.** Slice 13 fixed
> two K-weighting coefficient bugs in `mdsp_dsp::LoudnessAnalyzer`.
> avishali confirmed real-world agreement with WLM / Insight 2
> within ~0.2 LU (the prior ~1 LU offset is gone). The synthetic
> 1 kHz-sine validation showed a −0.22 LU residual — accepted as
> bilinear-transform warping that averages out on broadband
> material; no exact-coefficient follow-up planned. This close:
> ONE HQ commit (the analyzer fix) + the product submodule bump +
> PROGRESS.md and PLAN.md updates. No product source.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Push to remote REQUIRED (architect-blessed).
- HEREDOC for multi-line commit messages.
- No `--force`, no `--no-verify`, no `--amend`.
- `MCP` nested-submodule pointer in HQ stays dirty/untouched.
- Slice 13 gate: build clean (✓), product bench Slice 3/4/5
  PASS (✓), calibration improved 1.0 → ~0.2 LU on real
  material (✓ avishali), architect sign-off (✓).

────────────────────────────────────────
1. PRE-COMMIT INVENTORY
────────────────────────────────────────

HQ working tree on branch `slice-13-lufs-calibration`:
- ` M shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp` — the
  Slice 13 K-weighting fix.
- ` M MCP` — unrelated nested submodule pointer; leave untouched.

Product working tree on `main`:
- `?? PROMPTS/SLICE_13_lufs_calibration.md` — architect artefact,
  do NOT commit.
- `?? PROMPTS/SLICE_13_CLOSE.md` — this file, architect artefact,
  do NOT commit.
- Possibly other `?? PROMPTS/*.md` from prior closes (stashed or
  untracked) — do NOT commit.

────────────────────────────────────────
2. STEP 1 — HQ commit
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git branch --show-current     # slice-13-lufs-calibration
git status --short            # ' M LoudnessAnalyzer.cpp' + ' M MCP'
```

Stage ONLY the analyzer file:

```bash
git add shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp
git status --short            # MUST show only the analyzer staged; MCP untouched
```

Commit:

```bash
git commit -m "$(cat <<'EOF'
LoudnessAnalyzer: fix K-weighting coefficients to BS.1770-4 (calibration)

Two coefficient bugs in updateFilters() caused a ~1 LU positive
offset vs reference meters (WLM, Insight 2):

- Stage 1 high-shelf frequency 1500 Hz → 1681.974 Hz (BS.1770-4
  §A.1). The too-low shelf over-boosted HF energy.
- Stage 2 RLB high-pass Q: was JUCE's default 1/√2 ≈ 0.707 (via
  the 2-arg makeHighPass overload) → now explicit 0.5003 with
  the 3-arg overload, and frequency 38.0 → 38.135 Hz (§A.2).

Validation: real-world program material now agrees with WLM /
Insight 2 within ~0.2 LU (prior offset ~1 LU eliminated). A
synthetic 1 kHz sine normalized to −23.0 LUFS measures −23.22
(−0.22 LU residual); this is bilinear-transform warping in the
shelf transition region (corner 1681 Hz, worst-case for a 1 kHz
tone) and averages out on broadband material. Exact per-rate
BS.1770-4 digital coefficients were considered and declined —
the JUCE bilinear approximation is within tolerance for a
mastering tool.

Analyzer feeds UI meters only; no audio-path effect. Product
bench Slice 3/4/5 PASS unchanged.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Push + FF master:

```bash
git push -u origin slice-13-lufs-calibration
git checkout master
git pull --ff-only origin master
git merge --ff-only slice-13-lufs-calibration
git push origin master
git log --oneline -n 3
git status --short            # only ' M MCP' remains
```

────────────────────────────────────────
3. STEP 2 — Product submodule bump
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout main
git status --short
cd third_party/melechdsp-hq
git fetch origin
git checkout master
git pull --ff-only origin master    # picks up the Slice 13 fix
cd ../..
git status --short                  # third_party/melechdsp-hq modified
```

Commit the bump + the docs updates from §4 together on one
branch (do §4 edits FIRST, then this commit):

```bash
git checkout -b chore-slice-13-close
# (make the PROGRESS.md + PLAN.md edits per §4 now, before staging)
git add third_party/melechdsp-hq docs/PROGRESS.md PROMPTS/PLAN.md
git status --short                  # 3 paths staged; no PROMPTS/SLICE_*.md
```

Commit:

```bash
git commit -m "$(cat <<'EOF'
MasterLimiter Slice 13 close: LUFS calibration (HQ submodule bump + docs)

Bumps the HQ submodule to pick up the LoudnessAnalyzer K-weighting
coefficient fix (BS.1770-4 §A). The ~1 LU offset vs WLM / Insight 2
is eliminated; real-world material now agrees within ~0.2 LU. No
product source changes this slice. PROGRESS.md + PLAN.md updated.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"

git push -u origin chore-slice-13-close
git checkout main
git merge --ff-only chore-slice-13-close
git push origin main
git log --oneline -n 4
git status
```

Product `main` clean; top of log is the Slice 13 close commit.

────────────────────────────────────────
4. STEP 3 — Docs (edit BEFORE staging in §3)
────────────────────────────────────────

### 4.1 PROGRESS.md — new entry at TOP

Insert immediately after the "Append-only" line, before the
Slice 12 entry:

```markdown
## 2026-05-30 — Slice 13: LUFS calibration (BS.1770-4 K-weighting fix)

**Status:** ✅ Closed. HQ-side bugfix in `mdsp_dsp::LoudnessAnalyzer`.
No new params, no API change, no product source change, no ADR.

**Root cause & fix (HQ `shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp`)**
Two K-weighting coefficient deviations from BS.1770-4 §A caused a
consistent ~1 LU positive offset vs reference meters (WLM,
iZotope Insight 2):
- **Stage 1 high-shelf frequency**: `1500 Hz` → `1681.974 Hz`
  (§A.1). The too-low corner over-boosted HF energy, inflating
  the reading.
- **Stage 2 RLB high-pass**: Q was JUCE's default `1/√2 ≈ 0.707`
  (2-arg `makeHighPass`) → explicit `0.5003` (3-arg overload);
  frequency `38.0 Hz` → `38.135 Hz` (§A.2).

**Validation**
- Real-world program material now agrees with WLM / Insight 2
  within ~0.2 LU (prior ~1 LU systematic offset eliminated).
  avishali confirmed in Ableton.
- Synthetic 1 kHz stereo sine normalized (pyloudnorm) to
  −23.000 LUFS measures −23.22 in our analyzer (−0.22 LU). This
  residual is bilinear-transform warping in the shelf transition
  region — the shelf corner (1681 Hz) makes a 1 kHz tone the
  worst-case probe; broadband material averages it out, which is
  why real-world agreement is tighter than the synthetic test.
- Exact per-rate BS.1770-4 digital coefficients were considered
  and **declined** — the JUCE bilinear approximation is within
  tolerance for a mastering tool. If a future need for sub-0.1 LU
  precision arises, a Slice 13.1 would hardcode the canonical
  §A coefficients with per-rate (44.1/48/88.2/96 kHz) derivation.

**Gate result**
- [x] Debug + Release builds clean (one pre-existing JUCE/VST3
      SDK deprecation warning; none from `Source/` or
      `shared/mdsp_dsp/`).
- [x] Product bench Slice 3/4/5 PASS 13/13, 14/14, 25/25
      unchanged (analyzer feeds UI meters only; Track off at
      bench defaults → no audio-path effect).
- [x] avishali audition: real-world LUFS agrees with WLM /
      Insight 2 within ~0.2 LU.
- [x] Architect sign-off on diff scope + RT-safety (two
      coefficient values; no structural change).

**Followups**
- ADR-0009 (multiband detection) promoted to active — the
  remaining architectural lever for the "open" gap vs Ozone
  IRC IV (~7 dB null residual) and the CPU↔GR correlation,
  in the backlog since Slice 7 close.
- Slice 13.1 (exact BS.1770-4 per-rate coefficients) remains
  un-queued; open only if sub-0.1 LU precision is ever needed.
```

### 4.2 PLAN.md — mark Slice 13 closed, promote ADR-0009

Find the Slice 13 row (currently `**NEXT — LUFS calibration ...**`)
and change its status to shipped:

```
| 13 | ✅ **Shipped — LUFS calibration (BS.1770-4 K-weighting fix)** | HQ `LoudnessAnalyzer` | (submodule bump only) | Shipped 2026-05-30. Fixed two K-weighting coefficient bugs in `mdsp_dsp::LoudnessAnalyzer`: Stage 1 high-shelf 1500 → 1681.974 Hz; Stage 2 RLB high-pass Q 0.707 → 0.5003 (and 38.0 → 38.135 Hz), both per BS.1770-4 §A. Prior ~1 LU offset vs WLM / Insight 2 eliminated; real-world agreement within ~0.2 LU. Synthetic 1 kHz-sine residual −0.22 LU accepted (bilinear warping, worst-case probe). No ADR. No product source change. |
```

Append a Slice 14 row for the next architectural slice:

```
| 14 | **NEXT — Multiband detection (detection-bus only, ADR-0009)** | new ADR-0009; band-split detection in HQ | new product params + UI | Promoted from backlog. Closes the remaining ~7 dB null-residual / "open" gap vs Ozone IRC IV (Slice 9.6c reference) AND the CPU↔GR correlation surfaced in 7.1.5 (single-band 4× OS limiting concentrates all peak work in one envelope; multiband distributes across band-specific envelopes). Detection-bus only — LR4 split feeds per-band detection, audio path stays full-band, avoiding the LR4 audio-path phase concerns per ADR-0005's reasoning. ADR-0009 to be written first. |
```

Update the "Note:" block below the table. Find:

```
Note: Slice 12 (Clipper gain-staging + Hard/Soft + threshold +
meter, ADR-0010) closed 2026-05-30. Slice 13 (LUFS calibration)
is the active slice ...
```

Replace with:

```
Note: Slice 13 (LUFS calibration) closed 2026-05-30. Slice 14
(Multiband detection, ADR-0009) is the active architectural
slice — ADR-0009 to be written first. It targets the remaining
"open" gap vs Ozone IRC IV and the CPU↔GR correlation. Slice 6
(saturator) and Slice 7b (M/S detection) remain backlogged.
```

(Also update the Backlog table's "Multiband detection" row
status from `Backlog` to `Promoted → Slice 14` if a status
column exists; otherwise leave the backlog note and rely on the
slice-table row.)

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. HQ commit: SHA, file staged (1), push + FF confirmation.
3. Product close commit: SHA, files staged (3: submodule +
   PROGRESS.md + PLAN.md), push + FF confirmation.
4. Final HQ `git log --oneline -n 3` (Slice 13 fix at head).
5. Final product `git log --oneline -n 4` (Slice 13 close at
   head, on top of the Slice 12 chore bump).
6. Final product `git status` — clean.
7. Final HQ `git status --short` — only ` M MCP`.
8. Open questions.

────────────────────────────────────────
6. ARCHITECT FOLLOW-UP
────────────────────────────────────────

On successful close, architect:
- Writes **ADR-0009 — Multiband detection (detection-bus only)**
  in HQ, then the Slice 14 interview + prompt.
- Refreshes the `project-masterlimiter-slices` memory if any
  process detail changed (none expected).
