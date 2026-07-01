# SLICE_UI_WIDEN_RELAYOUT — Close record

**Slice:** Widen window + respectful relayout for L/R-per-band GR meter
**Date:** 2026-07-01
**Repos touched:** MasterLimiter only (UI-only)

## What shipped

1. **Canvas** — `kDesignWidth` 1100 → **1172**; `kMinBaseWidth` 958 → **1020** (same scale factor).
2. **Clipper knob** — shrunk from 140×120 to **100×100**; labels/mode/readout re-centred.
3. **GR meter** — **612, 104, 198×354** (was 650, 104, 88×354); ~2.25× wider.
4. **Right cluster** — shifted **+72 px** (meters, I/O trims, header buttons x≥838, Reset Peaks, I/O panel).
5. **GR meter paint** — wider band/sub-bar gaps + L/R hairline dividers.

## Verification checklist

- [ ] Build clean; AU validates; no param/latency change
- [ ] GR meter legible at default size; no overlaps
- [ ] Resize stable (min → large); aspect ratio holds
- [ ] DEV dock open/close correct on new base width
- [ ] avishali audition approved

## Files changed

- `Source/PluginEditor.h`
- `Source/ui/MainView.cpp`
- `Source/ui/meters/GainReductionMeter.cpp`
- `docs/PROGRESS.md`
- `PROMPTS/PLAN.md`
