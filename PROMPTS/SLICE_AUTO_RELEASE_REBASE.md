# Cursor Task — Auto-release: rebase onto new main + rebuild (resume)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Resume the parked auto-release WIP.** It was WIP-committed and branched off
> `main` BEFORE the Slice 7b.2 M/S ceiling fix, so it doesn't have that fix yet.
> Rebase both repos' `slice-auto-release` branches onto the current
> `main`/`master`, rebuild, confirm the default path is still clean, and
> install for audition. **No new feature work, no push, no commit beyond the
> rebase itself.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- This is a git rebase + build, no new code. No push. Leave `MCP` untouched.
- If a rebase conflict is non-trivial / you're unsure how to resolve it, STOP
  and report the conflicting hunks rather than guessing.

────────────────────────────────────────
1. REBASE HQ SIBLING (LimiterEnvelope auto-release)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git fetch origin
git checkout slice-auto-release
git rebase origin/master
```
- The auto-release HQ change is in `LimiterEnvelope.{h,cpp}`; `master` advanced
  in `shared/dsp_bench/` (Top48Db, M/S overs test) — **different files, expect
  no conflict.** If one appears, keep both sides' changes.

────────────────────────────────────────
2. REBASE PRODUCT (auto-release feature)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git fetch origin
git checkout slice-auto-release
git rebase origin/main
```
- Likely conflict in `Source/PluginProcessor.cpp`: the auto-release WIP touches
  the per-block **envelope setup/wiring** (setAutoRelease / setAutoReleaseMode)
  and the envelope `process` path; `main` (7b.2) added the **M/S decoded-L/R
  safety bound** in the M/S apply branch. These are different regions —
  **keep BOTH**: the auto-release wiring AND the 7b.2 M/S ceiling fix must
  survive the rebase.
- Possible conflict in `Source/ui/MainView.cpp`/`.h` (auto-release UI vs any
  main UI). Keep both sides' intent.
- After resolving, verify the M/S fix is still present (`msSafetyGain` /
  decoded-L/R bound in the M/S branch) and the auto-release wiring is intact.

────────────────────────────────────────
3. REBUILD + VERIFY DEFAULT PATH CLEAN
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/AR_REBASE_S$SLICE"; done
# plus the M/S overs test → must still PASS (0/0), proving the 7b.2 fix survived the rebase
```
Gate: build clean (no new `Source/` warnings); Slice 3/4/5 PASS (Auto OFF
default bit-identical); **M/S overs test PASS** (the 7b.2 fix is intact). The
Release build auto-installs via `COPY_PLUGIN_AFTER_BUILD`.

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Rebase result for both repos (clean / conflicts resolved — list any hunks
   you resolved, especially in `PluginProcessor.cpp`).
2. Confirmation the 7b.2 M/S ceiling fix is present post-rebase.
3. Build summary (Debug + Release, warnings).
4. Slice 3/4/5 + M/S overs: PASS.
5. `git status --short` both repos (rebased branches; HQ only ` M MCP`).
6. Open questions.

────────────────────────────────────────
5. AFTER THIS
────────────────────────────────────────

The installed build is now `main` (incl. M/S fix) + auto-release. avishali
re-auditions: (1) the first-touch pop isolation tests, (2) the 3 auto-release
modes. The architect drives the pop fix and mode tuning from there. **Do not
start fixes here.**
