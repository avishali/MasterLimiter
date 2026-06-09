# SLICE — History Graph v2 (accuracy, range, scroll, clip overlay) + Clip-LED fix

**Status:** ready for Cursor · **Architect:** Claude · **Audition/decide:** avishali
**Repo:** plugin `MasterLimiter` only. Commit + push after the slice.
**Builds on:** the shipped history-graph slice. Companion doc: `docs/SIGNAL_FLOW.md`.

---

## Goals (from avishali's audition)
1. **Finer resolution** — the traces look "squared/blocky." Root cause: the processor writes the *same per-block value to every sample* of each history frame, so GR and levels are step functions. Fix by capturing **true per-sample envelopes** (output, input) and **per-sample gain-reduction**, plus a finer frame cadence.
2. **Slower scroll options** — add longer time windows (up to 30 s).
3. **Wider / selectable dB range** — let the user choose the level floor and GR range to zoom in/out for detail.
4. **Clipper visualization** — draw the **clip threshold line** and render **clipped regions in a distinct color**.
5. **Bug fix — clipper LED stopped flashing** (regression, see §5).

---

## Allowed files to touch
```
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/ui/HistoryGraphComponent.h
Source/ui/HistoryGraphComponent.cpp
Source/ui/MainView.cpp          (clip-LED repaint fix, §5)
Source/PluginEditor.cpp         (bump default graph window size, §4)
docs/SIGNAL_FLOW.md             (one-line: frame now carries per-sample GR + clipDb)
docs/PROGRESS.md
PROMPTS/PLAN.md
PROMPTS/SLICE_HISTORY_GRAPH_V2_CLOSE.md   (new, at close)
```
Do **not** change the limiter/clipper DSP behavior — only *read* per-sample values that are already computed and route them to the history ring.

---

## 1. Processor — per-sample accurate capture (`PluginProcessor.h/.cpp`)

### 1a. Extend the frame + add per-sample scratch (header)
```cpp
struct HistoryFrame { float grDb; float outDb; float inDb; float clipDb; };   // add clipDb

// Per-host-sample scratch, filled during processCore, drained by the frame loop:
std::vector<float> grEnvBuf_;     // total limiter gain coeff per host sample (1.0 = no reduction)
std::vector<float> clipEnvBuf_;   // clipper reduction in dB per host sample (0 = none)
```
Bump the ring for the longer windows + finer cadence:
```cpp
static constexpr int kHistoryRingSize = 65536;      // 2^16; ~32 s at 0.5 ms/frame
```
`HistoryFrame` default `{0, -120, -120, 0}`.

### 1b. prepareToPlay
- `grEnvBuf_.assign(samplesPerBlock, 1.0f); clipEnvBuf_.assign(samplesPerBlock, 0.0f);`
- **Finer cadence:** `historyFrameSamples_ = jmax(1, round(sampleRate * 0.0005));`  // ~0.5 ms/frame
- Reset `frameMaxClipDb_ = 0.0f` alongside the other frame accumulators (add the member).

### 1c. Fill the scratch from values already computed in `processCore`
**At the very top of `processCore`** (after `n` is known), reset for this block:
```cpp
std::fill_n (grEnvBuf_.data(),   n, 1.0f);
std::fill_n (clipEnvBuf_.data(), n, 0.0f);
```
**Clipper loop** (~L711–748): you already compute per-sample per-channel `attDb` (negative = reduction) at OS rate. Map to host index and keep the deepest reduction per host sample:
```cpp
const int hostIdx = juce::jmin (n - 1, i * n / osN);          // OS index i → host sample
// when ax > 1e-6 and attDb < 0:
clipEnvBuf_[hostIdx] = std::max (clipEnvBuf_[hostIdx], -attDb);   // store as positive dB
```
**Wideband gain-apply loop** (~L957–1005, both stereo and M/S paths): you already compute `totalGain` (the product of band × wide gain, and `msSafetyGain` in M/S). Keep the *minimum* gain (deepest reduction) per host sample across channels & OS:
```cpp
const int hostIdx = juce::jmin (n - 1, i * n / osN);
grEnvBuf_[hostIdx] = std::min (grEnvBuf_[hostIdx], totalGain);    // 1.0 = no reduction
```
(The limiter-inactive `else` branch leaves the scratch at the reset defaults — GR 1.0, clip 0 — which is correct.)

### 1d. Rewrite the frame-accumulator loop (end of processCore) to use per-sample data
Replace the current block-held loop. Read **actual output** (`buffer`), **time-aligned input** (`dryScratch_`), and the per-sample GR/clip scratch:
```cpp
const float* outL = buffer.getReadPointer (0);
const float* outR = nch > 1 ? buffer.getReadPointer (1) : outL;
const float* dryL = dryScratch_.getReadPointer (0);
const float* dryR = nch > 1 ? dryScratch_.getReadPointer (1) : dryL;

for (int i = 0; i < n; ++i)
{
    const float outDb  = juce::Decibels::gainToDecibels (std::max (std::abs (outL[i]), std::abs (outR[i])), -120.0f);
    const float inDb   = juce::Decibels::gainToDecibels (std::max (std::abs (dryL[i]), std::abs (dryR[i])), -120.0f);
    const float grDb   = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (grEnvBuf_[i], -120.0f));
    const float clipDb = clipEnvBuf_[i];

    frameMaxGrDb_   = std::max (frameMaxGrDb_,   grDb);
    frameMaxOutDb_  = std::max (frameMaxOutDb_,  outDb);
    frameMaxInDb_   = std::max (frameMaxInDb_,   inDb);
    frameMaxClipDb_ = std::max (frameMaxClipDb_, clipDb);

    if (++historySampleCounter_ >= historyFrameSamples_)
    {
        const uint32_t w = historyWriteIdx_.load (std::memory_order_relaxed);
        historyRing_[w & (kHistoryRingSize - 1)] = { frameMaxGrDb_, frameMaxOutDb_, frameMaxInDb_, frameMaxClipDb_ };
        historyWriteIdx_.store (w + 1, std::memory_order_release);
        historySampleCounter_ = 0;
        frameMaxGrDb_ = 0.0f; frameMaxOutDb_ = -120.0f; frameMaxInDb_ = -120.0f; frameMaxClipDb_ = 0.0f;
    }
}
```
> RT-safety unchanged: still only max/min/compare + one store-release per frame. The OS→host mapping is integer math. No allocation (scratch sized in prepareToPlay).

### 1e. New getter for the clip threshold line
```cpp
// Input-referred clip threshold in dBFS: the clipper boosts by -driveDb before clipping at 0 dBFS,
// so clipping starts at an input level of clipper_drive_db. Returns +1 (off-scale) when clipper is bypassed.
float getClipThresholdDbForGraph() const noexcept;   // = clipperActive ? clipper_drive_db : +1e9f
```

---

## 2. Component — smoother rendering & per-pixel min/max (`HistoryGraphComponent.{h,cpp}`)
The per-sample data from §1 already removes the stepping. Additionally:
- In `frameForPixel`, you currently **max**-reduce frames per pixel. Keep that for `out`/`in`/`gr`/`clip` (peak-preserving). Make sure `framesPerPixel ≥ 1` decimates correctly for the long windows.
- Render `outputFill`, `grStroke`, `inputLine` as before (they're already AA paths) — with accurate data they'll read as smooth envelopes, not blocks.
- Optional polish (do if cheap): draw the **GR stroke** at 1.5 px and add a subtle 1 px highlight, so the limiter motion reads clearly.

---

## 3. Component — selectable dB range + slower scroll
Replace the fixed constants with state driven by two new selectors (plus the existing window selector). Lay all three in the header row.

### 3a. Window (scroll) selector — add slower options
```
0.75 s · 1.5 s · 3 s · 6 s · 10 s · 15 s · 30 s     (default 3 s)
```
Update the ring (§1a, 65536) so 30 s fits at 0.5 ms cadence (~60 k frames < 65536 ✓).

### 3b. Level-range selector (level floor) — `kLevelBottomDb` becomes a variable
```
-24 · -36 · -48 · -60 dB     (default -48)
```
Wider default so more of the signal is visible; user can zoom to -24 for detail. Regenerate the dB grid lines to suit the chosen floor (e.g. 0/-6/-12/-18/-24/-36/-48… label the major ones, don't overcrowd).

### 3c. GR-range selector — `kGrRangeDb` becomes a variable
```
6 · 12 · 18 · 24 dB     (default 18)
```
`grY()` uses the selected range.

Keep selector styling consistent with the existing `windowSelector_` (same palette colors).

---

## 4. Component — clipper threshold line + clipped-in-color
- **Threshold line:** draw a horizontal line at `levelY(getClipThresholdDbForGraph())` in a **distinct clip color** (e.g. a red `0xff5a4f` — clearly different from the GR orange `warning`). Skip drawing when the getter returns the off-scale sentinel (clipper bypassed) or the value is above the top of the scale. Label it "Clip".
- **Clipped regions:** for each pixel where `frame.clipDb > 0`, draw a **clip overlay in the same red**: a vertical tick/segment from the top of the graph down proportional to `clipDb` (use a small fixed range, e.g. 0…6 dB → top 0…20% of height), OR a solid red marker band at the top. Make clipped moments unmistakable and distinct from GR.
- **Default window size:** in `PluginEditor.cpp toggleHistoryGraph()`, bump `centreWithSize(720, 320)` → **`centreWithSize(940, 460)`** and raise the resize-limits max if needed, so there are more pixels for detail.

---

## 5. Clip-LED repaint fix (`MainView.cpp`) — regression
**Root cause:** the LED is drawn in `MainView::paint()` near `lblClipperDrive_` (x≈623), but the 30 Hz timer only calls `repaintMeterStrip()`, which repaints `meterStripArea_` (the union of the meters, starting at **x=650**). The LED is **outside** that rectangle, so `clipLedLevel_` updates every frame but its pixels are never invalidated → no flash.

**Fix:** in `syncMetersFromProcessor()` where `clipLedLevel_` is updated (~L1555), invalidate the LED's own rectangle whenever it's lit or just went dark:
```cpp
const float prevLed = clipLedLevel_;
clipLedLevel_ = master_limiter_ui::processClipLed (*clipBallistics_, clipDb > 0.0f, dt);
if (clipLedLevel_ > 0.001f || prevLed > 0.001f)
    repaint (lblClipperDrive_.getBounds().removeFromRight (12));   // the LED region (matches the paint at ~L1353)
```
Verify the LED now flashes in time with audible clipping.

---

## 6. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -20
cp -R build/MasterLimiter_artefacts/Release/AU/MasterLimiter.component  ~/Library/Audio/Plug-Ins/Components/
cp -R build/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3     ~/Library/Audio/Plug-Ins/VST3/
auval -v aufx MaLm Melc 2>&1 | tail -5
```
Acceptance:
1. Builds clean; AU validates.
2. Traces are **smooth** (no block stepping); GR motion and signal envelope read accurately.
3. Window selector includes up to 30 s and scrolls correctly; ring holds the longest window with no wrap glitches.
4. Level-floor and GR-range selectors change the vertical scale live.
5. Clip threshold line shows at the right dB and tracks the Clipper drive; clipped moments render in the distinct red; both hidden when the clipper is bypassed.
6. **Clipper LED flashes** with audible clipping again.
7. CPU still ~7–10%; no added allocations on the audio thread (verify the scratch is sized in prepareToPlay only).

Close gate: update `docs/SIGNAL_FLOW.md` §7 (frame now carries per-sample GR + clipDb), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit + push; archive `PROMPTS/SLICE_HISTORY_GRAPH_V2_CLOSE.md`.

---

## Notes
- The per-sample GR comes from the **total** limiter gain (band × wide × M/S safety) — exactly what the GR meter reports — so the graph and the GR meter agree.
- If 0.5 ms/frame ever feels heavy on very long sessions, it isn't a leak — the ring is fixed 65536; it just overwrites. Cadence/ring are tunable constants.
- Future (out of scope): min/max waveform band for output (true waveform look), true-peak overlay, freeze/snapshot button.
```
