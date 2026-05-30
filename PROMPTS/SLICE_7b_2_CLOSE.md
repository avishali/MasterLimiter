# Cursor Task — Slice 7b.2 CLOSE (M/S ceiling fix)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product + HQ close of the shipped-bug hotfix.** avishali confirmed M/S mode
> no longer overshoots the ceiling. Commit + push the bench M/S-overs test (HQ),
> bump the submodule, commit the product fix + docs + prompt archive, FF both
> remotes.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Stage by **explicit path only**. No force-push/amend/skip-hooks. Auth via
  system credential helper; STOP if interactive. `Co-Authored-By: Claude`
  trailer on commits.
- HQ commit on the **sibling** working copy. Leave `MCP` dirty/untouched.
- Current product branch `slice-7b-2-ms-ceiling`; HQ sibling branch
  `slice-7b-2-ms-ceiling`.

────────────────────────────────────────
1. PRE-FLIGHT
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # expect slice-7b-2-ms-ceiling
cmake --build build-release -j
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/SLICE7b2_CLOSE_S$SLICE"; done
# plus the M/S overs test (the --ms-ceiling-overs path) → expect PASS (0/0)
```
Gate: build clean; Slice 3/4/5 PASS; M/S overs test PASS. If anything fails, STOP.

────────────────────────────────────────
2. PHASE A — HQ commit + push (sibling, bench M/S overs test)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short               # expect: M MCP (leave), M bench.py, M drivers/master_limiter.py
git add shared/dsp_bench/bench.py shared/dsp_bench/drivers/master_limiter.py
git commit -m "dsp_bench: add M/S-mode ceiling-overs test (Link 0 / ceiling 0) for master_limiter"
git push origin slice-7b-2-ms-ceiling
git checkout master && git pull --ff-only && git merge --ff-only slice-7b-2-ms-ceiling && git push origin master
```
Confirm status shows only ` M MCP`. Record new HQ master SHA.

────────────────────────────────────────
3. PHASE B — bump product submodule
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/third_party/melechdsp-hq
git fetch origin
git checkout <new HQ master SHA>
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
```

────────────────────────────────────────
4. PHASE C — product commits (explicit paths)
────────────────────────────────────────

**Commit A — the fix:**
```bash
git add Source/PluginProcessor.cpp
git commit -m "MasterLimiter Slice 7b.2: M/S ceiling fix — bound decoded L/R to threshold (independent M/S limiting was overshooting the ceiling)"
```

**Commit B — submodule bump + docs (write §5 first):**
```bash
git add third_party/melechdsp-hq docs/PROGRESS.md PROMPTS/PLAN.md
git commit -m "MasterLimiter Slice 7b.2 close: bump HQ submodule (M/S overs test) + PROGRESS/PLAN"
```

**Commit C — archive prompt:**
```bash
git add PROMPTS/SLICE_7b_2_ms_ceiling_fix.md PROMPTS/SLICE_7b_2_CLOSE.md
git commit -m "docs: archive Slice 7b.2 prompts"
```
Leave `PROMPTS/SLICE_AUTO_RELEASE.md` untracked (auto-release ships at its close).

────────────────────────────────────────
5. DOCS CONTENT (write before Commit B)
────────────────────────────────────────

### `docs/PROGRESS.md` — new TOP entry
Slice 7b.2 — M/S ceiling fix (hotfix to the shipped 7b M/S mode):
- Bug: in M/S mode with Link < 100%, Mid and Side were limited independently
  then decoded (`L=M'+S'`, `R=M'−S'`), so the decoded L/R could overshoot the
  ceiling (up to +6 dB worst case; ~+1 dB at ceiling 0.0). Default Stereo mode
  unaffected, which is why the bench missed it.
- Fix: per-sample decoded-L/R safety bound in the M/S apply branch (scale both
  channels by `thresholdLin/peak` when over; gain, not clip), folded into GR
  metering. Stereo path untouched → Slice 3/4/5 bit-identical.
- Bench: new M/S-mode ceiling-overs test (Link 0 / ceiling 0); FAIL (SP 2 /
  TP 8) → PASS (0/0).
- Note: the fully-transparent final-ceiling stage (one stage covering M/S
  decode overshoot + clipper inter-sample peaks + post-gain) remains a backlog
  polish item.

### `PROMPTS/PLAN.md`
- Note **Slice 7b.2 ✅ Shipped** (M/S ceiling hotfix).
- Backlog: **final inter-sample-peak-safe ceiling stage** (transparent; covers
  M/S overshoot + clipper ISPs + post-gain overs) — future polish.
- **Auto-release** remains in-progress on `slice-auto-release` (WIP-committed):
  must be **rebased onto this new `main`** to inherit the M/S fix; open items =
  first-touch pop + 3-mode tuning before it can close.

────────────────────────────────────────
6. PHASE D — push product
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git push origin slice-7b-2-ms-ceiling
git checkout main && git pull --ff-only && git merge --ff-only slice-7b-2-ms-ceiling && git push origin main
```
Record new product `main` SHA; confirm submodule pointer == new HQ master.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Pre-flight (build + Slice 3/4/5 + M/S overs PASS).
2. HQ commit SHA, FF master, new master SHA, final status (only ` M MCP`).
3. Product commit SHAs, submodule pointer, FF main, new main SHA.
4. Final `git status --short` both repos.
5. PROGRESS/PLAN excerpts.
6. Open questions.
