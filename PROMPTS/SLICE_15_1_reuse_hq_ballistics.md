# Cursor Task — Slice 15.1: reuse HQ MeterBallistics/PeakHoldModel (drop the product-local duplicate)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Reviewer-directed refactor on the existing `slice-15-meter-ballistics`
> branch (uncommitted).** Slice 15 added a product-local
> `Source/ui/meters/MeterBallistics.h` per the prompt — but your REUSE CHECK
> correctly flagged that `mdsp_ui::meters::MeterBallistics` +
> `mdsp_ui::meters::PeakHoldModel` already exist in HQ and are already linked
> (the product uses `mdsp_ui` MeterRenderStateProvider). The architect is
> reversing the earlier "product-local" instruction: **use the HQ components,
> delete the product-local one.** Behaviour-equivalent (or better). No commit,
> no push, no HQ source change.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product UI edits only: `Source/ui/meters/GainReductionMeter.{cpp,h}`,
  `Source/ui/MainView.{cpp,h}`, `CMakeLists.txt`; delete
  `Source/ui/meters/MeterBallistics.h`.
- **No `mdsp_ui` / `mdsp_dsp` source edits** — only *consume* the existing HQ
  headers (`<mdsp_ui/meters/MeterBallistics.h>`,
  `<mdsp_ui/meters/PeakHoldModel.h>`, `<mdsp_ui/meters/MeterTypes.h>`).
- UI-thread display logic only; no audio/param/ADR change.
- Stay on branch `slice-15-meter-ballistics`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. SWAP
────────────────────────────────────────

Replace the product-local `MeterBallistic` usages with the HQ pair:

- `mdsp_ui::meters::MeterBallistics` — asymmetric attack/release smoother
  (`process(target, dtSec, config)`), for the live ballistic value.
- `mdsp_ui::meters::PeakHoldModel` — peak-hold with hold + dB/s falloff
  (`process(inputDb, dtSec, config)` / `getHeldDb()`), for the peak markers /
  LED hold.
- `mdsp_ui::meters::MeterBallisticsConfig` fields: `attackMs`, `releaseMs`,
  `holdMs`, `holdFalloffDbPerSec`, `clipHoldMs`.

### GainReductionMeter
- Two `MeterBallistics` (L, R) for the displayed GR values; two `PeakHoldModel`
  (L, R) for the per-channel peak-hold markers.
- Config (the Slice 15 feel): `attackMs = 1.0` (≈ instant attack at 30 Hz),
  `releaseMs = 300.0`, `holdMs = 1000.0`, `holdFalloffDbPerSec = 12.0`
  (≈ 1 s hold then a graceful fall — tunable later by the architect).
- `displayGrLDb_ = ballL_.process(rawL, dt, cfg)` etc.; peak marker level =
  `peakHoldL_.process(displayGrLDb_, dt, cfg)`.
- Reset paths (`resetPeakHolds`, readout `mouseDown`) call `.reset(0.0f)` on
  both ballistics and both peak-hold models.

### Clip (MainView)
- One `MeterBallistics` for the smoothed clip readout value (`attackMs ≈ 1`,
  `releaseMs ≈ 300`).
- Clip LED hold/fade via a `PeakHoldModel` (use `clipHoldMs`/`holdMs` for the
  hold, `holdFalloffDbPerSec` for the fade), or drive the 0..1 LED alpha from
  its held value. Keep the "light instantly, hold ~1 s, then fade" behaviour.
- Clip reset paths reset these too.

### Cleanup
- **Delete** `Source/ui/meters/MeterBallistics.h` and remove it from
  `CMakeLists.txt` `target_sources`.
- Remove the now-unused include.

────────────────────────────────────────
2. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings. If a link error appears for the HQ meter
  classes, confirm the `mdsp_ui` meters sources (`MeterBallistics.cpp`,
  `PeakHoldModel.cpp`) are part of the consumed `mdsp_ui` target — but they
  should already be linked (MeterRenderStateProvider is).
- Behaviour matches Slice 15: GR rises instantly, releases ~300 ms, peak marker
  holds ~1 s then falls; clip number steady, LED holds then fades. (Decay is
  now linear dB/s rather than exponential — standard peak-meter behaviour.)

────────────────────────────────────────
3. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Diffs: GainReductionMeter + MainView swapped to HQ components; deleted
   product-local header; CMakeLists update.
3. Build summary (Debug + Release, warnings).
4. Confirmation the product-local `MeterBallistics.h` is gone and the HQ
   classes link.
5. `git status --short` (branch `slice-15-meter-ballistics`, no commit).
6. Open questions.

────────────────────────────────────────
4. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali auditions the meters (same feel as Slice 15). On approval → architect
closes Slice 15 (now using HQ ballistics) and proceeds to Slice 15b, which will
reuse the same HQ components for the I/O meters. **Do not self-close.**
