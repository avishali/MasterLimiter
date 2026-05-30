# Cursor Task — Slice 7b: M/S Link (Stereo ↔ M/S domain mode)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Implements the deferred M/S control (ADR-0008 §4).** Adds a Stereo/M-S
> **mode selector**: the wideband stereo-unlink runs in either the L/R domain
> (existing `stereo_link_pct`) or the **M/S** domain (`ms_link_pct`, currently a
> dead control — this activates it). Default = Stereo mode, M/S Link 100%
> (linked) → bit-identical to today. Reuses the two wideband envelopes (no new
> envelopes, no added latency). Product DSP + UI + one new frozen param + an
> HQ ADR-0008 addendum. **Do NOT push** — architect closes.

> **Ordering:** branch off product `main` AFTER Slice 15b has closed. If 15b is
> still open, STOP and say so.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product edits: `Source/parameters/ParameterIDs.h` (the ID already exists —
  add only the new mode ID), `Source/parameters/Parameters.cpp`,
  `Source/PluginProcessor.{h,cpp}`, `Source/ui/MainView.{cpp,h}`.
- HQ edit: the architect will provide the **ADR-0008 addendum**
  (`docs/DECISIONS/ADR-0008-masterlimiter-stereo-unlink.md`) on the sibling
  M/S branch — do not write the ADR yourself; leave the doc to the architect.
- **New frozen parameter ID** `stereo_mode` — permanent once shipped.
- RT-safety: M/S encode/decode is a stateless per-sample matrix; no new
  allocation/locks/logging on the audio thread.
- Branch `slice-7b-ms-link` off product `main`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `Source/PluginProcessor.cpp` — the wideband final stage of the serial chain:
  the band-limited buffer `bandLimitedBuf_`, **Pass C** (wideband detection
  from `bandLimitedBuf_` → `peakBuf_`/`peakBufR_` → fast-path vs unlinked
  envelopes `envelope_`/`envelope_R_` → `gainBuf_`/`gainBufR_`), **Pass D**
  (apply via `lookaheadWide_`), the `stereo_link_pct` fast-path
  (`link >= 0.9995f`) + unlinked blend, and the Slice 14.7 total-GR metering.
- `Source/parameters/ParameterIDs.h` / `Parameters.cpp` — `stereo_link_pct`,
  `ms_link_pct` (exists), and the Choice-param pattern (`clipper_mode`,
  `character`).
- `Source/ui/MainView.{cpp,h}` — `sldMsLink_` (the dead M/S knob + its
  attachment ~326), the Stereo Link knob, and how `setEnabled`/greying is done
  elsewhere; layout of the link cluster.

Output the retrieval log.

────────────────────────────────────────
2. PARAMETER (new frozen ID)
────────────────────────────────────────

Add ONE new frozen `AudioParameterChoice`:
- **ID:** `stereo_mode`
- **Display name:** `"Stereo Mode"`
- **Choices:** `["Stereo", "M/S"]`
- **Default index:** `0` (Stereo)

Declare in `ParameterIDs.h`; construct in `Parameters.cpp` (mirror
`clipper_mode`). `ms_link_pct` already exists (frozen) — keep its range/default
(default **100%** = fully linked).

────────────────────────────────────────
3. DSP — domain branch in the wideband stage
────────────────────────────────────────

The bands stay stereo-linked (ADR-0009 §4 unchanged). Only the **wideband
final stage** gains a domain branch, selected by `stereo_mode`:

Generalise the wideband stage to two components **A/B**:
- **Stereo mode:** A = L, B = R; link = `stereo_link_pct`. (Exactly today's
  path — keep it bit-identical.)
- **M/S mode:** A = M, B = S; link = `ms_link_pct`. Encode the band-limited
  signal per sample: `M = 0.5*(L+R)`, `S = 0.5*(L-R)`; decode on output:
  `L = M+S`, `R = M-S` (lossless, phase-transparent).

Concretely:
- **Pass C (detect):** build the wideband peak streams from `bandLimitedBuf_`.
  In Stereo mode: `peakA=|L|`, `peakB=|R|` (as today). In M/S mode: encode each
  sample to M/S first, `peakA=|M|`, `peakB=|S|`. Run `envelope_` on A and
  `envelope_R_` on B (reused). Apply the **fast-path** (`link >= 0.9995f`) and
  the link blend using the **active** link param. (M/S fast-path at
  `ms_link_pct >= 99.95` → single envelope on `max(|M|,|S|)`.)
- **Pass D (apply):** take the `lookaheadWide_`-delayed band-limited sample.
  In Stereo mode: `outL = delayedL*gA`, `outR = delayedR*gB` (today). In M/S
  mode: encode delayed L/R → M/S, multiply `M*gA` and `S*gB`, then **decode**
  back to L/R for output.
- Keep a single code path that switches A/B meaning + the apply encode/decode
  by mode, so the Stereo branch stays exactly as shipped.

**GR metering:** the two GR bars show the two active components — L|R in Stereo
mode, **M|S in M/S mode**. Keep the Slice 14.7 total-GR computation
(`min(gLowOut,gHighOut) * gWide_component`); it now reports M/S reductions in
M/S mode. Optionally relabel the GR meter's L|R tags to M|S when in M/S mode
(nice-to-have; note if done).

────────────────────────────────────────
4. UI — mode toggle + activate the M/S knob
────────────────────────────────────────

- Add a **Stereo Mode** control (a compact 2-state toggle / segmented
  Stereo | M/S) bound to `stereo_mode`, placed in the link cluster near the
  Stereo Link and M/S Link knobs (architect/avishali audit placement; match
  the LookAndFeel).
- **Greying:** in **Stereo** mode, the Stereo Link knob is active and the
  **M/S Link knob is greyed/disabled**; in **M/S** mode, vice-versa. Wire the
  mode change to update `setEnabled` on the two knobs (and repaint).
- The M/S Link knob (`sldMsLink_`) keeps its existing `ms_link_pct` attachment —
  it is now functional in M/S mode.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout -b slice-7b-ms-link
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings.
- **Bench (default = Stereo, M/S 100%) must be bit-identical:** Slice 3/4/5
  quick PASS unchanged (the default never enters the M/S branch).
```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/SLICE7b_S$SLICE"; done
```
- Behavioural sanity (report, not gated):
  - M/S mode + `ms_link_pct = 100` ≈ Stereo-linked loudness (both components
    share the deeper gain) — no gross level/width jump vs Stereo mode at link
    100.
  - M/S mode + `ms_link_pct = 0` → Mid and Side limited independently (e.g. a
    loud centred kick reduces Mid without ducking the Sides).
  - Toggle the mode live with audio running → no clicks/zipper (smooth the link
    value if needed; `stereo_mode` switch should be click-safe — if a hard
    switch clicks, add a short crossfade or smooth the encode mix).

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Param diff (`stereo_mode`).
3. DSP diff (the A/B domain branch in Pass C/D; M/S encode/decode; fast-path +
   blend on the active link param; metering).
4. UI diff (mode toggle + knob greying).
5. Build summary (Debug + Release, warnings).
6. Slice 3/4/5 quick: PASS unchanged (default bit-identical).
7. Behavioural sanity notes (M/S 100 vs 0; live mode-switch click test).
8. `git status --short` (slice branch, no commit).
9. Open questions (mode-toggle placement; `stereo_mode` name before freeze;
   GR M|S relabel; any click on live mode switch).

────────────────────────────────────────
7. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

Architect provides the ADR-0008 addendum (M/S mode decision) on the sibling
M/S HQ branch. avishali auditions: M/S mode behaviour, knob greying, mode
switch click-safety, default unchanged. On approval → architect closes Slice 7b
(product + ADR addendum + submodule bump + PROGRESS/PLAN + prompt archive).
**Do not self-close, do not write the ADR.**
