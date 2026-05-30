# Cursor Task — Slice 17 CLOSE (beta packaging)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only close.** Presets + defaults + smoothing + version. No HQ → no
> submodule bump. Commit + push, FF `main`.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Stage by **explicit path only**. No force-push/amend/skip-hooks. Auth via
  system credential helper. `Co-Authored-By: Claude` trailer.
- No HQ commit / no submodule bump.

────────────────────────────────────────
1. PRE-FLIGHT
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-17-packaging
cmake --build build-release -j   # clean, no new Source/ warnings
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/SLICE17_CLOSE_S$SLICE"; done
```
Gate: build clean; Slice 3/4/5 PASS (no param ID/range changes). STOP on failure.

────────────────────────────────────────
2. COMMITS (explicit paths)
────────────────────────────────────────

**Commit A — packaging source:**
```bash
git add CMakeLists.txt Source/PluginProcessor.cpp Source/PluginProcessor.h \
        Source/parameters/Parameters.cpp Source/ui/MainView.cpp Source/ui/MainView.h \
        Source/ui/PresetManager.cpp Source/ui/PresetManager.h
git commit -m "MasterLimiter Slice 17: beta packaging — 5 factory presets (in-UI header menu) + sensible defaults + param smoothing + version 0.3.0 (beta)"
```

**Commit B — docs (write §3 first):**
```bash
git add docs/PROGRESS.md PROMPTS/PLAN.md
git commit -m "docs: Slice 17 close (beta packaging) + PROGRESS/PLAN"
```

**Commit C — archive Slice 17 prompt (leave SLICE_18 untracked — next slice):**
```bash
git add PROMPTS/SLICE_17_packaging.md PROMPTS/SLICE_17_CLOSE.md
git commit -m "docs: archive Slice 17 prompts"
```

────────────────────────────────────────
3. DOCS CONTENT (write before Commit B)
────────────────────────────────────────

### `docs/PROGRESS.md` — new TOP entry
Slice 17 — beta packaging (no param ID/range changes; Slice 3/4/5 unchanged):
- Defaults: `character` = Clean, I/O links off (ceiling −1.0 / SamplePeak /
  limiter-on confirmed) — fresh insert = the "Default" preset.
- 5 factory presets via an **in-UI header preset menu** (`PresetManager`):
  Default, Transparent Master, Loud & Aggressive, Gentle Glue, Clipper Punch
  (starting values — tunable). Presets set processing params only (not bypass/
  gain-match/I-O).
- Param smoothing added on Drive, Ceiling, Clipper drive, and I/O gains
  (`LinearSmoothedValue`) — no zipper.
- Version → **0.3.0**; header shows **v0.3.0 (beta) - Maximizer**.

### `PROMPTS/PLAN.md`
- Mark **Slice 17 ✅ Shipped** (beta packaging).
- Active next: **Slice 18** (code-signing + notarization + AAX/PACE build &
  signing). Note: **MasterLimiter AAX Product GUID =
  `75B5E420-5C80-11F1-9221-00505692C25A`** (catalog product GUID — verify
  whether it doubles as the wraptool wrap-config GUID; if wraptool returns
  WrapConfigNotFound, obtain the wrap-config GUID from PACE Fusion or use
  customernumber+customername). Credentials stay in the gitignored
  `scripts/.aax_wraptool.env`.
- Then: **user manual** (architect-authored), then **beta build**.
- Backlog unchanged (palette-injection centralization, full knob/slider
  re-draw, auto-release mode tuning, final transparent ceiling stage, HQ
  dual-meter-system consolidation).

────────────────────────────────────────
4. PUSH
────────────────────────────────────────

```bash
git push origin slice-17-packaging
git checkout main && git pull --ff-only && git merge --ff-only slice-17-packaging && git push origin main
```
Record new product `main` SHA; confirm submodule pointer unchanged.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Pre-flight (build + Slice 3/4/5 PASS). 2. Three commit SHAs. 3. Push + FF
main; new main SHA; submodule unchanged. 4. Final `git status --short` (clean
except untracked `SLICE_18` prompt). 5. PROGRESS/PLAN excerpts. 6. Open questions.

────────────────────────────────────────
6. AFTER CLOSE
────────────────────────────────────────

Next: **Slice 18** (signing/notarize/AAX) off the new `main`. **Do not start it
here.**
