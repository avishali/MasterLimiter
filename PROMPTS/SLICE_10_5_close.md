# Cursor Task — Slice 10.5: close Slice 10 (collapse WIP + 10.4 into one clean commit)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. Pure git housekeeping + a PROGRESS update — NO CODE
> CHANGES.** Slice 10 (Maximizer UI shell + polish rounds 10.0/10.1/10.2/
> 10.3/10.4) has been audition-approved. Collapse the WIP commit (`2e5ec5f`)
> and the uncommitted 10.4 working-tree changes into **one clean Slice 10
> commit** on `slice-10-maximizer-ui-shell`. Do NOT push. Do NOT merge to
> main — architect handles that.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:
- This task makes **no code edits**. The only file content change is the
  `docs/PROGRESS.md` update the architect already wrote into the working tree.
- No HQ edits, no AnalyzerPro edits.
- Stay on branch `slice-10-maximizer-ui-shell`. **Do NOT push, do NOT merge
  to main, do NOT delete the branch.**

────────────────────────────────────────
1. PRECONDITION CHECK
────────────────────────────────────────

Verify before doing anything:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current     # must be: slice-10-maximizer-ui-shell
git log --oneline -3          # expect: 2e5ec5f WIP, 107ff1e Slice 8.1, c74d98f Slice 8
git status --short            # expect uncommitted 10.4 changes:
                              #   M Source/ui/MainView.cpp
                              #   M Source/ui/meters/MeterComponent.cpp
                              #   M Source/ui/meters/MeterGroupComponent.cpp
                              #   M docs/PROGRESS.md
                              #   ?? PROMPTS/SLICE_10_4_ui_refine4.md
                              #   ?? PROMPTS/SLICE_10_5_close.md

# Sanity: confirm no DSP / params / processor edits anywhere in the branch
git diff --stat main -- Source/PluginProcessor.cpp Source/PluginProcessor.h Source/parameters/
# (expect no output — these files must be untouched on this branch)
```

If any precondition fails — wrong branch, processor/params touched, unexpected
extra modifications — **STOP and report**.

────────────────────────────────────────
2. COLLAPSE TO ONE CLEAN COMMIT
────────────────────────────────────────

Stage the 10.4 working-tree changes and the two new prompt files, then
soft-reset the branch to `main` so the WIP + the just-staged 10.4 changes
become one combined staged set. Commit that as the single Slice 10 close.

```bash
# 1. Stage all current 10.4 changes (modified files + the new prompt files
#    in PROMPTS/ — explicit paths, do NOT use `git add -A`):
git add Source/ui/MainView.cpp \
        Source/ui/meters/MeterComponent.cpp \
        Source/ui/meters/MeterGroupComponent.cpp \
        docs/PROGRESS.md \
        PROMPTS/SLICE_10_4_ui_refine4.md \
        PROMPTS/SLICE_10_5_close.md

# 2. Move HEAD back to main while KEEPING everything staged (soft reset).
#    Combines the WIP commit's content with the just-staged 10.4 content
#    into one staged set:
git reset --soft main

# 3. Sanity: nothing should be unstaged after this; everything is staged.
git status --short

# 4. Single clean Slice 10 commit:
git commit -m "$(cat <<'EOF'
MasterLimiter Slice 10: Maximizer UI shell (Ozone-inspired two-panel layout)

Editor-only slice. NO DSP changes, NO new APVTS parameters, NO range/default/
ID changes to existing parameters. Bench unaffected.

What this lands
- Two-panel layout: MAXIMIZER (Gain drive placeholder, Output Level wired to
  ceiling_db, SP/TP toggle wired to ceiling_mode, Character slider wired,
  Release/Sustain/Lookahead/Auto/Stereo/MS knobs wired, Gain-Match toggles
  placeholders) and I/O (Learn placeholder, IN/OUT meters, vertical L/R
  fader handles on the meters as placeholders, L/R Link toggles, LUFS panel,
  Reset Peaks). Bypass placeholder in the header.
- Product LookAndFeel (Source/ui/MasterLimiterLookAndFeel.{h,cpp}) and
  dedicated GR meter (Source/ui/meters/GainReductionMeter.{h,cpp}, single
  bar, top-down, fine 0..-10 dB scale, no clip LED).
- Locked-aspect uniform scaling: setFixedAspectRatio(1100.0/620.0) on the
  constrainer + AffineTransform::scale on MainView at the design size.
- Slider readout formatting (single suffix + sensible precision; APVTS
  attachment text formatters cleared after attach so per-slider settings
  win). IN/OUT meter scale set to Top24Db.

Deferred to Slice 11
- Real Gain drive APVTS wiring, I/O Input/Output gain stages, Gain-Match
  (Auto/Track + Gain<->Ceiling Link), Learn Input Gain logic, Ceiling range
  change 0..-12 -> 0..-24. ADR captures the I/O + Gain-Match design at that
  point.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

────────────────────────────────────────
3. POST-COMMIT VERIFY
────────────────────────────────────────

```bash
# Single new commit on the branch above main, WIP gone:
git log --oneline main..HEAD
# Expect exactly ONE line — the Slice 10 close commit.

git show --stat HEAD | head -40
# Confirm the diff lists the expected files: CMakeLists.txt,
# PROMPTS/SLICE_10*.md (×5), Source/PluginEditor.{h,cpp},
# Source/ui/MainView.{h,cpp}, Source/ui/MasterLimiterLookAndFeel.{h,cpp},
# Source/ui/meters/MeterComponent.cpp,
# Source/ui/meters/MeterGroupComponent.{h,cpp},
# Source/ui/meters/GainReductionMeter.{h,cpp},
# Source/ui/loudness/LoudnessNumericPanel.cpp,
# docs/mockups/SLICE10_ui_layout.svg,
# docs/PROGRESS.md,
# PROMPTS/PLAN.md.
# It must NOT list Source/PluginProcessor.* or Source/parameters/* — if it
# does, STOP and report.

git status --short    # expect: clean (empty)
```

Working tree clean. Branch is one ahead of `main`. **No push.** **No merge
to `main`.**

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Precondition check output.
2. The single-commit SHA + `git show --stat HEAD` file list.
3. `git log --oneline main..HEAD` (one line expected).
4. Final `git status --short` (clean).
5. Explicit confirmation: **no push, no merge to main, no branch deletion**.
6. Open questions.

After this the architect fast-forwards `main` and pushes (same flow as the
Slice 8/8.1 close), then opens Slice 11.
