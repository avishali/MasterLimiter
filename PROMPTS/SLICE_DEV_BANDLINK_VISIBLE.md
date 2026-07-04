# SLICE: DEV Band-Link visibility + disambiguate the two "Band Link" sliders

**Status:** closed 2026-07-04

**Scope:** DEV panel UI only (`Source/ui/DevControlsComponent.{h,cpp}`). No DSP, no param IDs, no param defaults, no APVTS changes. Label/layout/tooltip only.

## Problem (verified in code)

There are **two DEV sliders both labeled "Band Link"**, and one of them is **invisible**:

1. `sldBandLink_` / `lblBandLink_` ("Band Link") → attached to `param::band_color`. This is the **multiband band-to-band link** (0 = 3 bands glued to shared min GR, 100 = independent 3-band). It is placed as the **7th row of the Crossover group** (`placeGroup(groupCrossover_, 248)`).
   - The Crossover group inner height is `248 − 44 = 204` px, but 7 rows need `7×28 + 6×8 = 244` px. The 7th row (`lblBandLink_`/`sldBandLink_`) gets a **0-height rect and never renders.** This is the user-reported "Band Link is not visible."
2. `sldBandStereoLink_` / `lblBandStereoLink_` (also "Band Link") → attached to `param::dev_band_stereo_link_pct`. This is the **L/R stereo link within bands**, in group `groupBandStereo_` ("BAND · Stereo link"). This one IS visible, so the user only sees a stereo link and concludes the multiband link doesn't exist.

## Required changes

### A. Rename the two labels + tooltips (kill the duplicate name)

- `band_color` slider: label `lblBandLink_` → **"Band Split"**. Keep/clarify tooltip: `"Multiband band-to-band link. 0 = bands glued (single shared GR), 100 = fully independent 3-band. (Main Color knob is greyed; this is the live control.)"`
- `dev_band_stereo_link_pct` slider: label `lblBandStereoLink_` → **"Band Stereo"**. Tooltip: `"Per-band L/R stereo link. 0 = independent L/R per band, 100 = mono-linked GR per band."`

### B. Move the "Band Split" (band_color) slider OUT of the Crossover group into its own group

- Remove the `lblBandLink_`/`sldBandLink_` row from the Crossover group layout (the `placeSliderRow(inner..., lblBandLink_, sldBandLink_)` line after the Hi-Atten row, and its preceding `inner.removeFromTop(8)`).
- Add a new `juce::GroupComponent groupMultiband_ { "MultibandGroup", "BAND · Multiband link" };` in the header, next to `groupBandStereo_`.
- Register `groupMultiband_` wherever the other group components are `addAndMakeVisible`'d (the group-pointer list near the top of the constructor).
- In `resized()`, place `groupMultiband_` as a single-row group (`placeGroup(groupMultiband_, 72)`) with one `placeSliderRow(..., lblBandLink_, sldBandLink_)`. Put it **immediately after `groupBandScaling_` (per-band trim) and before `groupBandStereo_`** so the three band groups sit together: per-band trim → multiband link → band stereo.
- The Crossover group now has 6 rows; its `248` height already fits 6 rows, leave it unchanged.

(`lblBandLink_` stays the same C++ member — only its display text and its parent group change. It's already in the `for (auto* label : {...})` addAndMakeVisible loop, so no change needed there.)

## Must NOT change
- `param::band_color` default (stays 0.0) — the "should multiband be on by default for the alpha" question is a **separate** product decision, not this slice.
- Any DSP, `mapBandColorToLink`, or the greyed main-window Color knob.

## Verify before close
- Build clean, auval PASS, install via `install_user.sh`.
- Open DEV panel: confirm **"Band Split"** is now visible as its own row (its own group), separate from **"Band Stereo"**. No two controls share a name.
- Drag Band Split 0 → 100 and confirm the three per-band GR bars separate (they collapse to one at 0).
- Confirm nothing below the moved control is now clipped (Release/Peak/Manual groups still fully visible; scroll height still correct via `content_.setSize`).

## Close gate
Update `docs/SIGNAL_FLOW.md` §DEV-controls, `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; archive as `PROMPTS/SLICE_DEV_BANDLINK_VISIBLE_CLOSE.md`; commit `fix(dev): make multiband Band-Split control visible; disambiguate from Band-Stereo`.
