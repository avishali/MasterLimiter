# Cursor Task — Slice 14 CONSOLIDATED CLOSE (2-band multiband + Color, ADR-0009)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Close + ship the entire Slice 14 family.** avishali approved the audition
> (de-pumping competitive with Ozone; GR meter tracks push; Color sweep good).
> This commits and pushes everything: HQ (ADR-0009 + bench) and product
> (multiband DSP + Color param/UI + total-GR meter + docs + prompt archive),
> bumps the submodule, and deletes the audition scaffolding. The architect has
> already finalised ADR-0009 (Revisions R5/R6/R7 + §5/§6 supersede notes).

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Commit/push permitted (this is the close). Stage by **explicit path only** —
  never `git add -A` / `git add .` — so nothing stray is swept in.
- No force-push, no skip-hooks, no amend of prior commits. Auth via the system
  credential helper only; STOP if interactive auth is needed.
- HQ commits happen in the **sibling** working copy
  `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq`
  (branch `slice-14-multiband-bench`), NOT the product submodule.
- The HQ `MCP` nested-submodule pointer stays dirty as it has since the
  ADR-0007 close — **do NOT stage or touch it.**
- End commit messages with the standard `Co-Authored-By: Claude` trailer.

────────────────────────────────────────
1. PRE-FLIGHT VERIFY (build + bench on the to-be-shipped state)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current     # expect slice-14-multiband-2band
cmake --build build-debug -j
cmake --build build-release -j
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE14_CLOSE_S$SLICE"
done
```
Gate: Debug + Release build clean (no new `Source/` warnings); Slice 3/4/5
PASS. If anything fails, STOP and report — do not commit.

────────────────────────────────────────
2. PHASE A — HQ commit + push (sibling working copy)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git branch --show-current     # expect slice-14-multiband-bench
git status --short            # expect: M MCP (leave it), M bench files, ?? ADR-0009
```

**Commit 1 — ADR (docs), explicit path:**
```bash
git add docs/DECISIONS/ADR-0009-masterlimiter-multiband-detection.md
git commit -m "ADR-0009: 2-band multiband (frequency-selective) limiting + Revisions R1-R7"
```

**Commit 2 — bench, explicit paths:**
```bash
git add shared/dsp_bench/bench.py shared/dsp_bench/criteria.py \
        shared/dsp_bench/measure.py shared/dsp_bench/signals.py
git commit -m "dsp_bench: Slice 14 multiband — cross-band IMD + pumping diagnostic, per-band GR, SP-over magnitude, default-config criteria recal"
```

Confirm `git status --short` now shows only ` M MCP` (untouched). Then push +
fast-forward master:
```bash
git push origin slice-14-multiband-bench
git checkout master && git pull --ff-only && git merge --ff-only slice-14-multiband-bench && git push origin master
```
Record the new HQ master SHA.

────────────────────────────────────────
3. PHASE B — bump product submodule to the new HQ master
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/third_party/melechdsp-hq
git fetch origin
git checkout <new HQ master SHA from Phase A>
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short            # third_party/melechdsp-hq now shows as modified (gitlink)
```

────────────────────────────────────────
4. PHASE C — product commits (source, submodule+docs, prompts)
────────────────────────────────────────

**Commit A — product DSP/UI source, explicit paths:**
```bash
git add Source/PluginProcessor.cpp Source/PluginProcessor.h \
        Source/parameters/ParameterIDs.h Source/parameters/Parameters.cpp \
        Source/ui/MainView.cpp Source/ui/MainView.h
git commit -m "MasterLimiter Slice 14: 2-band multiband (ADR-0009) — serial two-lookahead ceiling, band headroom, Color param, total-GR meter"
```

**Commit B — submodule bump + docs:**
- First update `docs/PROGRESS.md` (TOP entry) and `PROMPTS/PLAN.md` per §5.
```bash
git add third_party/melechdsp-hq docs/PROGRESS.md PROMPTS/PLAN.md
git commit -m "MasterLimiter Slice 14 close: bump HQ submodule (ADR-0009 + bench) + PROGRESS/PLAN"
```

**Commit C — archive slice prompts (incl. the Slice 13 stragglers):**
```bash
git add PROMPTS/SLICE_13_CLOSE.md PROMPTS/SLICE_13_lufs_calibration.md \
        PROMPTS/SLICE_14_multiband_2band.md \
        PROMPTS/SLICE_14_1_wideband_post_band_detection.md \
        PROMPTS/SLICE_14_2_pumping_test_and_band_gr_diagnostic.md \
        PROMPTS/SLICE_14_3_band_threshold_headroom.md \
        PROMPTS/SLICE_14_4_serial_lookahead_ceiling_fix.md \
        PROMPTS/SLICE_14_5_dual_objective_config_sweep.md \
        PROMPTS/SLICE_14_6_color_control.md \
        PROMPTS/SLICE_14_7_total_gr_meter_fix.md \
        PROMPTS/SLICE_14_CONSOLIDATED_CLOSE.md
git commit -m "docs: archive Slice 13/14 Cursor prompts"
```

────────────────────────────────────────
5. DOCS CONTENT (write before Commit B)
────────────────────────────────────────

### `docs/PROGRESS.md` — new TOP entry

Cover, comprehensively: Slice 14 ships 2-band frequency-selective (multiband)
limiting per ADR-0009, the one remaining Ozone-Maximizer gap (cross-band
pumping). Note the arc honestly:
- 2-band LR4 @ 120 Hz inside the 4× OS region; existing chain reused as the
  final wideband brickwall.
- **Serial two-lookahead** (14.4) — required to guarantee the ceiling; the
  single-lookahead estimate (14.1) leaked overs on dense material. Reported
  latency 301 → **541 samples**.
- **Band headroom** 2 dB (14.3) — bands are the primary limiter; closed the
  HF-ducking gap to Ozone (default Color ≈ −21.5 dB vs Ozone −22.8).
- **`band_color` "Color"** param (14.6) — `0..100 %`, default 50 % (Balanced);
  maps to band-link 0.5→0.0; smoothed; UI knob with 0/50/100 detents.
- **Total-GR meter** (14.7) — meter reports band × wideband reduction (was
  wideband-only, capped at ~4 dB).
- Bench: new cross-band IMD + pumping diagnostic + per-band GR; Slice 3/4/5
  recalibrated treatment-B at the default Color (imd→11.6, SP-overs→2400, etc.,
  all commented `ADR-0009`); audio unchanged by the 14.7 meter fix.
- Audition: competitive with Ozone, slightly brighter; approved.
- Latency now 541; ADR-0009 R5/R6/R7.

### `PROMPTS/PLAN.md`

- Mark **Slice 14 ✅ Shipped** (2-band multiband + Color, ADR-0009) with a
  one-paragraph summary mirroring the table style of slices 7/9/11/12.
- Add the **beta-prep batch** to the roadmap as the next work:
  - **Slice 15 — Meter ballistics** (GR + Clip + loudness: attack / release-
    decay / peak-hold).
  - **Slice 16 — UI/UX interaction** (type-in parameter values / double-click
    edit; tooltips; label clarity; final layout + resize polish; Color knob
    placement lock).
  - **Slice 17 — Beta packaging** (param smoothing + default-state audit;
    factory preset(s); visible version stamp).
  - **User manual / instructions** — authored by the architect (doc), after
    15–16 freeze the control surface.
  - Goal of the batch: ship a beta build for avishali's beta tester.
- Keep the STFT "Max Transparency" rung (ADR-0009 R4/Alternatives) in the
  backlog as the path to fuller Ozone parity if real-world use demands it.

────────────────────────────────────────
6. PHASE D — push product
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git push origin slice-14-multiband-2band
git checkout main && git pull --ff-only && git merge --ff-only slice-14-multiband-2band && git push origin main
```
Record the new product main SHA. Confirm submodule pointer == new HQ master.

────────────────────────────────────────
7. PHASE E — scaffolding cleanup (disk only; these dirs are gitignored)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
rm -rf build-rel-* build-coexist-* build-release-banlink0
```
Confirm `git status` is unaffected by the deletion (they were gitignored).
Leave `build-debug` and `build-release` intact. The `MDSP_BAND_HEADROOM_DB`
macro guard may remain (fixed 2 dB default, harmless); `MDSP_BAND_LINK` was
already removed in 14.6 — confirm no dangling references.

Installed plugins: only `~/Library/Audio/Plug-Ins/VST3/MasterLimiter.vst3`
should remain (the L025/L050 audition bundles were already removed). Confirm.

────────────────────────────────────────
8. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Pre-flight: build + Slice 3/4/5 PASS.
2. HQ: the two commit SHAs, push + FF master confirmation, new master SHA,
   final `git status --short` (only ` M MCP`).
3. Product: the three commit SHAs, submodule pointer SHA, push + FF main
   confirmation, new main SHA.
4. Final `git status --short` on both repos (product clean; HQ only ` M MCP`).
5. Scaffolding cleanup confirmation; installed-plugins check.
6. PROGRESS.md + PLAN.md excerpts (the new entries).
7. Open questions.

────────────────────────────────────────
9. AFTER CLOSE
────────────────────────────────────────

Slice 14 ships. Next: the beta-prep batch (Slice 15 meter ballistics first).
The architect opens Slice 15 after this close lands clean. **Do not start 15
in this task.**
