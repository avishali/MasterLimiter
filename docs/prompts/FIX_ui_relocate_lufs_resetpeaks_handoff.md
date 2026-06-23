# Cursor handoff — fix clipped LUFS panel + Reset Peaks after the vertical trim

**Repo:** `MasterLimiter`, `main` @ `9000f54`. UI-layout only (`MainView.cpp` `resized()`), no DSP.
Build DEV-ON, user-install + verify (post-stale-binary protocol). Iterate visually with avishali.

## Bug
The `kDesignHeight 620→580` trim (`9000f54`) moved the gain-match row up −40 px but **left the
LUFS panel and Reset Peaks behind**, so they now hang below the 580 design floor and clip:
- `lufsPanel_.setBounds(790, 536, 160, 68)` → bottom y=**604** (> 580) — clipped.
- `btnResetPeaks_.setBounds(962, 556, 100, 26)` → bottom y=**582** (> 580) — clipped.

## avishali's relocation plan (target layout)

### A. LUFS panel → into the LEFT panel, right of the gain-match row
- Left panel = `maximizerPanelArea_ {16,64,736,500}` (x 16–752). The GR meter `meterGr_ {650,104,88,354}`
  ends at y≈458, so the bottom strip (y≈520–558) under it is free.
- The gain-match row currently spans x **152–620** at y=528
  (`btnGainMatchAutoTrack_ 152,528` · `lblGainMatchNote_ 288,528` · `compGainBar_ 372,540` ·
  `btnLearnInputGain_ 432,528` · `lblLearnInputLufs_ 524,528`).
- **Shift the whole gain-match row LEFT** (≈ −128 px, e.g. start ~x24) so it ends ~x492, then place
  **`lufsPanel_` to its right** in the freed space, e.g. `~ (508, 525, 160, 60)` — under the GR meter,
  baseline-aligned with the gain-match row, fully inside the left panel (right edge < 752) and above
  the 580 floor. Keep it the same panel-relative size (shrink height slightly if needed to clear 580).

### B. Reset Peaks → smaller, centered at the bottom-middle of the meters (right) panel
- Right/meters panel = `ioPanelArea_ {768,64,316,500}` (x 768–1084, center x≈926); In/Out meters
  (`meterIn_ 790,112,116,358` · `meterOut_ 948,112,116,358`) end at y≈470, panel bottom 564.
- **Shrink** `btnResetPeaks_` (e.g. 100×26 → ~84×22) and **center it horizontally** in that panel at
  the bottom strip, e.g. `~ (884, 520, 84, 22)` — below the meters, above the 580 floor. "Middle bottom
  of the meters panel."

## Constraints
- Nothing may exceed y=580 (the design floor) or overlap the GR/In/Out meters or the gain-match row.
- These are starting coordinates — **build and iterate with avishali** on exact px (he'll eyeball it).
- Don't touch DSP, defaults, or the preset work from `9000f54`.

## Report back
- New bounds for the gain-match row, `lufsPanel_`, `btnResetPeaks_`; confirm nothing clips at the
  default `958×505` window and at a larger size (scaling holds).
- DEV-ON build, user-installed, header `main@<HEAD>`, auval PASS.
- Commit: `fix: relocate LUFS panel + Reset Peaks into compact layout (no clip)`.
- Don't push (folds into the AAX beta after avishali approves).

## avishali audition
At the default small size: LUFS panel reads cleanly to the right of the gain-match section; Reset
Peaks sits small and centered under the meters; nothing clipped at min size or when enlarged.
