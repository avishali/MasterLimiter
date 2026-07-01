# SLICE — Per-band GR visualization (band-mode gain-reduction bars + history traces)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` ONLY. **No SDK change.** No submodule bump.
**Companion:** `docs/SIGNAL_FLOW.md` §2.14, §7.
**Arc:** This is **Slice A of the 3-band arc**. It is metering-only (no audio-path change) and is intentionally built **N=3-band-ready now** so the ring/atomic format is frozen once, before the 3rd band lands in Slice B (`SLICE_3BAND_DSP.md`).

---

## Why
The 2-band multiband stage already computes per-band gain (`gLowOut[i]`, `gHighOut[i]`) every oversampled sample, but the meters only ever see the **collapsed total** GR (`min(gLowOut,gHighOut) × wideGain`). You cannot see what each band is doing — which is exactly the instrument we need to voice the multiband stage toward Pro-L 2 / Ozone and to audition the incoming 3rd band.

This slice taps per-band GR and shows it two ways, Ozone-/Pro-L-style:
1. **Per-band GR bars** in the existing GR-meter footprint — **Low / (Mid reserved) / High**.
2. **Per-band GR traces** in the History Graph, in distinct colours.

> **Deliberate UX change (flag at audition):** the GR meter currently shows **L/R channel** GR. Per your decision it becomes **per-band** GR (Low/Mid/High). The stereo split moves to numeric-only. The total/max numeric readout stays. If you miss the L/R view at audition we can add it back as a mode.

---

## Allowed files to touch
```
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/ui/meters/GainReductionMeter.h
Source/ui/meters/GainReductionMeter.cpp
Source/ui/HistoryGraphComponent.h
Source/ui/HistoryGraphComponent.cpp
Source/ui/MainView.cpp            # only meterGr_.sync path if signature changes; layout unchanged
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_BAND_GR_VIZ_CLOSE.md   (new, at close)
```
**Non-goals / STOP if scope expands:** no DSP/audio-path change; no new APVTS params; no parameter ID/range change; no SDK edit; no latency change; no change to `meterGr_.setBounds(...)` in MainView (same 88×354 footprint at 650,104).

---

## 1. Processor — per-band GR taps (N=3, mid=0 until Slice B)

### 1a. New atomics + getters (`PluginProcessor.h`, next to the existing `currentGrDb_` block ~L289–294)
```cpp
// Per-band GR (multiband stage), published from the GR loop. Mid is 0 until the
// 3-band slice lands. These are band-level (already stereo-maxed), not per-channel.
std::atomic<float> currentGrLowDb_  { 0.0f };
std::atomic<float> currentGrMidDb_  { 0.0f };
std::atomic<float> currentGrHighDb_ { 0.0f };
std::atomic<float> maxGrLowDb_      { 0.0f };
std::atomic<float> maxGrMidDb_      { 0.0f };
std::atomic<float> maxGrHighDb_     { 0.0f };
```
Getters (next to `getCurrentGrDb()` ~L84–88):
```cpp
float getCurrentGrLowDb()  const noexcept { return currentGrLowDb_.load  (std::memory_order_relaxed); }
float getCurrentGrMidDb()  const noexcept { return currentGrMidDb_.load  (std::memory_order_relaxed); }
float getCurrentGrHighDb() const noexcept { return currentGrHighDb_.load (std::memory_order_relaxed); }
float getMaxGrLowDb()      const noexcept { return maxGrLowDb_.load  (std::memory_order_relaxed); }
float getMaxGrMidDb()      const noexcept { return maxGrMidDb_.load  (std::memory_order_relaxed); }
float getMaxGrHighDb()     const noexcept { return maxGrHighDb_.load (std::memory_order_relaxed); }
```
Extend `resetMaxGr()` to also clear the three `maxGr{Low,Mid,High}Db_`.

### 1b. Compute in the existing GR loop (`PluginProcessor.cpp` ~L1430–1505)
The GR block already loops osN samples tracking `minTotalL/R`. In the **same loop**, track per-band minima of the **post-Color band gains** already computed at L1346–1351:
```cpp
float minLow = 1.0f, minMid = 1.0f, minHigh = 1.0f;   // before the osN loop
...
// inside the loop, using the values you already have:
minLow  = std::min (minLow,  gLowOut[i]);
minHigh = std::min (minHigh, gHighOut[i]);
// minMid stays 1.0 — no mid band yet (Slice B fills gMidOut[i])
```
After the loop, publish (mirror the existing `grL/grR` conversion):
```cpp
const float grLow  = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minLow,  -120.0f));
const float grMid  = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minMid,  -120.0f)); // 0 for now
const float grHigh = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minHigh, -120.0f));
currentGrLowDb_.store  (grLow,  std::memory_order_relaxed);
currentGrMidDb_.store  (grMid,  std::memory_order_relaxed);
currentGrHighDb_.store (grHigh, std::memory_order_relaxed);
if (grLow  > maxGrLowDb_.load  (std::memory_order_relaxed)) maxGrLowDb_.store  (grLow,  std::memory_order_relaxed);
if (grHigh > maxGrHighDb_.load (std::memory_order_relaxed)) maxGrHighDb_.store (grHigh, std::memory_order_relaxed);
if (grMid  > maxGrMidDb_.load  (std::memory_order_relaxed)) maxGrMidDb_.store  (grMid,  std::memory_order_relaxed);
```
> RT-safe: scalar min/log/atomic-store, no allocation. `gLowOut`/`gHighOut` are the same buffers already read at L1437/L1483 — do not recompute the blend.

### 1c. History frame — add three band fields (`PluginProcessor.h` ~L69)
```cpp
struct HistoryFrame {
    float grDb    = 0.0f;   // total (unchanged)
    float outDb   = -120.0f;
    float inDb    = -120.0f;
    float clipDb  = 0.0f;
    float grLowDb  = 0.0f;  // NEW
    float grMidDb  = 0.0f;  // NEW (0 until Slice B)
    float grHighDb = 0.0f;  // NEW
};
```
Add per-frame accumulators next to `frameMaxGrDb_` (`frameMaxGrLowDb_`, `frameMaxGrMidDb_`, `frameMaxGrHighDb_`). In the history aggregation (`PluginProcessor.cpp` ~L1589–1611): the per-sample GR fill loop already derives total `grDb` from `grEnvBuf_[i]`. For the bands, aggregate the **block-level** `grLow/grMid/grHigh` computed in 1b into the frame max (band GR is a per-block scalar, so hold it across the frame like the current-value publish): set `frameMaxGr{Low,Mid,High}Db_ = max(existing, gr{Low,Mid,High})` once per block, write into the ring frame at flush, and reset to 0 after flush alongside `frameMaxGrDb_`.
> The ring format is internal/non-persisted — changing `HistoryFrame` is safe. `readHistorySince()` copies whole frames, so no signature change.

---

## 2. UI — GR meter becomes per-band (`GainReductionMeter.{h,cpp}`)

Same component, same bounds (88×354). Replace the 2-channel (L/R) model with an **N-band** model, `kNumBands = 3`, columns **LO · MID · HI**, MID drawn as a reserved/empty slot until Slice B feeds it.

### Header
Replace the `displayGrLDb_/RDb_`, `ballL_/R_`, `peakHoldL_/R_` members with arrays:
```cpp
static constexpr int kNumBands = 3;   // Low, Mid, High (Mid reserved until 3-band DSP)
float displayGrBandDb_ [kNumBands] { 0.0f, 0.0f, 0.0f };
std::unique_ptr<mdsp_ui::meters::MeterBallistics> ball_ [kNumBands];
std::unique_ptr<mdsp_ui::meters::PeakHoldModel>   peakHold_ [kNumBands];
float displayCurrentGrDb_ { 0.0f };   // keep — total, for readout
float displayMaxGrDb_ { 0.0f };       // keep — total max, for readout
```

### `sync(dtSec)`
Read the three band atomics + the total for the readout:
```cpp
const float raw[kNumBands] = { processor_.getCurrentGrLowDb(),
                               processor_.getCurrentGrMidDb(),
                               processor_.getCurrentGrHighDb() };
const auto cfg = makeGrBallisticsConfig();     // unchanged 1/50/1000 ms
for (int b = 0; b < kNumBands; ++b)
{
    float v = std::isfinite (raw[b]) ? std::abs (raw[b]) : 0.0f;
    displayGrBandDb_[b] = ball_[b]->process (juce::jmax (0.0f, v), dtSec, cfg);
    peakHold_[b]->process (displayGrBandDb_[b], dtSec, cfg);
}
float rawCur = processor_.getCurrentGrDb();     // total (max over bands×wide)
float rawMax = processor_.getMaxGrSinceResetDb();
displayCurrentGrDb_ = std::isfinite (rawCur) ? std::abs (rawCur) : 0.0f;
displayMaxGrDb_     = std::isfinite (rawMax) ? std::abs (rawMax) : 0.0f;
repaint();
```

### `paint()`
Reuse the existing panel/frame chrome. Replace the two-half bar layout with `kNumBands` equal columns (2 px gaps). For each band draw the existing `drawBar` (top-down `theme.warning` fill + peak line). For the **reserved MID** band, draw the empty slot only (no fill) with a faint centred `–` so the 3rd/placeholder slot reads as "coming". Add small band captions **LO / MID / HI** in `theme.textMuted` at the bar tops (height ~10). Keep the bottom numeric readout as **total** `current / max` (unchanged formatting). Peak-hold reset on readout click stays (also clears the three band peak holds + calls `processor_.resetMaxGr()`).

> Ballistics/peak-hold construction in the ctor: build the arrays in a loop. `resetPeakHolds()` loops the arrays.

---

## 3. UI — History Graph per-band traces (`HistoryGraphComponent.{h,cpp}`)

`drawTraces()` currently plots Out (fill), total GR (fill+stroke), In (line), Clip (overlay). **Keep** the total-GR fill as faint context, and **add three stroked band-GR traces** from the new frame fields, in distinct colours, plus a small legend.

Palette (from the file's `palette` namespace ~L10–20):
- **LO** → `palette::warning` (amber `0xe8704f`)
- **MID** → `palette::accent` (teal `0x33d2be`)
- **HI** → `palette::accentBright` (bright teal `0x5be7d6`) — if too close to MID, use a violet `juce::Colour::fromRGB(0x9b, 0x8c, 0xff)` added to the palette
- keep total GR fill at `palette::warning.withAlpha(~0.18f)` behind the band lines

For each band, build a path over `displayFrames_` mapping `grLowDb/grMidDb/grHighDb` through the same GR-range vertical mapping the total trace already uses (top-hanging, `dev`-range selector honoured), stroke at `m.strokeThin`/~1.4 px. Draw a compact legend (three colour chips + LO/MID/HI) top-right of the plot in `textMuted`. MID trace will be flat-zero until Slice B — that's expected and a good visual confirmation when it activates.

> Timer/drain unchanged (60 Hz `readHistorySince`). No processor calls added beyond the wider frame.

---

## 4. MainView
No layout change. `meterGr_.sync(dt)` signature is unchanged (still `sync(float)`), so `MainView.cpp:1710` is untouched unless you rename. Peak reset wiring at L1751 unchanged.

---

## 5. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
```
**Acceptance (Claude verifies items 1–5; avishali auditions 6):**
1. Plugin builds clean, no new warnings; AU validates.
2. **No** parameter count change (`auval` param count identical to prior build); no latency change (reported PDC identical).
3. GR meter shows **LO** and **HI** bars responding to band GR; **MID** is an empty reserved slot with a faint `–`; total `current / max` readout matches the old behaviour; readout click still resets holds.
4. Push input gain with **Color 100%** on a full-range mix: LO and HI bars move **independently**; at **Color 0%** they move **together** (linked) — this is the visible proof of the band-link blend.
5. History Graph shows LO/HI traces in distinct colours with a legend; MID trace flat at 0; In/Out/Clip/total-GR unchanged; no repaint stutter.
6. **Audition:** the per-band read is legible and useful for voicing; confirm the L/R→per-band trade is acceptable (or request an L/R toggle as a follow-up).

**Close gate:** update `docs/SIGNAL_FLOW.md` §7 (add per-band GR atomics + `HistoryFrame` band fields to the taps table) and note the GR meter is now per-band; append `docs/PROGRESS.md`; mark the slice in `PROMPTS/PLAN.md`; commit (plugin only, no submodule bump); archive `PROMPTS/SLICE_BAND_GR_VIZ_CLOSE.md`.

---

## Notes for the architect (context, not for Cursor)
- Band GR is intentionally the **post-Color** gain (`gLowOut/gHighOut`), so the meter shows the *effective* per-band reduction including the link blend — matches what the ear hears and what Slice B extends to 3 bands by adding `gMidOut`.
- Freezing `HistoryFrame` + the atomics at N=3 now means Slice B only *fills* `mid`, touching no metering format again.
