# MasterLimiter — UI HQ-lift candidates

Items copied locally from AnalyzerPro for Slice 8.1. To be lifted into
`melechdsp-hq/shared/mdsp_ui/` in a future HQ-refactor slice so future
products inherit.

- `Source/ui/meters/MeterComponent.{h,cpp}` (with GR variant added here)
- `Source/ui/meters/MeterGroupComponent.{h,cpp}`
- `Source/ui/loudness/LoudnessNumericPanel.{h,cpp}`
- Per-meter `resetPeakHold()` on `mdsp_ui::meters::widgets::LevelMeter`
  (currently no public API; Slice 8 workaround was widget recreation,
  Slice 8.1 may inherit the workaround or use AnalyzerPro's local
  reset mechanism).
- True-peak measurement in the snapshot path (Slice 8 uses
  max-sample-peak approximation for `outputTpDb_`).
- GR ballistics tuning — `mdsp_ui::meters::GainReductionRenderStateProvider`
  is hard-coded at 10 ms / 150 ms; Q8-4 asked for 0 ms / 200 ms. Defer.

These are intentional debt items, not bugs. Each gets its own slice
when we have a second product needing them.

## Slice 8.1 editor sizing

- Default editor: **1100 × 620**, minimum **960 × 540**, meter strip **280 px** wide (within 260–300 px spec).
