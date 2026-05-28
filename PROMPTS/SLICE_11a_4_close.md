# Cursor Task — Slice 11a.4: re-close Slice 11a (fold fader-safety into the single slice commit)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. Pure git housekeeping + the architect-drafted
> PROGRESS update — NO CODE CHANGES.** The earlier close commit
> `01345e3` already landed on `slice-11a-io-gains` (before the fader-
> safety patch). We collapse that unpushed commit + the working tree
> (fader-safety + PROGRESS update + prompt rename) into **one clean
> Slice 11a commit**. Do NOT push. Do NOT merge to main — architect
> handles that. HQ ADR commit (`276c397`) is untouched.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- This task makes **no code edits** of its own. The MainView.cpp fader-
  safety change is already in the working tree (from Slice 11a.3); the
  PROGRESS.md update is already in the working tree (architect-written).
- No HQ edits.
- Stay on branch `slice-11a-io-gains`. **Do NOT push, do NOT merge to
  main, do NOT delete the branch.**
- The collapse uses `git reset --soft main` to combine the existing
  unpushed commit's content with the staged working-tree changes into a
  single new commit. Local-only history reshape — safe (nothing pushed).

────────────────────────────────────────
1. PRECONDITION CHECK
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current     # must be: slice-11a-io-gains

git log --oneline main..HEAD  # expect exactly ONE line:
                              #   01345e3 MasterLimiter Slice 11a: I/O gain stages ...

git status --short            # expect these (and only these):
                              #    D PROMPTS/SLICE_11a_3_close.md
                              #    M Source/ui/MainView.cpp
                              #    M docs/PROGRESS.md
                              #   ?? PROMPTS/SLICE_11a_3_fader_safety.md
                              #   ?? PROMPTS/SLICE_11a_4_close.md
```

If the precondition fails — wrong branch, no `01345e3` on the branch,
extra files dirty, etc. — **STOP and report**.

────────────────────────────────────────
2. STAGE EVERYTHING, COLLAPSE, RE-COMMIT
────────────────────────────────────────

```bash
# 1. Stage the working-tree changes by explicit paths:
git add Source/ui/MainView.cpp \
        docs/PROGRESS.md \
        PROMPTS/SLICE_11a_3_fader_safety.md \
        PROMPTS/SLICE_11a_4_close.md
git rm  PROMPTS/SLICE_11a_3_close.md   # the renamed-away file

# 2. Soft-reset to main — folds the prior 01345e3 content into the index
#    on top of what's already staged:
git reset --soft main

# 3. Sanity: status should now show ONLY staged changes (no unstaged,
#    no untracked), and the file list should cover the full Slice 11a:
git status --short
# Expected (every line should start with "A " or "M " — i.e. staged):
#   M  CMakeLists.txt                       (if 01345e3 touched it — usually no)
#   M  Source/PluginProcessor.h
#   M  Source/PluginProcessor.cpp
#   M  Source/parameters/ParameterIDs.h
#   M  Source/parameters/Parameters.cpp
#   M  Source/ui/MainView.h
#   M  Source/ui/MainView.cpp
#   M  Source/ui/meters/MeterGroupComponent.cpp
#   M  docs/PROGRESS.md
#   M  PROMPTS/PLAN.md
#   A  PROMPTS/SLICE_11a_io_gains.md
#   A  PROMPTS/SLICE_11a_1_clip_indicator.md
#   A  PROMPTS/SLICE_11a_2_click_to_reset.md
#   A  PROMPTS/SLICE_11a_3_fader_safety.md
#   A  PROMPTS/SLICE_11a_4_close.md
#
# Specifically, the old `PROMPTS/SLICE_11a_3_close.md` must NOT appear
# (it was added by 01345e3 and removed by our `git rm` — net zero, gone).
# If it appears, run `git rm --cached PROMPTS/SLICE_11a_3_close.md` and
# re-check.

# 4. Single clean Slice 11a commit:
git commit -m "$(cat <<'EOF'
MasterLimiter Slice 11a: I/O gain stages + Drive 0..+24 + Ceiling 0..-24 (ADR-0007)

Wires the gain-staging half of ADR-0007. Six new FROZEN parameter IDs
(io_input_l/r_db, io_input_link, io_output_l/r_db, io_output_link) plus
two range changes: input_gain_db 0..+24 (positive-only drive, default
0.0), ceiling_db -24..0 (default -1.0).

processBlock chain: per-channel I/O Input -> IN meter (post-IO-In, pre-
drive) -> mono Drive -> peak/envelope/lookahead/ceiling -> TP ispTrim
when in TP mode -> per-channel I/O Output -> OUT meter + TP (post-IO-
Out) -> LoudnessAnalyzer. RT-safe (relaxed atomic loads, no allocs, no
locks).

UI: Gain knob becomes a real SliderAttachment to input_gain_db. I/O
Input/Output L/R faders attach to the four new floats with guarded UI-
side L/R link mirroring. Clip indicator wired (peak >= 0 dB -> latched
LED) and click-on-LED-or-value resets peak hold + clip latch. I/O
faders are drag-only (setSliderSnapsToMousePosition false) so accidental
clicks on the meter area cannot spike the level while monitoring.

Bench Slice 3/4/5 PASS unchanged (defaults are byte-identical no-ops on
the audio path). No state-versioning needed (pre-1.0, no shipped
sessions). Gain-Match Auto/Track, Gain<->Ceiling Link, Learn Input
Gain, and Bypass are 11b.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

────────────────────────────────────────
3. POST-COMMIT VERIFY
────────────────────────────────────────

```bash
git log --oneline main..HEAD
# Expect exactly ONE line — the new Slice 11a close commit (NEW SHA,
# the old 01345e3 is gone, that's intended — local, unpushed, safe).

git show --stat HEAD | head -40
# Must list every Slice 11a file (the ones listed above in §2 step 3).
# It must NOT list PROMPTS/SLICE_11a_3_close.md (we renamed it to
# SLICE_11a_4_close.md). It must NOT list Source/PluginEditor.*,
# CMakeLists.txt (unless touched), or anything in HQ.

git status --short    # expect: clean (empty)
```

Working tree clean. Branch is one commit ahead of `main`. **No push.**
**No merge to `main`.**

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Precondition check output (showing the existing `01345e3` and the dirty
   working tree).
2. `git status --short` after step 3 in §2 (the staged set before commit).
3. The new commit SHA + `git show --stat HEAD` file list.
4. `git log --oneline main..HEAD` (one line, the new SHA).
5. Final `git status --short` (clean).
6. Explicit confirmation: **no push, no merge to main, no branch
   deletion, HQ untouched in this task.** The previous `01345e3` is no
   longer reachable from the branch (intentional — local-only collapse).
7. Open questions.

After this the architect fast-forwards `main` and pushes, then opens
Slice 11b.
