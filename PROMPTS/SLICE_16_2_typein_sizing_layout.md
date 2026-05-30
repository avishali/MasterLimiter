# Cursor Task — Slice 16.2: type-in double-click, button sizing, layout nudges

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **UI refinement on the uncommitted Slice 16 branch** (`slice-16-ui-interaction`).
> From avishali's screenshot audit: (1) type-in opens on **double-click**, exits
> on Enter or click-away; (2) the oversized mode buttons (auto-release
> "Transparent", stereo "Stereo") shrink to fit; (3) move the Imaging
> (Stereo-Mode) button **left** so it doesn't cover Color; (4) Gain-Match
> section moves **down + centered** in the panel; (5) fix the `100%%`
> double-percent on the Link readout. No DSP/param change. **Do NOT commit.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Product UI only: `Source/ui/MainView.{cpp,h}`, `Source/ui/
  MasterLimiterLookAndFeel.{cpp,h}` if needed. No params, no DSP, no HQ.
- Continue branch `slice-16-ui-interaction`. **Do NOT commit, do NOT push.**
- Control map (from 16.1): Stereo-Mode `btnStereoMode_`, auto-release mode
  `btnAutoReleaseMode_`-equivalent (the "Transparent" button), Color
  `sldBandColor_`, Link `sldStereoLink_`, Gain-Match `btnGainMatchAutoTrack_`,
  the GAIN MATCH cluster (Learn, ref readout, comp slider/readout).

────────────────────────────────────────
1. TYPE-IN — double-click to open, Enter / click-away to exit
────────────────────────────────────────

Adjust the Ozone-style value editor (from 16.1):
- **Open the inline editor on DOUBLE-click** of the value/knob centre (not
  single-click — single click/drag stays as normal knob control).
- **Commit + close** on **Enter** OR on **click elsewhere / focus loss**.
- **Esc** cancels (revert to prior value).
- Keep unit parsing + range clamp.

────────────────────────────────────────
2. SHRINK OVERSIZED MODE BUTTONS
────────────────────────────────────────

The auto-release mode button ("Transparent") and the Stereo-Mode button
("Stereo") are too large for their clusters. Reduce them to a compact size
consistent with the other small buttons (e.g. match the Clipper Hard/Soft
toggle / the Lim-On/Link/SP button footprint), and re-fit them in their
clusters. Keep text legible. Apply consistent sizing to the mode/selector
buttons so the layout reads tidy.

────────────────────────────────────────
3. MOVE IMAGING (STEREO-MODE) BUTTON LEFT
────────────────────────────────────────

`btnStereoMode_` currently sits where it crowds/covers the **Color** section.
Move it **left** within its cluster so it clears Color entirely (Color +
Link knobs must be fully visible and unobstructed).

────────────────────────────────────────
4. GAIN MATCH — DOWN + CENTERED
────────────────────────────────────────

Move the whole **GAIN MATCH** cluster (label + Auto/Track + comp readout +
slider + Learn + ref readout) **down** and **center it horizontally within the
Maximizer panel**, so it reads as a balanced footer row rather than
bottom-left-aligned.

────────────────────────────────────────
5. FIX `100%%` DOUBLE-PERCENT
────────────────────────────────────────

The Link knob value reads `100%%` — the percent suffix is applied twice (the
formatter adds `%` and the drawn label adds another). Fix so it reads `100%`.
Check the other `%` controls (Color, Link) for the same.

────────────────────────────────────────
6. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j && cmake --build build-release -j
```
- Zero new `Source/` warnings. UI-only → Slice 3/4/5 unchanged.
- In host: double-click opens editor / Enter or click-away commits / Esc
  cancels; mode buttons are compact; Imaging button clears Color; Gain-Match
  centered footer; Link reads `100%` not `100%%`.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log. 2. Diffs per item. 3. Build summary. 4. `git status --short`.
5. Open questions (placement specifics for avishali).

> Button *look* + palette restyle remain Slice 16b (separate visual pass). This
> slice is sizing/position/interaction only.
