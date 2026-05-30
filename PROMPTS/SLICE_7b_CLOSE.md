# Cursor Task — Slice 7b CONSOLIDATED CLOSE (M/S mode)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product + HQ close.** avishali approved M/S mode (single Link knob +
> Stereo/M-S toggle). Commit + push the HQ ADR-0008 addendum (sibling), bump
> the product submodule, commit the M/S feature, commit the carried-over meter
> tuning as its own commit, docs + prompt archive, fast-forward both remotes.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Stage by **explicit path only** (never `git add -A`/`.`). No force-push, no
  amend, no skip-hooks. Auth via system credential helper; STOP if interactive.
  End commit messages with the `Co-Authored-By: Claude` trailer.
- HQ commit on the **sibling** `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq`.
  Leave its `MCP` pointer dirty/untouched.

────────────────────────────────────────
1. PRE-FLIGHT
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # expect slice-7b-ms-link
cmake --build build-release -j   # clean, no new Source/ warnings
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/SLICE7b_CLOSE_S$SLICE"; done
```
Gate: build clean; Slice 3/4/5 PASS (default = Stereo mode, M/S 100% →
bit-identical). If anything fails, STOP.

────────────────────────────────────────
2. PHASE A — HQ commit + push (sibling, ADR-0008 addendum)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short               # expect: M MCP (leave), M ADR-0008-...md
git checkout -b slice-7b-ms-adr
git add docs/DECISIONS/ADR-0008-masterlimiter-stereo-unlink.md
git commit -m "ADR-0008 addendum: M/S mode (Slice 7b) — stereo_mode domain selector"
git push origin slice-7b-ms-adr
git checkout master && git pull --ff-only && git merge --ff-only slice-7b-ms-adr && git push origin master
```
Confirm `git status --short` shows only ` M MCP`. Record the new HQ master SHA.

────────────────────────────────────────
3. PHASE B — bump product submodule
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/third_party/melechdsp-hq
git fetch origin
git checkout <new HQ master SHA from Phase A>
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short               # third_party/melechdsp-hq now modified (gitlink)
```

────────────────────────────────────────
4. PHASE C — product commits (explicit paths)
────────────────────────────────────────

**Commit A — M/S feature:**
```bash
git add Source/PluginProcessor.cpp Source/PluginProcessor.h \
        Source/parameters/ParameterIDs.h Source/parameters/Parameters.cpp \
        Source/ui/MainView.cpp Source/ui/MainView.h
git commit -m "MasterLimiter Slice 7b: M/S mode (ADR-0008 addendum) — stereo_mode selector, single Link knob re-targeted by mode, M/S-domain wideband unlink"
```

**Commit B — carried-over meter tuning (separate, honest):**
```bash
git add Source/ui/meters/GainReductionMeter.cpp Source/ui/meters/MeterGroupComponent.cpp
git commit -m "meter tuning: faster I/O bar release (40->60 dB/s) + clip LED fade (1.2->6.0 dB/s)"
```

**Commit C — submodule bump + docs (write §5 first):**
```bash
git add third_party/melechdsp-hq docs/PROGRESS.md PROMPTS/PLAN.md
git commit -m "MasterLimiter Slice 7b close: bump HQ submodule (ADR-0008 addendum) + PROGRESS/PLAN"
```

**Commit D — archive Slice 7b prompts (NOT SLICE_AUTO_RELEASE — next slice):**
```bash
git add PROMPTS/SLICE_7b_ms_link.md PROMPTS/SLICE_7b_1_single_link_knob.md PROMPTS/SLICE_7b_CLOSE.md
git commit -m "docs: archive Slice 7b prompts"
```
Leave `PROMPTS/SLICE_AUTO_RELEASE.md` untracked (it ships at its own close).

────────────────────────────────────────
5. DOCS CONTENT (write before Commit C)
────────────────────────────────────────

### `docs/PROGRESS.md` — new TOP entry
Slice 7b — M/S mode (ADR-0008 addendum):
- New frozen `stereo_mode` Choice (`Stereo`/`M/S`, default `Stereo`).
- Wideband final stage gains a domain branch: Stereo = existing L/R parallel
  envelopes (`stereo_link_pct`); M/S = lossless Mid/Side encode of the
  band-limited signal, same two wideband envelopes blended by `ms_link_pct`,
  decoded back. Reuses envelopes — no new latency/CPU; M/S is phase-transparent.
- `ms_link_pct` (dormant placeholder since Slice 7) is now active in M/S mode.
- UI: **single Link knob** re-targeted by the mode (swapped attachment;
  per-mode value recall) + Stereo/M-S toggle — collapses the earlier two-knob
  layout (7b.1).
- GR meter shows M|S components in M/S mode.
- Neutral default (Stereo + M/S 100%) → Slice 3/4/5 bit-identical.
- Also: meter tuning — I/O bar release 60 dB/s, clip LED fade 6.0 dB/s
  (post-15b audition tweaks, committed separately).

### `PROMPTS/PLAN.md`
- Mark **Slice 7b ✅ Shipped** (M/S mode).
- Remove "Slice 7b: M/S detection" from the **Backlog** (now shipped;
  `ms_link_pct` active).
- Active next: **Auto-release** (program-dependent release, 3 modes), then
  Slice 16 (UI/UX interaction + hide Lookahead/T/S + clip-ballistics tidy),
  Slice 17 (packaging), manual, beta.

────────────────────────────────────────
6. PHASE D — push product
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git push origin slice-7b-ms-link
git checkout main && git pull --ff-only && git merge --ff-only slice-7b-ms-link && git push origin main
```
Record the new product `main` SHA; confirm submodule pointer == new HQ master.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Pre-flight (build + Slice 3/4/5 PASS).
2. HQ: commit SHA, push + FF master, new master SHA, final status (only ` M MCP`).
3. Product: four commit SHAs, submodule pointer SHA, push + FF main, new main SHA.
4. Final `git status --short` both repos (product clean; HQ only ` M MCP`).
5. PROGRESS.md + PLAN.md excerpts.
6. Open questions.

────────────────────────────────────────
8. AFTER CLOSE
────────────────────────────────────────

Next: **Auto-release** (`SLICE_AUTO_RELEASE.md`) off the new `main`. Architect
authors its ADR on the HQ branch when it starts. **Do not start it here.**
