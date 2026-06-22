# SLICE — DEV panel: dock beside the editor (don't cover meters/controls)

**Status:** ready for Cursor · **Architect:** Claude · **Audition:** avishali
**Repo:** plugin `MasterLimiter` (UI only). Commit + push. Builds on `f6fcf49` (DEV embedded in editor).

## Problem
The DEV controls are now embedded in the editor (good — fixes AAX). But they open as a **centered overlay with a scrim that covers the meters and main controls**, so you can't watch the meters / GR while tuning. Tuning needs the meters + History Graph visible *and* the DEV controls at the same time.

## Goal
When DEV is toggled open, **the main view (controls + meters) stays fully visible** and the DEV controls appear in **added space**, not on top. Keep it AAX-safe: everything stays a child of the editor (no separate window).

## Approach (recommended)
**Grow the editor and dock the DEV panel in the new space** so nothing is covered:
- On DEV open: increase the editor size (e.g. **width +~360 px**, or height +~300 px — pick whichever reflows the grouped DEV sections cleanest) and place the DEV panel in the **new strip** (right side if width-grow, bottom if height-grow). The existing `mainView` keeps its original bounds/position → meters + controls fully visible.
- On DEV close: shrink the editor back to the base size.
- **Remove the dimming scrim** (it's what blocks visibility). The DEV panel itself is opaque and lives only in the added strip.
- Keep a title bar + close ✕ on the DEV panel; wrap `DevControlsComponent` in a `juce::Viewport` (vertical scroll) so all sections are reachable in the narrower/shorter dock.
- Update the `ResizableCornerComponent`/constrainer limits so the grown size is allowed, and `resized()` lays out `mainView` (base area) + DEV dock (added area) without overlap.

(If editor-resize proves flaky in any host, the fallback is to dock the DEV panel over the **left/control area only**, leaving the **meter strip on the right (x≥650) uncovered** — but the grow-the-editor approach is preferred since it covers nothing.)

## Allowed files
```
Source/PluginEditor.h / PluginEditor.cpp   (sizing, dock layout, remove scrim)
Source/ui/DevControlsComponent.{h,cpp}     (only if a Viewport / narrower reflow is needed)
docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_DEV_PANEL_DOCK_CLOSE.md
```

## Acceptance
1. Builds clean; AU validates.
2. Opening DEV **does not cover** the meters, GR readout, or main controls — they stay visible and live while the DEV panel is open.
3. DEV controls still drive their params in **VST3/AU and AAX** (no regression from the embed fix).
4. Editor grows on open / shrinks on close; resize stays stable; close ✕ works.
5. History Graph window unaffected.

Close gate: update `docs/SIGNAL_FLOW.md` §6 (DEV controls dock beside the editor, no scrim), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit + push; archive CLOSE prompt.

## Note — separate open bug (NOT this slice)
`dev_attack_ms` has no audible effect **in AAX** while it works in VST3/AU (other DEV controls now work in AAX after the embed fix). Being diagnosed separately (snap-back vs DSP-no-response). Do not change attack DSP here.
