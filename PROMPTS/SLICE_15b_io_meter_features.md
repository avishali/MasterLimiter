# Cursor Task — Slice 15b: I/O meter features (RMS bar + range +/- + peak reset + shared centre scale)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only slice. Read-only metering + UI.** Adds Ozone-style features to
> the Input/Output level meters: (1) RMS bar fill + numeric RMS readout
> (engine currently publishes peak only — add RMS), (2) shared +/- buttons that
> step the meter display range through the existing ScaleMode presets, (3)
> click-to-reset the peak line (largely already wired — verify/finish), (4)
> move the dB value lines/labels OUT of the bars into a single shared scale
> **between** the In and Out meters, so the bars read clean. No audio-path
> change, no params, no ADR. Sequenced AFTER Slice 15 (both edit MainView).
> Branch off product `main`. **Do NOT push** — architect closes.

> **Ordering:** assumes Slice 15 (meter ballistics) has closed and is on
> `main`. If Slice 15 is still open, STOP and say so (to avoid MainView
> conflicts).

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product edits only: `Source/PluginProcessor.{h,cpp}` (RMS measurement +
  atomics + getters), `Source/ui/meters/MeterComponent.{cpp,h}`,
  `Source/ui/meters/MeterGroupComponent.{cpp,h}`, `Source/ui/MainView.{cpp,h}`.
- **No `mdsp_ui` / `mdsp_dsp` edits** — the HQ provider/render-state already
  support RMS and ScaleMode (`updateFromValues(peak, rms, …)`,
  `setScaleMode`). No params, no frozen IDs, no ADR.
- RMS is **measurement only** — it reads the buffers and stores atomics; it
  must NOT alter any audio sample. RT-safe (per-block compute, atomic stores,
  no allocation/logging/locks on the audio thread).
- Branch `slice-15b-io-meter-features` off `main`. **Do NOT commit, do NOT
  push.**

────────────────────────────────────────
1. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `Source/PluginProcessor.{h,cpp}` — the input peak measurement (~line
  512–513, `inputPeakLDb_/RDb_` from `pL/pR`) and output peak (~801–803,
  `outputPeakLDb_/RDb_`, `outputTpDb_`); `prepareToPlay`; existing peak getters
  (~70–74).
- `Source/ui/meters/MeterGroupComponent.{cpp,h}` — `pushLevelRenderStates`,
  `provider0_/provider1_.updateFromValues(lPeak, lPeak, …)` (peak passed as
  rms today), `setScaleMode`/`setDisplayMode` usage (~134–137,
  hardcoded `Top24Db` + `Peak`), `PeakNumericSmoother`, `handlePeakReset`,
  `resetPeakHolds`, the peak-reset wiring (`setPeakResetCallback`).
- `Source/ui/meters/MeterComponent.{cpp,h}` — `paintLevel`, `setRangeDb`,
  `numericTextPeak_`/`numericTextRms_`, the render-state fields it draws
  (`peakNorm`, `rmsNorm`, `maxPeakNorm`).
- HQ `mdsp_ui/meters/MeterRenderState.h` (FullRange/Top24/Top12/Top6;
  peakNorm + rmsNorm both available) and `MeterRenderStateProvider.h`
  (`updateFromValues`, `setScaleMode`, `scaleMinDb/scaleMaxDb`) — read-only,
  do not edit.
- `Source/ui/MainView.{cpp,h}` — where `meterIn_`/`meterOut_` are laid out and
  synced.

Output the retrieval log.

────────────────────────────────────────
2. ENGINE — publish RMS (input + output, per channel)
────────────────────────────────────────

- Add atomics + getters mirroring the peak ones: `inputRmsLDb_`,
  `inputRmsRDb_`, `outputRmsLDb_`, `outputRmsRDb_` (init −100.0f) +
  `getInputRmsLDb()` etc.
- Add smoothed mean-square state members per channel for input and output
  (e.g. `msInL_`, `msInR_`, `msOutL_`, `msOutR_`), and a coefficient.
- In `prepareToPlay`: reset the ms state to 0; compute a one-pole coefficient
  for a **~300 ms** RMS window at the host sample rate.
- At the **input** measurement point (where `pL/pR` peak is taken, ~512):
  compute the block mean-square per channel, one-pole-smooth `msInL_/msInR_`
  toward it, then store `inputRmsLDb_ = gainToDecibels(sqrt(msInL_), -100.0f)`
  (same for R). Do the same at the **output** point (~801) into
  `outputRmsL/RDb_`. Measurement only — do not modify the buffer.

(Block-rate one-pole on mean-square is sufficient for a level meter; keep it
simple and allocation-free.)

────────────────────────────────────────
3. UI — RMS bar fill + numeric
────────────────────────────────────────

- `MeterGroupComponent::pushLevelRenderStates`: feed **real RMS** —
  `provider0_.updateFromValues(lPeak, lRms, …)` /
  `provider1_.updateFromValues(rPeak, rRms, …)` using the new getters (was
  `lPeak, lPeak`). Add an RMS numeric smoother (mirror `PeakNumericSmoother`,
  or reuse the Slice 15 `MeterBallistic`) so the RMS number is steady.
- `MeterComponent::paintLevel`: draw **both** — an RMS **fill** up to
  `rmsNorm` and a **peak cap/outline** at `peakNorm` (Ozone style), instead of
  a single peak bar. Use the theme; RMS fill slightly more saturated/solid,
  peak as a thin cap line + the existing peak-hold marker. Keep the existing
  scale/range mapping.
- Numeric area shows **both** peak and RMS (e.g. peak number on top, RMS below,
  using `numericTextPeak_` + `numericTextRms_` which already exist).

────────────────────────────────────────
4. UI — shared range +/- buttons
────────────────────────────────────────

- Add `void setScaleMode (mdsp_ui::meters::MeterScaleMode)` to
  `MeterGroupComponent` (sets `provider0_/provider1_.setScaleMode`, updates the
  child `MeterComponent` range via `scaleMinDb/scaleMaxDb` if needed, repaints).
- In `MainView`: hold a shared `MeterScaleMode currentScale_` (default
  `FullRange`). Add two small **+ / −** buttons (one shared pair, Ozone-style),
  placed near the I/O meters (architect/avishali audit exact placement; match
  the clean/dark/teal LookAndFeel). `+` zooms in
  (`FullRange → Top24 → Top12 → Top6`), `−` zooms out; clamp at the ends and
  disable/grey the button at each limit. On change, call
  `meterIn_.setScaleMode(currentScale_)` **and** `meterOut_.setScaleMode(...)`
  so both stay in lockstep. Optionally show the current range as a tiny label
  (e.g. "Full" / "24" / "12" / "6 dB").
- Persistence is optional/nice-to-have; if there's an existing UI-state hook
  use it, otherwise default to `FullRange` each session (note which you did).

────────────────────────────────────────
5. UI — click-to-reset peak line (verify/finish)
────────────────────────────────────────

The peak-reset path is already wired (`setPeakResetCallback` → `mouseDown` →
`handlePeakReset` → `resetPeakHold`). Verify clicking an I/O meter resets the
**peak-hold line/marker** (and the latched `maxPeakL/RDb_`). Extend the reset
to also clear the RMS max (`maxRmsDb`) if shown. Confirm it works per bus.

────────────────────────────────────────
5b. SHARED CENTRE dB SCALE (value lines between the meters)
────────────────────────────────────────

Currently each `MeterComponent::paintLevel` draws its own dB scale lines +
labels *inside* the bar (the `-6/-12/-24` labels and gridlines, ~381–418, per
`scaleMode`). Move them into ONE shared scale column between the two meters:

- **MeterComponent:** add `void setDrawInternalScale (bool) noexcept;` (default
  true for back-compat). When **false**, `paintLevel` skips the internal dB
  scale lines/labels entirely — drawing only the bar, RMS fill, peak cap, and
  peak-hold marker (clean bar). Set the I/O `MeterComponent`s to `false`.
- **Centre scale:** draw a single vertical dB scale (tick lines + numeric
  labels: 0 / −6 / −12 / −24 etc. per the current `scaleMode`) in the gap
  **between** `meterIn_` and `meterOut_`. Use the **same** normalised mapping
  the bars use (`dbToNormForScale` / `scaleMin/MaxDb`) so the ticks line up
  exactly with both bars. Implement as either a small dedicated component
  (e.g. `MeterScaleColumn`) placed in the gap, or a paint routine in MainView —
  whichever is cleaner. (The HQ `mdsp_ui/ScaleLabelRenderer.h` exists and may
  be reused read-only, but a simple product-side draw is fine.)
- **Tracks the range:** the centre scale must relabel/redraw whenever the +/-
  range (§4) changes `scaleMode`, staying in sync with both bars.
- **Layout (MainView):** the present gap (In ends x≈906, Out starts x=934 ≈
  28 px) is likely too narrow for labels like "−12". Widen the centre column
  (and/or nudge the meters) so the scale fits and is centred between them; keep
  the overall meter strip tidy and resize-clean. avishali audits exact spacing.

Net effect: two clean meter bars flanking one shared, readable dB scale.

────────────────────────────────────────
6. NON-GOALS
────────────────────────────────────────

- Do NOT change the GR or Clip meters (Slice 15 owns those).
- Do NOT add params / frozen IDs / ADR. The range setting is a UI display
  preference, not an audio parameter.
- Do NOT edit `mdsp_ui` / `mdsp_dsp`.
- No audio-path change (RMS is read-only measurement).

────────────────────────────────────────
7. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout -b slice-15b-io-meter-features
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings.
- No audio change → Slice 3/4/5 quick should be **unchanged** (run as sanity):
  RMS measurement must not alter output.
- In a host: I/O meters show RMS fill + peak cap + numeric peak/RMS; +/-
  buttons zoom both meters together through Full/24/12/6 and clamp at ends;
  clicking a meter resets its peak line + maxes. RMS reads sensibly (a steady
  tone reads ~3 dB below its peak for a sine; pink noise lower). The dB value
  lines/labels now sit in a single shared scale **between** the two bars (bars
  are clean, no internal labels); ticks align with both bars and relabel when
  the +/- range changes.

────────────────────────────────────────
8. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Engine diff (RMS atomics/getters/compute), confirming audio untouched.
3. UI diffs (paintLevel RMS fill + cap; RMS numeric; +/- buttons + shared
   scale; peak-reset verification; `setDrawInternalScale` + the shared centre
   dB scale between the meters + the layout change).
4. Build summary (Debug + Release, warnings).
5. Slice 3/4/5 quick: unchanged (proves no audio impact).
6. Note on RMS window (300 ms) and whether range persists.
7. `git status --short` (slice branch, no commit).
8. Open questions (placement of +/- buttons, RMS bar styling).

────────────────────────────────────────
9. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali eyeballs the I/O meters in the host (RMS fill, +/- range, peak
reset). On approval → architect closes Slice 15b (commit + push +
PROGRESS/PLAN) and proceeds to Slice 16 (UI/UX interaction). **Do not
self-close.**
