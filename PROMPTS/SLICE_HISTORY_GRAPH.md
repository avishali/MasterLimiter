# SLICE — History Graph Window (Pro-L 2 style)

**Status:** ready for Cursor · **Architect:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` only (no SDK change). Commit + push after the slice.
**Companion doc:** `docs/SIGNAL_FLOW.md` (§7 metering taps explains the data this graph visualizes).

---

## Goal
Add a **Pro-L-2-style scrolling history graph** in a **separate window**, opened from a **button in the main header**. The graph shows, over a scrolling time window:
- **Gain reduction** trace — filled down from the top (0 dB at top), the primary trace.
- **Output level** envelope — filled up toward the **Ceiling** line.
- **Input level** — a faint reference line.
- A **dB grid + ceiling line + time grid**, accurate and readable like Pro-L 2.

Purpose: visually analyze attack/release behavior and time-constants while tuning (esp. the new LookaheadFollower release). This must be **accurate** — uniform time axis, no missed transients.

---

## Why a ring buffer is required
Today the processor publishes only **instantaneous scalar atomics** read by the UI at 30 Hz (`SIGNAL_FLOW.md` §7). A scrolling graph needs **time history**, so this slice adds a **lock-free SPSC ring buffer** of decimated frames in the processor, written on the audio thread and drained by the graph component on the UI thread.

---

## Allowed files to touch
```
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/ui/HistoryGraphComponent.h        (new)
Source/ui/HistoryGraphComponent.cpp      (new)
Source/PluginEditor.h
Source/PluginEditor.cpp
Source/ui/MainView.h
Source/ui/MainView.cpp
CMakeLists.txt                            (register the new .cpp)
docs/PROGRESS.md
PROMPTS/PLAN.md
PROMPTS/SLICE_HISTORY_GRAPH_CLOSE.md      (new, at close)
```
Do **not** touch `processCore`'s DSP math, the limiter/envelope/oversampler, or any param IDs. The only `processCore` edit is appending a metering write at the very end (see §1).

---

## 1. Processor — lock-free history ring (`PluginProcessor.h/.cpp`)

### Frame + ring (header)
```cpp
struct HistoryFrame { float grDb; float outDb; float inDb; };   // GR positive = reduction

static constexpr int kHistoryRingSize = 4096;                   // power of two; ~8 s at 2 ms/frame
HistoryFrame historyRing_[kHistoryRingSize];
std::atomic<uint32_t> historyWriteIdx_ { 0 };                   // monotonic; UI keeps its own read cursor

// Frame accumulator (audio thread only):
int   historyFrameSamples_ = 0;     // samples per frame, set in prepareToPlay (~2 ms)
int   historySampleCounter_ = 0;
float frameMaxGrDb_  = 0.0f;
float frameMaxOutDb_ = -120.0f;
float frameMaxInDb_  = -120.0f;
```

### Public API (header)
```cpp
// UI drains new frames since its cursor. Returns how many were copied (<= maxOut).
// Lock-free, RT-safe on the audio side; safe to call from the UI thread.
int   readHistorySince (uint32_t& inOutCursor, HistoryFrame* out, int maxOut) const noexcept;
uint32_t getHistoryWriteIndex() const noexcept { return historyWriteIdx_.load (std::memory_order_acquire); }
double getHistorySampleRate() const noexcept;        // host SR (for time axis)
int    getHistoryFrameSamples() const noexcept { return historyFrameSamples_; }
float  getCeilingDbForGraph() const noexcept;        // current ceiling_db (for the ceiling line)
```

### prepareToPlay
```cpp
historyFrameSamples_ = juce::jmax (1, (int) std::llround (sampleRate * 0.002));  // ~2 ms/frame
historySampleCounter_ = 0;
frameMaxGrDb_ = 0.0f; frameMaxOutDb_ = -120.0f; frameMaxInDb_ = -120.0f;
historyWriteIdx_.store (0, std::memory_order_release);
```

### Write — at the END of `processCore`, after output metering (~L1055+)
Drive a **uniform sample-cadence** loop so the time axis is even regardless of block size. Use the block's already-computed values (held constant across the block — acceptable resolution at ~2 ms frames):
```cpp
const float blkGrDb  = std::max (currentGrLDb_.load (rlx), currentGrRDb_.load (rlx));   // GR this block
const float blkOutDb = std::max (outputPeakLDb_.load (rlx), outputPeakRDb_.load (rlx));
const float blkInDb  = std::max (inputPeakLDb_.load (rlx),  inputPeakRDb_.load (rlx));
for (int i = 0; i < n; ++i)
{
    frameMaxGrDb_  = std::max (frameMaxGrDb_,  blkGrDb);
    frameMaxOutDb_ = std::max (frameMaxOutDb_, blkOutDb);
    frameMaxInDb_  = std::max (frameMaxInDb_,  blkInDb);
    if (++historySampleCounter_ >= historyFrameSamples_)
    {
        const uint32_t w = historyWriteIdx_.load (std::memory_order_relaxed);
        historyRing_[w & (kHistoryRingSize - 1)] = { frameMaxGrDb_, frameMaxOutDb_, frameMaxInDb_ };
        historyWriteIdx_.store (w + 1, std::memory_order_release);   // publish
        historySampleCounter_ = 0;
        frameMaxGrDb_ = 0.0f; frameMaxOutDb_ = -120.0f; frameMaxInDb_ = -120.0f;
    }
}
```
`readHistorySince` copies frames `[cursor, writeIdx)` (clamped to ring capacity if the UI fell behind) into `out`, advances `cursor`, returns count. **No locks, no allocation.**

> RT-safety: audio thread only ever does `max`, integer compare, one store-release. Reader uses acquire-load + plain copies. SPSC — correct without a mutex. A torn frame is impossible (single producer writes the struct fully before publishing the index).

---

## 2. New component — `Source/ui/HistoryGraphComponent.{h,cpp}`
`class HistoryGraphComponent : public juce::Component, private juce::Timer`

- Ctor takes `MasterLimiterAudioProcessor&` (+ the `mdsp_ui::UiContext&` for theme colors, matching existing components).
- `startTimerHz(60)` for smooth scroll. `timerCallback()`: `readHistorySince(cursor_, tmp, …)`, append into an internal display buffer (a `std::vector<HistoryFrame>` or `juce::Array`, capped to the visible window length), then `repaint()`.
- **Display model:** maintain a rolling buffer sized to the visible window in frames = `windowSeconds_ * SR / frameSamples`. Newest on the **right**, scrolling **right → left** (Pro-L convention).
- **Time window control:** internal combo/segment **1.5 s / 3 s / 6 s** (default 3 s). No APVTS param — local UI state.
- **paint():**
  - Background + **dB grid**: horizontal lines at 0, −3, −6, −9, −12, −18 dB (label them). Map dB→y with a fixed top range (e.g. top = 0 dB, bottom = −24 dB for the level view; GR uses a 0…−`grRange` scale, **default 18 dB**, top-anchored).
  - **Ceiling line:** horizontal line at `getCeilingDbForGraph()`.
  - **Output level fill:** translucent filled area from bottom up to `outDb` per column.
  - **GR trace:** filled region hanging from the top, depth = `grDb` (use the accent/warning color; this is the star of the show — make it crisp).
  - **Input line:** thin faint line at `inDb`.
  - **Time grid:** vertical ticks every 0.5 s with small labels (e.g. "-1s", "-2s").
  - Use theme colors from `UiContext` (match the existing meters' palette — teal/dark per `ui-aesthetic-direction`).
- Reset the cursor to `getHistoryWriteIndex()` when the window opens so it starts live (no stale backlog dump).
- Pixel mapping: column x = `width * (1 - age/windowFrames)`. Decimate or average frames-per-pixel if frames > width (max-reduce GR, max output) so transients aren't lost.

Keep it a self-contained Component so it can live in any container.

---

## 3. Window + header button

### Window (owned by the editor — `PluginEditor.h/.cpp`)
- Add `std::unique_ptr<juce::DocumentWindow> historyWindow_;` (a small `DocumentWindow` subclass, or a configured `DocumentWindow` whose content is a `HistoryGraphComponent`).
- `toggleHistoryGraph()`: if window exists → close/null it; else create it, set content to a new `HistoryGraphComponent(processor, ui_)`, `setResizable(true,…)`, a sane default size (e.g. 720×320), `setVisible(true)`, `toFront`. On the window's close button → null the pointer (use a `closeButtonPressed` override or a lambda). Ensure the timer stops when destroyed (Timer dtor handles it).
- Make the window **non-modal**, independent, and remember it's optional (plugin works without it open).

### Header button (`MainView.h/.cpp`)
- Add `juce::TextButton btnHistory_ { "Graph" };` in the header. Place it in the **free gap around x≈956** (presetMenu ends ~960, Bypass starts ~982) — if too tight, shift `btnBypass_` left slightly and give the new button ~64 px; keep the existing aesthetic/L&F.
- Expose `std::function<void()> onToggleHistoryGraph;` on MainView; `btnHistory_.onClick = [this]{ if (onToggleHistoryGraph) onToggleHistoryGraph(); };`
- In `PluginEditor` ctor, wire `mainView.onToggleHistoryGraph = [this]{ toggleHistoryGraph(); };`

---

## 4. CMake
Add `Source/ui/HistoryGraphComponent.cpp` to the plugin target's source list (mirror how the other `Source/ui/*.cpp` are added).

---

## 5. Build & verify (Release)
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -20
cp -R build/MasterLimiter_artefacts/Release/AU/MasterLimiter.component  ~/Library/Audio/Plug-Ins/Components/
cp -R build/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3     ~/Library/Audio/Plug-Ins/VST3/
```
Acceptance:
1. Builds clean (Release).
2. Header **Graph** button opens a separate, resizable window; closing it doesn't affect audio.
3. Graph scrolls smoothly (60 Hz), GR trace hangs from top and tracks limiting; output fill rises to the ceiling line; input line visible; dB + time grids accurate; time-window selector (1.5/3/6 s) works.
4. **RT-safe:** no allocations or locks added to the audio thread (only the max/compare/store-release loop). Verify the write loop adds negligible CPU (it's O(n) integer/float work).
5. Opening/closing repeatedly doesn't leak or crash; multiple plugin instances each get their own window.

---

## 6. Close gate
1. Update `docs/PROGRESS.md` + `PROMPTS/PLAN.md` (history graph shipped; ring-buffer metering seam added).
2. Note in `docs/SIGNAL_FLOW.md` §7 that a history ring now exists (one-line update is fine — that file is in the allowed list via docs/).
3. Commit + push plugin repo. Never leave a slice unpushed.
4. Archive this prompt → `PROMPTS/SLICE_HISTORY_GRAPH_CLOSE.md`.

---

## Notes / future (out of scope now)
- Later we may **embed** the graph in the main window (there's room) instead of a separate window — keeping `HistoryGraphComponent` self-contained makes that a trivial reparent.
- Later: add a **true-peak** output trace and a **clip** marker overlay; store `outTpDb` in the frame if wanted (cheap to extend `HistoryFrame`).
- Frame cadence (~2 ms) and ring size (4096 ≈ 8 s) are generous; tune if you want a longer max window.
```
