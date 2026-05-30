# Cursor Task — Slice 15 CLOSE (meter ballistics, GR + Clip)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only close.** avishali approved the GR/Clip meter ballistics
> (Slice 15 + 15.1 reuse refactor). Commit + push the product UI work + docs +
> prompt archive, fast-forward `main`. **No HQ change** this slice (the
> ballistics reuse existing `mdsp_ui` components — no HQ source edits, no
> submodule bump).

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Stage by **explicit path only** (never `git add -A`/`.`).
- No force-push, no skip-hooks, no amend. Auth via system credential helper;
  STOP if interactive auth needed.
- End commit messages with the standard `Co-Authored-By: Claude` trailer.
- This slice touched no HQ files — do not commit or bump anything under
  `third_party/melechdsp-hq`. Leave the HQ `MCP` pointer untouched.

────────────────────────────────────────
1. PRE-FLIGHT
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current      # expect slice-15-meter-ballistics
git status --short             # expect: M MainView.{cpp,h}, M GainReductionMeter.{cpp,h}; ?? PROMPTS/*
cmake --build build-release -j  # clean, no new Source/ warnings
```
UI-only change → bench unaffected; a Slice 3 quick run is optional sanity
(expected unchanged). If the build fails, STOP.

────────────────────────────────────────
2. COMMITS (explicit paths)
────────────────────────────────────────

**Commit 1 — product source:**
```bash
git add Source/ui/MainView.cpp Source/ui/MainView.h \
        Source/ui/meters/GainReductionMeter.cpp Source/ui/meters/GainReductionMeter.h
git commit -m "MasterLimiter Slice 15: GR + Clip meter ballistics (instant attack / ~300 ms release / peak-hold) reusing mdsp_ui MeterBallistics + PeakHoldModel"
```

**Commit 2 — docs (write per §3 first):**
```bash
git add docs/PROGRESS.md PROMPTS/PLAN.md
git commit -m "docs: Slice 15 close (meter ballistics) + backlog (HQ meter-system consolidation, clip-ballistics file tidy)"
```

**Commit 3 — archive Slice 15 prompts (NOT 15b — that ships at its own close):**
```bash
git add PROMPTS/SLICE_15_meter_ballistics.md PROMPTS/SLICE_15_1_reuse_hq_ballistics.md PROMPTS/SLICE_15_CLOSE.md
git commit -m "docs: archive Slice 15 prompts"
```
Leave `PROMPTS/SLICE_15b_io_meter_features.md` untracked (next slice).

────────────────────────────────────────
3. DOCS CONTENT (write before Commit 2)
────────────────────────────────────────

### `docs/PROGRESS.md` — new TOP entry
Slice 15 — meter ballistics (GR + Clip), UI-only, no audio/param/bench change:
- GR meter: replaced the symmetric 100 ms smoothing with **instant attack /
  ~300 ms release** + **per-channel peak-hold** (~1 s hold, ~12 dB/s falloff).
- Clip: smoothed current-value readout; LED lights instantly, holds ~1 s, fades.
- Reuses HQ `mdsp_ui::meters::MeterBallistics` + `PeakHoldModel` (no duplicate
  product helper; the product-local helper added then removed in 15.1).
- Implementation note: a clean compilation firewall isolates the new-system
  headers (`MeterTypes.h`) from `MainView.cpp` (which transitively pulls the
  old-system `MeterRenderState.h`) — the clip-ballistics free functions live in
  the `GainReductionMeter.cpp` TU behind an opaque `ClipBallisticsState`.

### `PROMPTS/PLAN.md`
- Mark **Slice 15 ✅ Shipped** (meter ballistics).
- Note **Slice 15b** (I/O meter features: RMS, range +/-, peak reset, centre
  scale) as the active next slice; then 16 (UI/UX interaction), 17 (packaging),
  manual, beta.
- **Backlog additions:**
  - *HQ — consolidate the dual `mdsp_ui` meter systems:* `MeterTypes.h` and
    `MeterRenderState.h` both define `mdsp_ui::meters::MeterRenderState`,
    colliding in any TU including both. Forced the Slice 15 firewall and will
    recur. Post-beta HQ cleanup slice.
  - *Product tidy:* move the clip-ballistics free functions out of
    `GainReductionMeter.cpp` into their own `ClipBallistics.cpp`; fold into
    Slice 16 (UI interaction) which touches these files anyway.

────────────────────────────────────────
4. PUSH
────────────────────────────────────────

```bash
git push origin slice-15-meter-ballistics
git checkout main && git pull --ff-only && git merge --ff-only slice-15-meter-ballistics && git push origin main
```
Record the new product `main` SHA. Confirm the submodule pointer is unchanged
(no HQ bump this slice).

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Pre-flight (build clean).
2. The three commit SHAs.
3. Push + FF main confirmation; new main SHA; submodule pointer unchanged.
4. Final `git status --short` (clean except the untracked `SLICE_15b` prompt).
5. PROGRESS.md + PLAN.md excerpts.
6. Open questions.

────────────────────────────────────────
6. AFTER CLOSE
────────────────────────────────────────

Slice 15 shipped. Next: **Slice 15b** (I/O meter features) — branch off the new
`main`. The architect proceeds after this close lands clean. **Do not start 15b
in this task.**
