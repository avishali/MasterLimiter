# SLICE UI-4 — DEV per-engine controls leak across engines; Ceiling button unlabelled + overlapping

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (build/install) · **Audition:** avishali
**Scope:** UI only — `DevControlsComponent.{h,cpp}`, `MainView.{h,cpp}`. **No DSP, no params, no SDK.**
**Queue after SMART-3P** (that slice is `tools/analysis/` only, so no file conflict).

---

## BUG 1 — Transparent's DEV panel is overdrawn with Open-engine controls

Reported by avishali with a screenshot: with `Engine: Transparent` selected, the panel shows
`Rel Engine`, `MB Safety (TP)`, `Fast (ms) 40.0`, `Slow (ms) 300`, `Sustain (ms) 450`, `Leak 0.15`
drawn **on top of** the Transparent controls — labels collide (`CrosModerr`, `Attack (ms)` over
`Attack x`, `Release` over `Low Atk x`, `Lookahead` over `High Atk x`).

### Root cause (verified in `DevControlsComponent.cpp`)

`updateEngineFrameVisibility()` (~L715) hides the **`GroupComponent` frames only**:

```cpp
groupMbEngine_.setVisible (openEngine);
groupSmartRelease_.setVisible (openSmart);
```

But a JUCE `GroupComponent` is **just a border** — the labels/sliders/combos inside it are *siblings*
added to `content_`, not children of the group. Hiding the group hides the border and nothing else.

Meanwhile `resized()` branches the layout:

```cpp
if (! openEngine) { ...place Transparent groups... }
if (openEngine)   { ...place Open groups...      }
```

and `placeGroupIfVisible()` (~L450) returns an empty rectangle when the group is hidden, so the
whole placement block is skipped. **The Open controls are therefore never repositioned and never
hidden — they keep their stale bounds from the last time Open was selected and float over the
Transparent layout.** Same in reverse for Transparent's controls when Open is selected.

### Fix
Hide the **controls**, not just the frames. Preferred: give every DEV control an explicit
group association once (e.g. a `std::vector<std::pair<juce::Component*, juce::GroupComponent*>>`
built at construction), then in `updateEngineFrameVisibility()` set each control's visibility from
its group's visibility. A hard-coded list of `setVisible` calls is acceptable if it is exhaustive —
but the association approach cannot rot when the next slice adds a control, which is how this bug
arrived.

**Do not** fix it by positioning hidden controls off-screen.

## BUG 2 — an unlabelled power button that defeats peak safety

avishali: *"there is a power button near the TP/SP that i don't know what it does."*

It is **`btnCeilingStageActive_`** → param `dev_final_ceiling`, display name **"Ceiling Active"**.
Tooltip (exists, but nothing is visible without hovering):
> *"Peak-safety Ceiling on/off. Off lets peaks exceed the output level - audition only."*

Two problems:

**2a. It overlaps the SP/TP segment.** `MainView.cpp`:
```
btnCeilingMode_        .setBounds (206, 194, 86, 22);   // spans x 206-292, y 194-216
btnCeilingStageActive_ .setBounds (250, 210, 34, 34);   // spans x 250-284, y 210-244
```
→ a **6 px overlap** in x 250-284, y 210-216. That is why it reads as part of the SP/TP control.

**2b. It is unlabelled.** This is the switch that **turns off peak safety** on a mastering limiter —
with it off the plugin will exceed its ceiling. An unlabelled, unexplained control that can break the
product's core guarantee is the most dangerous thing on the surface.

### Fix
- Separate the two so they do not touch (give the Ceiling power button its own row or clear gap).
- Add a visible **"Ceiling"** label next to it, in the same style as the other labelled clusters.
- Consider a warning affordance when it is OFF (e.g. the Ceiling readout or the button tinted
  differently) — the state is currently invisible at a glance. Propose, do not implement without asking.
- Keep the existing tooltip.

> ⚠️ **ASCII-GATE**: all UI literals stay ASCII — the build fails on non-ASCII (`a1d283f`).

## Gate
- [ ] `Engine: Transparent` shows **only** Transparent controls; `Engine: Open` shows **only** Open
      controls. Switch back and forth repeatedly — no stale controls, no overlapping labels.
- [ ] With Open + `Rel Engine: Smart`, the four Smart knobs appear; with Manual/Lookahead they do not.
- [ ] `MB Release` greys out when the release engine is not Manual (existing behaviour preserved).
- [ ] Ceiling power button no longer overlaps SP/TP and carries a visible label.
- [ ] Screenshots of: Transparent panel, Open+Manual panel, Open+Smart panel, and the main-window
      ceiling cluster.
- [ ] `mbl_calibrate.py` unchanged at 54/55 (UI-only change must not move a single measurement).
- [ ] Build clean (ASCII gate passes), AU + VST3, **both installed**, mtimes for both.

## Non-goals
- No parameter changes, no renames, no DSP, no SDK.
- Do not restyle the DEV panel generally — fix these two defects only.

## Output requirements
1. Retrieval log. 2. Diffs. 3. The four screenshots. 4. `mbl_calibrate.py` summary line.
5. Build + install mtimes for BOTH formats. 6. Open questions.
