# Cursor Task — Slice 11b1.4: close Slice 11b1 (single clean commit)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. Pure git housekeeping + the architect-drafted
> PROGRESS update — NO CODE CHANGES.** Slice 11b1 (incl. 11b1.1 latched
> max, 11b1.2 readout-fit, 11b1.3 dual-line) has been audition-approved.
> Stage and commit as **one clean Slice 11b1 commit** on
> `slice-11b1-gain-ceiling-link`. Do NOT push. Do NOT merge to main —
> architect handles that.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- This task makes **no code edits**. The only file-content change is the
  `docs/PROGRESS.md` Slice 11b1 entry the architect already wrote into the
  working tree.
- No HQ edits in this task. (ADR-0007 was already committed in HQ as
  `276c397` during Slice 11a — leave it as-is.)
- Stay on branch `slice-11b1-gain-ceiling-link`. **Do NOT push, do NOT
  merge to main, do NOT delete the branch.**

────────────────────────────────────────
1. PRECONDITION CHECK
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current     # must be: slice-11b1-gain-ceiling-link
git log --oneline main..HEAD  # expect NOTHING (no commits yet on branch)
git status --short            # expect these (and only these):
                              #   M PROMPTS/PLAN.md
                              #   M Source/PluginProcessor.cpp
                              #   M Source/PluginProcessor.h
                              #   M Source/parameters/ParameterIDs.h
                              #   M Source/parameters/Parameters.cpp
                              #   M Source/ui/MainView.cpp
                              #   M Source/ui/MainView.h
                              #   M Source/ui/meters/MeterComponent.cpp
                              #   M Source/ui/meters/MeterGroupComponent.cpp
                              #   M Source/ui/meters/MeterGroupComponent.h
                              #   M docs/PROGRESS.md
                              #   ?? PROMPTS/SLICE_11b1_gain_ceiling_link.md
                              #   ?? PROMPTS/SLICE_11b1_1_meter_readout.md
                              #   ?? PROMPTS/SLICE_11b1_2_readout_fit.md
                              #   ?? PROMPTS/SLICE_11b1_3_dual_readout.md
                              #   ?? PROMPTS/SLICE_11b1_4_close.md
```

If any precondition fails — wrong branch, commits already on branch,
unexpected extra modifications — **STOP and report**.

────────────────────────────────────────
2. ONE CLEAN COMMIT
────────────────────────────────────────

Stage every expected file by **explicit path** (do NOT `git add -A`)
and commit:

```bash
git add Source/parameters/ParameterIDs.h \
        Source/parameters/Parameters.cpp \
        Source/PluginProcessor.h \
        Source/PluginProcessor.cpp \
        Source/ui/MainView.h \
        Source/ui/MainView.cpp \
        Source/ui/meters/MeterComponent.cpp \
        Source/ui/meters/MeterGroupComponent.h \
        Source/ui/meters/MeterGroupComponent.cpp \
        docs/PROGRESS.md \
        PROMPTS/PLAN.md \
        PROMPTS/SLICE_11b1_gain_ceiling_link.md \
        PROMPTS/SLICE_11b1_1_meter_readout.md \
        PROMPTS/SLICE_11b1_2_readout_fit.md \
        PROMPTS/SLICE_11b1_3_dual_readout.md \
        PROMPTS/SLICE_11b1_4_close.md

git commit -m "$(cat <<'EOF'
MasterLimiter Slice 11b1: Gain<->Ceiling structural link + meter readout polish (ADR-0007)

Wires the Gain<->Ceiling Link half of ADR-0007. One new FROZEN bool param
gain_ceiling_link (default false). Inverse coupling listener on the
processor: when link is on, Delta on input_gain_db writes -Delta to
ceiling_db (and vice versa), clamped to each range, with a
couplingInProgress_ feedback-loop guard. Toggling the link captures
current values as the baseline so the user sees no jump on enable.

UI: the existing Gain/Ceiling Link button (placeholder since Slice 10)
gets a ButtonAttachment to the new bool. No layout change.

Meter readout polish (11b1.1 / 11b1.2 / 11b1.3 folded in):
- Latched max-peak per channel (numeric matches the yellow max-peak
  tick line); cleared by Reset Peaks, click-on-LED, click-on-value.
- Number-only format for IN/OUT meter readouts (TP readout and
  knob/fader readouts elsewhere still carry their " dB" suffix).
- Two-line layout restored with distinct values: top = current peak
  (smoothed, 1s hold + decay); bottom = latched max. 12 px font in the
  existing 30 px area.

Behavioural: Link is structural by design (no measurement). Output
level constancy is exact only when the limiter is actively clamping;
universal-match is Slice 11b2's Auto/Track (uses the Slice 8
LoudnessAnalyzer). Real per-channel RMS, the meter panel restructure,
and IN/OUT calibration check are deferred to Slice 12.

Bench Slice 3/4/5 PASS unchanged (gain_ceiling_link default OFF; no
DSP audio-path change).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

────────────────────────────────────────
3. POST-COMMIT VERIFY
────────────────────────────────────────

```bash
git log --oneline main..HEAD
# Expect exactly ONE line — the Slice 11b1 close commit.

git show --stat HEAD | head -30
# Must list ONLY the staged files above. It must NOT list any other
# file — in particular NOT Source/PluginEditor.*, NOT CMakeLists.txt,
# NOT anything in HQ, NOT GainReductionMeter.*, NOT
# MasterLimiterLookAndFeel.*, NOT LoudnessNumericPanel.*. If it does,
# STOP and report.

git status --short    # expect: clean (empty)
```

Working tree clean. Branch is one commit ahead of `main`. **No push.**
**No merge to `main`.**

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Precondition check output.
2. The single commit SHA + `git show --stat HEAD` file list.
3. `git log --oneline main..HEAD` (one line expected).
4. Final `git status --short` (clean).
5. Explicit confirmation: **no push, no merge to main, no branch
   deletion, HQ untouched in this task.**
6. Open questions.

After this the architect fast-forwards `main` and pushes, then opens
Slice 9.
