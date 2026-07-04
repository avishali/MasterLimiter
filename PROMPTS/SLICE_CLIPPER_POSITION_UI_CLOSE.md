# SLICE_CLIPPER_POSITION_UI — Close record

**Slice:** Clipper Pre/Post UI segment button
**Date:** 2026-07-05
**Repos touched:** MasterLimiter only (UI)

## What shipped

1. **`btnClipperPosition_`** — Pre/Post segment beside Hard/Soft (50+50 px row at y=244).
2. **`ParameterAttachment`** — mirrors `btnClipperMode_`; click toggles; callback tracks automation/presets.
3. **Disabled with clipper off** — same as Hard/Soft via `updateClipperActiveState()`.

## Verification checklist

- [x] Build clean; AU validates; `install_user.sh build`
- [x] Installed VST3: `clipper_position` attribute present
- [x] Layout: Hard/Soft + Pre/Post row; readout/drive/active unchanged
- [ ] avishali audition: live Pre↔Post flip

## Files changed

- `Source/ui/MainView.{h,cpp}`
- `docs/SIGNAL_FLOW.md`
- `docs/PROGRESS.md`
- `PROMPTS/PLAN.md`
