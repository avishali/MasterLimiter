# Cursor Task — Slice 8.1.2: warning fix + close (TWO clean commits)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only.** Slice 8 + 8.1 passed the UI audition. Fix the one
> remaining in-scope build warning, then commit the uncommitted work as
> **two clean commits** on the current branch `slice-8-1-ui-polish-wip`:
> first the Slice 8 meters DSP backend, then the Slice 8.1 UI polish.
> Do NOT push.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Stay within the files listed below. No HQ edits. No AnalyzerPro edits.
- The only code change is the single warning fix in §2. No DSP changes,
  no APVTS changes, no behavior change.
- Create commits on the existing branch. **Do NOT push, do NOT open a PR,
  do NOT force anything.** Two commits, in the order given.
- Frozen parameter IDs unchanged.

────────────────────────────────────────
1. PRECONDITION CHECK
────────────────────────────────────────

Confirm before doing anything:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current            # must be: slice-8-1-ui-polish-wip
git diff --quiet -- Source/parameters/Parameters.cpp && echo "Parameters.cpp CLEAN (good)" || echo "Parameters.cpp DIRTY — STOP"
git status --short
```

`Source/parameters/Parameters.cpp` must show **no** changes (reverted in
Slice 8.1.1). If it is dirty, STOP and report — do not commit.

────────────────────────────────────────
2. WARNING FIX (the only code change)
────────────────────────────────────────

There is one implicit `int → float` conversion warning in
`Source/ui/meters/MeterComponent.cpp` (you reported it in the Slice 8.1.1
build). Locate the exact line from the compiler output and make the
conversion **explicit** with `static_cast<float>(...)` (or use a float
literal where an int literal is feeding a float expression). Do not change
any numeric result — this is a pure type-clarity fix.

Report the file:line and the before/after of that line in your output.

Rebuild to confirm the warning is gone:

```bash
cmake --build build-debug -j
cmake --build build-release -j
```

Zero warnings from MasterLimiter `Source/` files. (HQ/JUCE/SDK
deprecation warnings are out of scope and expected.)

────────────────────────────────────────
3. COMMIT 1 — Slice 8 meters DSP backend
────────────────────────────────────────

Stage **only** these two files and commit:

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "$(cat <<'EOF'
MasterLimiter Slice 8: meters DSP backend — peak snapshots + LoudnessAnalyzer

Lock-free meter snapshot path (audio-thread writes, UI-thread reads via
relaxed atomics): input/output peak L/R, output TP (max-sample-peak
approximation), and an mdsp_dsp::LoudnessAnalyzer for M/S/I LUFS. Meters
are observational only — the limiter gain path is unchanged, bench Slice
3/4/5 PASS unchanged.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Verify nothing else slipped in:

```bash
git show --stat HEAD   # must list ONLY Source/PluginProcessor.h and .cpp
```

If `git show --stat` lists any other file, STOP and report (do not
proceed to commit 2).

────────────────────────────────────────
4. COMMIT 2 — Slice 8.1 UI polish
────────────────────────────────────────

Stage the remaining UI + docs + prompt files and commit:

```bash
git add CMakeLists.txt \
        Source/PluginEditor.h Source/PluginEditor.cpp \
        Source/ui/MainView.h Source/ui/MainView.cpp \
        Source/ui/meters/ Source/ui/loudness/ \
        Source/util/ \
        docs/TODO.md docs/PROGRESS.md \
        PROMPTS/SLICE_08_1_1_scope_revert.md \
        PROMPTS/SLICE_08_1_2_close.md \
        PROMPTS/SLICE_08_1_3_meter_occlusion_fix.md

git commit -m "$(cat <<'EOF'
MasterLimiter Slice 8.1: UI polish — AnalyzerPro meter/loudness pattern

Local meter + loudness components adapted from AnalyzerPro (no HQ lift this
slice): MeterComponent with a new top-down GR Kind and L/R bars,
MeterGroupComponent (IN/GR/OUT), LoudnessNumericPanel (fixes the ASCII
LUFS-label bug). MainView redesigned with a 280px meter strip, Sustain
Ratio knob, knob tooltips, and smoothed numeric readouts. Resizable editor
(1100x620 default, 960x540 min) on a 30 Hz timer that suspends meter
sync/repaint when the window is occluded (Slice 8.1.3 fix for audio clicks
when the DAW is on another macOS Space). Fixes one int->float warning in
MeterComponent.cpp. Out-of-scope Parameters.cpp retune was reverted
(Slice 8.1.1); it gets its own slice. PROGRESS + TODO updated.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

────────────────────────────────────────
5. POST-COMMIT VERIFY
────────────────────────────────────────

```bash
git status --short     # expect: clean (nothing uncommitted, nothing untracked)
git log --oneline -3   # expect: Slice 8.1 (top), Slice 8, then prior prompt commit
```

Working tree must be fully clean. Do NOT push.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Precondition check output (branch + Parameters.cpp clean confirmation).
2. Warning fix: file:line + before/after line; rebuild confirmation that
   no `Source/` warnings remain.
3. Commit 1: the `git show --stat HEAD` proving it contains only the two
   PluginProcessor files.
4. Commit 2: `git show --stat HEAD` file list.
5. Final `git status --short` (clean) and `git log --oneline -3`.
6. Explicit confirmation: **nothing was pushed.**
7. Open questions.

After this, the architect opens the DSP investigation slice (SP-mode
pops/clicks on full-mix material + compressor-like envelope character).
