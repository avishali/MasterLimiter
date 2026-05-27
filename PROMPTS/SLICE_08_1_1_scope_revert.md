# Cursor Task — Slice 8.1.1: scope revert + clean build + bench regression (NO COMMIT)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only.** This is a scope-correction patch on the
> uncommitted Slice 8 + 8.1 work currently sitting on branch
> `slice-8-1-ui-polish-wip`. Slice 8.1 was declared **UI-only / no DSP /
> APVTS unchanged**, but `Source/parameters/Parameters.cpp` was modified
> (parameter range + default retune). Back that one file out, prove the
> build is clean, prove the DSP bench still passes, then **STOP without
> committing** — the architect auditions in Live before close.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- ONE focused diff. Touch only the file named in §3.
- No HQ modifications. AnalyzerPro read-only.
- No new DSP, no APVTS changes (this patch *removes* an out-of-scope
  APVTS change; it must not introduce any others).
- RT-safety: audio thread untouched by this patch.
- **Do NOT commit, do NOT create branches, do NOT push.** Leave the
  working tree dirty for architect audition. Commit instructions come
  in a later prompt after audition passes.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

The Slice 8.1 prompt (`PROMPTS/SLICE_08_1_ui_polish.md` §0, §7) froze
the processor and APVTS: *"No DSP changes. APVTS unchanged.
PluginProcessor.{h,cpp} is not modified in this slice."*

The implementation nonetheless edited `Source/parameters/Parameters.cpp`:

- `input_gain_db` range `-12..24` → `0..24` (removes attenuation; would
  silently clamp any saved session that stored a negative input gain).
- `release_ms` default `100` → `250`.
- `release_sustain_ratio` default `4.0` → `2.0`.

These may be reasonable retunes, but they are out of scope for a UI
slice and carry a saved-session-compat consequence. The architect's
decision: **revert them here**, keep Slice 8.1 strictly UI, and handle
the parameter retune in its own dedicated slice later (separate prompt,
with the saved-session migration question addressed explicitly).

Everything else in the working tree (the new meter/loudness UI
components, `MainView`, `PluginEditor`, `CMakeLists.txt`, `docs/TODO.md`,
and the Slice 8 `PluginProcessor` snapshot wiring) is **correct and
stays untouched**.

────────────────────────────────────────
2. TRINITY (lightweight)
────────────────────────────────────────

Read before editing:

- `Source/parameters/Parameters.cpp` (current dirty version vs HEAD).
- `git show HEAD:Source/parameters/Parameters.cpp` (the target state).

Confirm via `git diff Source/parameters/Parameters.cpp` that the ONLY
differences from HEAD are the three parameter blocks above (range +
defaults + their explanatory comments). If the diff shows anything
else in this file, STOP and report.

────────────────────────────────────────
3. SCOPE — the single file you may MODIFY
────────────────────────────────────────

**REVERT to HEAD (committed Slice 5 state):**
- `Source/parameters/Parameters.cpp`

Cleanest method:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout HEAD -- Source/parameters/Parameters.cpp
```

After revert, re-confirm:

```bash
git diff Source/parameters/Parameters.cpp   # must print NOTHING
```

**DO NOT TOUCH** any other file. The other 7 modified files
(`CMakeLists.txt`, `Source/PluginEditor.{h,cpp}`,
`Source/PluginProcessor.{h,cpp}`, `Source/ui/MainView.{h,cpp}`) and the
untracked UI dirs (`Source/ui/meters/`, `Source/ui/loudness/`,
`docs/TODO.md`) are in scope for Slice 8 / 8.1 and must remain as-is.

NON-GOALS:
- Do NOT re-add the param retune anywhere else.
- Do NOT modify the snapshot atomics or LoudnessAnalyzer wiring in
  `PluginProcessor` — that is the legitimate Slice 8 meters DSP and it
  stays.
- Do NOT commit / branch / push.

────────────────────────────────────────
4. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```

Zero errors. Report any warnings originating from MasterLimiter
`Source/` files (HQ/JUCE warnings out of scope).

### Bench regression (DSP must be byte-for-byte the committed behavior)

With Parameters.cpp back at HEAD and no other DSP touched, Slice 3/4/5
benches must PASS unchanged:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE08_1_1_REGRESSION_S$SLICE"
done
```

Each run must end `PASS N/N`. If any fails, the revert was incomplete
or something else got touched — STOP and report.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. RETRIEVAL LOG.
2. Confirmation that `git diff Source/parameters/Parameters.cpp` is now
   empty, and the full `git status --short` (to show the remaining
   in-scope Slice 8/8.1 working tree is intact and uncommitted).
3. Build summary — Debug + Release, both succeeded, warnings from
   `Source/` (expect none new).
4. Bench regression — invocation + final `PASS N/N` line for Slice
   3 / 4 / 5.
5. Explicit confirmation: **no commit was made, no branch created, no
   push.**
6. Open questions.

The architect will then audition the Release build in Ableton Live
(VST3 + AU). On audition pass the architect issues the close prompt:
two clean commits (Slice 8 meters DSP wiring, then Slice 8.1 UI polish)
and the PROGRESS.md update.
