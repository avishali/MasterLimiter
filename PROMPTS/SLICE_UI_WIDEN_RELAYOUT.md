# SLICE — Widen the window + respectful relayout (room for the L/R-per-band GR meter)

**Status:** ready for Cursor **after `SLICE_PERBAND_STEREO_UNLINK` (A2) closes** · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` only. UI-only — **no DSP, no params, no audio, no latency.** No SDK/submodule.
**Companion:** design language in `design/ui_direction_v1.html` (clean dark/teal).
**Arc position:** small UI follow-up to A2. A2 packs 6 L/R sub-bars into the current **88px** GR slot (intentionally tight). This slice widens the window and moves everything respectfully so the GR meter — and the whole right cluster — breathe.

---

## Why
The editor is a fixed **1100×580 design canvas** (`PluginEditor.h` `kDesignWidth/kDesignHeight`) scaled to the window by a locked aspect ratio. Every element is laid out in absolute design coords in `MainView::resized()`. The GR meter sits at `650,104,88,354` with the input meter starting at `790` — only ~52px of slack. 6 L/R sub-bars need ~150px, so we **add width to the canvas** and **shift the right cluster over**, preserving today's margins and relationships (no rescaling, no cramming).

---

## Allowed files to touch
```
Source/PluginEditor.h                 # kDesignWidth, kMinBaseWidth (+ derived), aspect ratio follows
Source/ui/MainView.cpp                # resized(): widen meterGr_, shift the right cluster
Source/ui/meters/GainReductionMeter.cpp   # OPTIONAL: sub-bar spacing polish at the larger width
docs/SIGNAL_FLOW.md (only if a dimension is documented)  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_UI_WIDEN_RELAYOUT_CLOSE.md   (new, at close)
```
**Non-goals / STOP:** no DSP/param/audio/latency/metering change; no aesthetic redesign (palette, fonts, control art stay); no height change (keep 580 / `kMinBaseHeight`); do not re-flow the **left/centre** controls (Gain, Ceiling, Color, Gain-Match, LUFS) — they keep their coords. The **Clipper knob is the one exception** — it shrinks to give the GR meter room (§2). This is width + clipper-shrink + GR-widen + right-cluster shift.

---

## 1. Canvas width (`PluginEditor.h`)
- `kDesignWidth 1100 → ~1170` (pick the exact value from §2 so the GR meter is comfortable and the right margin equals today's ~36px). Add the same delta `Δ = kDesignWidth_new − 1100`.
- `kMinBaseWidth`: keep the current scale factor — `kMinBaseWidth_new = round(kDesignWidth_new × 958 / 1100)` (≈ 1019 at 1170). Height constants unchanged.
- The constrainer's fixed aspect ratio is `kDesignWidth/kDesignHeight` — it updates automatically; no other change. The DEV dock (`kDevDockWidth = 360`) still adds on top of the new base; confirm DEV open/close math (`setSize(getWidth() ± kDevDockWidth, …)`) still lands correctly.

## 2. Relayout (`MainView::resized()`) — the GR meter gets room from BOTH sides
avishali's audition: the GR bars must be **noticeably wider**. Reclaim space on the **left** by shrinking the (oversized) Clipper knob, and on the **right** by the window widen — so the meter grows generously without shoving the whole layout.

- **Shrink the Clipper Drive knob (reclaim left space):** it is currently `sldClipperDrive_.setBounds(495, 134, 140, 120)` — a 140-wide rotary that is bigger than it needs to be. Scale it down to ~`100×100` (and re-centre its label `lblClipperDrive_` (495,116,140,18), mode button `btnClipperMode_` (515,260,100,22), readout `lblClipperReadout_` (495,286,140,18), and power `btnClipperActive_` (450,134,34,34) to the narrower stack). This frees ~40–50px on the right of the clipper column.
- **Widen the GR meter into the freed left space + move its left edge left:** e.g. `meterGr_.setBounds(~600, 104, W_gr, 354)` with **`W_gr ≈ 190–210`** — enough that each of the **3 band groups × 2 sub-bars** reads with clear separation, hairline dividers, and LO/MID/HI captions. Pick the left edge so it clears the shrunk clipper column with a tidy gap.
- **Shift the right cluster right by `Δ`** so nothing overlaps and margins are preserved. Everything with x ≥ 790 moves by `+Δ`:
  - `meterIn_` (790), `meterOut_` (948);
  - I/O input trim faders/readouts/link (`800/856/826…`), I/O output trims (`960/1014/984…`);
  - the meter **scale column** (`scaleX = meterIn_.getRight()+4`, its ±/RMS/label at `474/492/514`) — computed relative to `meterIn_`, so it follows automatically;
  - `btnResetPeaks_` (`884,538`).
  - Choose `Δ` so `meterGr_.getRight()` + the original ~52px gap places `meterIn_`; `Δ = meterGr_.getRight() + gap − 790`. Add the same `Δ` to `kDesignWidth`.
- **Rest of left/centre untouched.** `lufsPanel_` (508), `sldBandColor_` (526), Gain-Match footer keep their coords; just verify none collide with the moved GR-meter left edge.
- Net effect: clipper a touch smaller, GR meter substantially wider (both directions), right cluster slid over, window a bit wider. avishali auditions the balance.

## 3. GR meter spacing (`GainReductionMeter.cpp`, optional)
A2 draws the 6 sub-bars inside `meterBounds_`; at the new width they should space out naturally. Only touch this if the sub-bar/divider/caption spacing needs a tune for the larger slot — keep the A2 structure.

---

## 4. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
```
**Acceptance (Claude verifies 1–4; avishali auditions 5):**
1. Build clean; AU validates; **no param/latency change** (identical to the A2 build except window size).
2. Default window is wider; GR meter shows 6 L/R sub-bars with clear spacing + LO/MID/HI captions; **no element overlaps**; right margin matches the old look.
3. **Resize is stable** across the full range (min → large): aspect ratio holds, nothing clips or detaches, meters/faders track.
4. **DEV dock** still opens/closes correctly (editor grows by 360 on the new base, main view fully visible); History Graph window unaffected.
5. **Audition:** the wider layout reads clean and balanced (not just "shoved over"); GR meter is legible at default size.

**Close gate:** update `docs/PROGRESS.md`, `PROMPTS/PLAN.md` (and any documented dimension in `SIGNAL_FLOW.md`); commit plugin-only; archive CLOSE prompt.

---

## Notes for the architect (context, not for Cursor)
- The whole point is *insert room where the GR meter needs it* and slide the right cluster to match — relationships/margins preserved, so it reads as intentional, not stretched. If avishali wants more overall breathing room later, that's a separate polish pass.
- Slice B (3rd band) fills the reserved MID sub-bars; the width chosen here already accounts for all 3 bands × 2 channels, so B needs no further layout work.
