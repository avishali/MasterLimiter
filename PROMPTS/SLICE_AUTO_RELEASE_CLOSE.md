# Cursor Task — Auto-release CONSOLIDATED CLOSE

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product + HQ close.** avishali approved auto-release (mode fine-tuning
> deferred post-beta). The branches currently carry a single "WIP" commit each;
> **re-commit cleanly** (the WIP message must not reach `main`/`master`), commit
> the ADR-0011 (architect-authored, already in the sibling working tree), bump
> the submodule, docs + prompt archive, FF both remotes.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Stage by **explicit path only**. No force-push (the soft-reset below is local,
  pre-push, on an unpushed branch — allowed). No amend of pushed commits, no
  skip-hooks. Auth via system credential helper. `Co-Authored-By: Claude`
  trailer.
- HQ commit on the **sibling**. Leave `MCP` dirty/untouched.
- Both repos are on `slice-auto-release`, each = upstream base + one local
  unpushed WIP commit (rebased clean).

────────────────────────────────────────
1. PRE-FLIGHT
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-auto-release
cmake --build build-release -j
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/AR_CLOSE_S$SLICE"; done
# plus the M/S overs test → must still PASS (the 7b.2 fix is in this branch)
```
Gate: build clean; Slice 3/4/5 PASS (Auto OFF default bit-identical); M/S overs
PASS. If anything fails, STOP.

────────────────────────────────────────
2. PHASE A — HQ: re-commit cleanly + ADR, push (sibling)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git fetch origin
git reset --soft origin/master    # un-commit the WIP, keep all changes staged/working
git status --short                # expect: M MCP, M LimiterEnvelope.{h,cpp}, ?? ADR-0011-...md
git add shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h \
        shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp
git commit -m "mdsp_dsp: LimiterEnvelope program-dependent auto-release (3 modes)"
git add docs/DECISIONS/ADR-0011-masterlimiter-auto-release.md
git commit -m "ADR-0011: auto-release (program-dependent release, 3 modes)"
git push origin slice-auto-release
git checkout master && git pull --ff-only && git merge --ff-only slice-auto-release && git push origin master
```
Confirm status shows only ` M MCP`. Record new HQ master SHA.

────────────────────────────────────────
3. PHASE B — bump product submodule
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/third_party/melechdsp-hq
git fetch origin
git checkout <new HQ master SHA>
```

────────────────────────────────────────
4. PHASE C — product: re-commit cleanly, push
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git reset --soft origin/main      # un-commit the WIP, keep all changes
git status --short                # auto-release source changes + submodule (from §B) + prompts
```

**Commit A — auto-release feature:**
```bash
git add Source/PluginProcessor.cpp Source/PluginProcessor.h \
        Source/parameters/ParameterIDs.h Source/parameters/Parameters.cpp \
        Source/ui/MainView.cpp Source/ui/MainView.h
git commit -m "MasterLimiter auto-release: program-dependent release + Transparent/Balanced/Reactive mode selector (ADR-0011)"
```

**Commit B — submodule bump + docs (write §5 first):**
```bash
git add third_party/melechdsp-hq docs/PROGRESS.md PROMPTS/PLAN.md
git commit -m "MasterLimiter auto-release close: bump HQ submodule (ADR-0011 + envelope) + PROGRESS/PLAN"
```

**Commit C — archive prompts:**
```bash
git add PROMPTS/SLICE_AUTO_RELEASE.md PROMPTS/SLICE_AUTO_RELEASE_REBASE.md PROMPTS/SLICE_AUTO_RELEASE_CLOSE.md
git commit -m "docs: archive auto-release prompts"
```

────────────────────────────────────────
5. DOCS CONTENT (write before Commit B)
────────────────────────────────────────

### `docs/PROGRESS.md` — new TOP entry
Auto-release (ADR-0011):
- New frozen `auto_release_mode` Choice (Transparent/Balanced/Reactive,
  default Transparent); existing `release_auto` bool now functional.
- `LimiterEnvelope` program-dependent release: per-mode precomputed fast/slow
  release coefficients blended per-sample by a sustain factor (no per-sample
  exp); applied to all four envelopes. Off by default → Slice 3/4/5
  bit-identical.
- UI: Auto toggle greys the manual Release knob; mode selector active when on.
- Note: Auto on holds reduction longer through sustained passages → more
  *sustained* GR on the meter (peak depth unchanged) — expected anti-pump
  behaviour. **Per-mode constant fine-tuning deferred to post-beta.**
- The earlier "first-touch pop" did not reproduce after a clean rebuild
  off current `main` (stale-build artifact); on watch during beta.

### `PROMPTS/PLAN.md`
- Mark **auto-release ✅ Shipped** (ADR-0011). Dead-control sweep: M/S +
  auto-release now real features; **Lookahead + T/S still to be hidden in
  Slice 16.**
- Active next: **Slice 16** (UI/UX interaction: type-in values, tooltips,
  label clarity, layout polish + Color placement, **hide Lookahead + T/S**,
  clip-ballistics file tidy), then 17 (packaging), manual, beta.
- Backlog: **auto-release mode fine-tuning** (post-beta); final transparent
  inter-sample-peak-safe ceiling stage; HQ dual-meter-system consolidation.

────────────────────────────────────────
6. PHASE D — push product
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git push origin slice-auto-release
git checkout main && git pull --ff-only && git merge --ff-only slice-auto-release && git push origin main
```
Record new product `main` SHA; confirm submodule pointer == new HQ master.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Pre-flight (build + Slice 3/4/5 + M/S overs PASS).
2. HQ: two commit SHAs, FF master, new master SHA, final status (` M MCP`).
3. Product: three commit SHAs, submodule pointer, FF main, new main SHA.
4. Final `git status --short` both repos.
5. PROGRESS/PLAN excerpts.
6. Open questions.
