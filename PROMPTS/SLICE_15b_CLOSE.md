# Cursor Task — Slice 15b CONSOLIDATED CLOSE (I/O meter features + Top48Db)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product + HQ close.** avishali approved the I/O meters (RMS opt-in, range
> incl. 48 dB, peak reset, centre scale, bar release, peak colour) and the
> tightened GR release. Commit + push the HQ `Top48Db` add (sibling), bump the
> product submodule, commit the product meter work + docs + prompt archive,
> fast-forward both remotes.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Stage by **explicit path only** (never `git add -A`/`.`).
- No force-push, no skip-hooks, no amend. Auth via system credential helper;
  STOP if interactive auth needed. End commit messages with the
  `Co-Authored-By: Claude` trailer.
- HQ commit on the **sibling** `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq`
  (branch `slice-15b-meter-scale`). Leave the HQ `MCP` pointer dirty/untouched.

────────────────────────────────────────
1. PRE-FLIGHT
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current       # expect slice-15b-io-meter-features
cmake --build build-release -j  # clean, no new Source/ warnings
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/SLICE15b_CLOSE_S$SLICE"; done
```
Gate: build clean; Slice 3/4/5 PASS (UI/measurement-only → unchanged). If
anything fails, STOP.

────────────────────────────────────────
2. PHASE A — HQ commit + push (sibling)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git branch --show-current       # expect slice-15b-meter-scale
git status --short              # expect: M MCP (leave), M MeterRenderState.h, M MeterRenderStateProvider.cpp
git add shared/mdsp_ui/include/mdsp_ui/meters/MeterRenderState.h \
        shared/mdsp_ui/src/meters/MeterRenderStateProvider.cpp
git commit -m "mdsp_ui: add Top48Db meter scale mode (48 dB range)"
git push origin slice-15b-meter-scale
git checkout master && git pull --ff-only && git merge --ff-only slice-15b-meter-scale && git push origin master
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
git status --short              # third_party/melechdsp-hq now shows modified (gitlink)
```

────────────────────────────────────────
4. PHASE C — product commits (explicit paths)
────────────────────────────────────────

**Commit A — product source:**
```bash
git add Source/PluginProcessor.cpp Source/PluginProcessor.h \
        Source/ui/MainView.cpp Source/ui/MainView.h \
        Source/ui/meters/GainReductionMeter.cpp \
        Source/ui/meters/MeterComponent.cpp Source/ui/meters/MeterComponent.h \
        Source/ui/meters/MeterGroupComponent.cpp Source/ui/meters/MeterGroupComponent.h
git commit -m "MasterLimiter Slice 15b: I/O meter features — RMS bar+numeric (opt-in), range +/- incl 48 dB, click-to-reset peak, shared centre scale, bar release; GR release 300->50 ms"
```

**Commit B — submodule bump + docs (write §5 first):**
```bash
git add third_party/melechdsp-hq docs/PROGRESS.md PROMPTS/PLAN.md
git commit -m "MasterLimiter Slice 15b close: bump HQ submodule (Top48Db) + PROGRESS/PLAN"
```

**Commit C — archive Slice 15b prompts:**
```bash
git add PROMPTS/SLICE_15b_io_meter_features.md \
        PROMPTS/SLICE_15b_1_meter_refinements.md \
        PROMPTS/SLICE_15b_2_io_release_and_peak_colour.md \
        PROMPTS/SLICE_15b_CLOSE.md
git commit -m "docs: archive Slice 15b prompts"
```

────────────────────────────────────────
5. DOCS CONTENT (write before Commit B)
────────────────────────────────────────

### `docs/PROGRESS.md` — new TOP entry
Slice 15b — I/O level meter features (UI + read-only RMS measurement; no
audio change, Slice 3/4/5 unchanged):
- Engine: per-bus per-channel **RMS** (one-pole ~300 ms, measurement-only,
  new atomics/getters) at the existing I/O measurement points.
- I/O meters: **RMS bar fill + numeric**, gated by a **shared "RMS" toggle
  (default OFF / sample-peak only)**; **range +/-** stepping Full → 48 → 24 →
  12 → 6 dB (shared across In+Out); **click-to-reset** peak line/maxes;
  **shared centre dB scale** between the bars (bars clean via
  `setDrawInternalScale(false)`), uniform colour, no red zone; **bar display
  release** (~20 dB/s) so bars glide down on stop; sample-peak bar recoloured
  to light blue.
- GR meter: release tightened **300 → 50 ms** (instant attack + peak-hold) so
  individual limiting events read clearly (supersedes the Slice 15 300 ms note).
- HQ: `mdsp_ui` adds `MeterScaleMode::Top48Db` (+ provider −48 dB mapping).

### `PROMPTS/PLAN.md`
- Mark **Slice 15b ✅ Shipped** (I/O meter features). Note the **meters are
  now complete** (GR ballistics + full Ozone-style I/O meter).
- Active next: **Slice 16** (UI/UX interaction: type-in parameter values,
  tooltips, label clarity, final layout polish, Color-knob placement) — and
  fold in the **clip-ballistics file tidy** (move out of `GainReductionMeter.cpp`).
- Then Slice 17 (packaging) → manual → beta.
- Keep backlog: **HQ dual-meter-system consolidation** (`MeterTypes.h` vs
  `MeterRenderState.h` duplicate `MeterRenderState`) — post-beta.

────────────────────────────────────────
6. PHASE D — push product
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git push origin slice-15b-io-meter-features
git checkout main && git pull --ff-only && git merge --ff-only slice-15b-io-meter-features && git push origin main
```
Record the new product `main` SHA; confirm the submodule pointer == new HQ
master.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Pre-flight (build + Slice 3/4/5 PASS).
2. HQ: commit SHA, push + FF master, new master SHA, final status (only ` M MCP`).
3. Product: three commit SHAs, submodule pointer SHA, push + FF main, new main SHA.
4. Final `git status --short` both repos (product clean; HQ only ` M MCP`).
5. PROGRESS.md + PLAN.md excerpts.
6. Open questions.

────────────────────────────────────────
8. AFTER CLOSE
────────────────────────────────────────

Meters complete. Next: **Slice 16** (UI/UX interaction) off the new `main`.
Architect proceeds after this close lands clean. **Do not start 16 here.**
