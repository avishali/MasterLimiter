# Cursor Task — Slice 8: Meters (GR + Input + Output + TP + LUFS)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only** (mostly). HQ widgets and the existing
> `mdsp_dsp::LoudnessAnalyzer` are reused as-is. Slice 8 wires
> processor snapshots into the UI and lays out a vertical meter strip.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- ONE focused diff. Stay strictly within §4 file scope.
- RT-safety: audio-thread additions (input/output peak detection,
  `LoudnessAnalyzer::process`) must be allocation-free. All buffers
  preallocated in `prepareToPlay`.
- Engine/UI boundary preserved: UI reads `std::atomic<float>` snapshots
  only; never touches DSP buffers directly.
- APVTS unchanged (no new parameters). Existing IDs frozen.
- Minimal diff. No new mdsp_ui or mdsp_dsp components in this slice.

────────────────────────────────────────
1. TRINITY (lightweight)
────────────────────────────────────────

Re-read before editing:

- HQ widget headers (confirm public API):
  - `shared/mdsp_ui/include/mdsp_ui/meters/widgets/LevelMeter.h`
  - `shared/mdsp_ui/include/mdsp_ui/meters/widgets/GainReductionMeter.h`
  - `shared/mdsp_ui/include/mdsp_ui/meters/MeterTypes.h`
  - `shared/mdsp_ui/include/mdsp_ui/meters/MeterValueModel.h` (if exists)
- HQ LUFS module:
  - `shared/mdsp_dsp/include/mdsp_dsp/loudness/LoudnessAnalyzer.h`
    Confirm `prepare(sampleRate, estimatedSamplesPerBlock)`, `process(buffer)`,
    `getSnapshot()` returning `LoudnessSnapshot{momentaryLufs, shortTermLufs,
    integratedLufs, peakDb}`. Internal atomics already exist.
- Reference for usage pattern: how AnalyzerPro consumes the meter widgets
  in its UI — grep `MasterLimiter/third_party/melechdsp-hq` is not the
  right path, look at the sibling `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro/Source/`
  to mirror the meter wiring idiom (Timer, Listener, snapshot polling).
- Existing product files to extend:
  - `MasterLimiter/Source/PluginProcessor.{h,cpp}`
  - `MasterLimiter/Source/PluginEditor.{h,cpp}`
  - `MasterLimiter/Source/ui/MainView.{h,cpp}`

Output the retrieval log. If the HQ widget APIs are not what we expect
(e.g. require external ballistics state rather than computing internally),
STOP and report — architect will decide between adapting wiring vs
extending HQ widgets (Q8-9 was "use as-is; revisit HQ later").

────────────────────────────────────────
2. TASK TITLE
────────────────────────────────────────

Add real-time meters to the MasterLimiter UI: per-channel input level,
per-channel output level + true-peak overlay, gain reduction, and a
LUFS readout cluster (Momentary / Short-term / Integrated). Reuses
`mdsp_ui` meter widgets and `mdsp_dsp::LoudnessAnalyzer` from HQ
without modification. Vertical meter strip on the right side of
MainView; parameters in the center grid (existing layout preserved
elsewhere).

────────────────────────────────────────
3. CONTEXT — decisions locked from Q8 interview
────────────────────────────────────────

| # | Decision |
|---|----------|
| Q8-1 | Show **all four** clusters: Input L/R, Output L/R, GR, LUFS (M/S/I numeric). |
| Q8-2 | Layout: vertical meter strip on the right of MainView; parameters in center grid below header. |
| Q8-3 | New atomics: input peak L/R, output peak L/R, output TP (single value). Existing `currentGrDb_` is the GR source. LUFS read from `LoudnessAnalyzer::getSnapshot()` directly (its atomics are internal). |
| Q8-4 | Ballistics: GR 0 ms attack / 200 ms release; level meters 1 ms attack / 300 ms release. Use `mdsp_dsp::MeterBallistics` if the HQ widget doesn't already apply ballistics internally — verify in Trinity. |
| Q8-5 | UI thread: 30 Hz `juce::Timer` in editor; meters poll atomics on each tick. |
| Q8-6 | Peak-hold: **infinite hold with click-to-reset** (AnalyzerPro-style). Per-meter or global reset button — decide based on the AnalyzerPro idiom you find in Trinity. |
| Q8-7 | Numeric readout: current dB value displayed near each meter (AnalyzerPro idiom). |
| Q8-8 | Gate: build clean + bench Slice 3/4/5 regressions all still PASS + manual audition in Live. |
| Q8-9 | Reuse HQ widgets as-is. No mdsp_ui modifications in this slice. |

References:
- `MasterLimiter/docs/SPEC.md` §3 (parameters, snapshots), §4 (snapshot contract)
- ADR-0004 Stage 6 — TP detector / IspTrimStage
- ADR-0005 — T/S split

────────────────────────────────────────
4. SCOPE — files you may CREATE / MODIFY
────────────────────────────────────────

### Product repo (`MasterLimiter/`)

**MODIFY:**
- `Source/PluginProcessor.h` — new atomics, `LoudnessAnalyzer` member, accessors.
- `Source/PluginProcessor.cpp` — input/output peak detection per block;
  feed audio into `LoudnessAnalyzer::process`; publish snapshots.
- `Source/PluginEditor.h` — Timer-derived class or composition; meter
  widget members; LUFS analyzer accessor.
- `Source/PluginEditor.cpp` — timer wiring; bounds; pass refs to MainView.
- `Source/ui/MainView.h` — meter widget members; new layout slots.
- `Source/ui/MainView.cpp` — instantiation, layout, polling delegation.

**CREATE (only if cleanly extractable):**
- `Source/ui/MeterStrip.{h,cpp}` — encapsulates the right-side meter
  column (input, GR, output, LUFS readouts). Optional. If MainView
  stays simple enough, inline the widgets there. Architect prefers
  inline unless line count balloons.

### HQ (`melechdsp-hq/`)

**Do not modify.** Reuse `mdsp_ui` widgets and `mdsp_dsp::LoudnessAnalyzer`
as-is. If a wiring detail genuinely requires an HQ change, STOP and ask.

NON-GOALS:
- NO new APVTS parameters.
- NO DSP changes (envelope, IspTrimStage, etc. untouched).
- NO new mdsp_ui or mdsp_dsp components.
- NO production-grade visual polish (color tweaks, animations, custom
  paint code). Use existing theme defaults.
- NO bench modifications.
- NO presets, save/load changes.

────────────────────────────────────────
5. PROCESSOR — `PluginProcessor.{h,cpp}` changes
────────────────────────────────────────

### Header additions

```cpp
#include <mdsp_dsp/loudness/LoudnessAnalyzer.h>

// Snapshot atomics — UI reads via accessors below.
std::atomic<float> inputPeakLDb_  { -100.0f };
std::atomic<float> inputPeakRDb_  { -100.0f };
std::atomic<float> outputPeakLDb_ { -100.0f };
std::atomic<float> outputPeakRDb_ { -100.0f };
std::atomic<float> outputTpDb_    { -100.0f };  // sample-rate-aware TP from IspTrimStage in TP mode; sample-peak when SP mode.

// LUFS lives in HQ; its internal atomics are the snapshot — expose by ref.
mdsp_dsp::LoudnessAnalyzer loudness_;

// Accessors for UI thread.
float getInputPeakLDb()  const noexcept { return inputPeakLDb_.load(std::memory_order_relaxed); }
float getInputPeakRDb()  const noexcept { return inputPeakRDb_.load(std::memory_order_relaxed); }
float getOutputPeakLDb() const noexcept { return outputPeakLDb_.load(std::memory_order_relaxed); }
float getOutputPeakRDb() const noexcept { return outputPeakRDb_.load(std::memory_order_relaxed); }
float getOutputTpDb()    const noexcept { return outputTpDb_.load(std::memory_order_relaxed); }
mdsp_dsp::LoudnessAnalyzer& getLoudnessAnalyzer() noexcept { return loudness_; }
```

### prepareToPlay

After existing component preparation:

```cpp
loudness_.prepare (sampleRate, samplesPerBlock);
loudness_.reset();
```

### processBlock

Add **before** input gain is applied (i.e. immediately after the
early-return / param checks, before the `applyGain` loop):

```cpp
// Input level snapshot (pre-input-gain, what the user sees as 'input').
{
    const float pL = buffer.getMagnitude (0, 0, n);
    const float pR = (nch > 1) ? buffer.getMagnitude (1, 0, n) : pL;
    inputPeakLDb_.store (juce::Decibels::gainToDecibels (pL, -100.0f),
                         std::memory_order_relaxed);
    inputPeakRDb_.store (juce::Decibels::gainToDecibels (pR, -100.0f),
                         std::memory_order_relaxed);
}
```

Add **at the end** of `processBlock` (after envelope is applied, after
optional TP path, after `currentGrDb_` is published):

```cpp
// Output level snapshot + TP (post everything).
{
    const float pL = buffer.getMagnitude (0, 0, n);
    const float pR = (nch > 1) ? buffer.getMagnitude (1, 0, n) : pL;
    outputPeakLDb_.store (juce::Decibels::gainToDecibels (pL, -100.0f),
                          std::memory_order_relaxed);
    outputPeakRDb_.store (juce::Decibels::gainToDecibels (pR, -100.0f),
                          std::memory_order_relaxed);

    // TP: in TP mode, the IspTrimStage's job is to keep TP at or below
    // ceiling; in SP mode, output TP ≈ output sample peak. For Slice 8
    // we publish the max of L/R sample peak as a base-rate approximation,
    // which is the simplest correct thing for SP mode. A more rigorous
    // TP measurement in the snapshot path would require an additional
    // oversampled measurement in this block; deferred (informational note).
    outputTpDb_.store (std::max (outputPeakLDb_.load(std::memory_order_relaxed),
                                 outputPeakRDb_.load(std::memory_order_relaxed)),
                       std::memory_order_relaxed);

    loudness_.process (buffer);
}
```

RT-safety check: `getMagnitude` is `juce::AudioBuffer`'s built-in
`std::max(std::abs(...))` reduction — allocation-free. `LoudnessAnalyzer::process`
is already used in AnalyzerPro on the audio thread (verify via Trinity).

────────────────────────────────────────
6. EDITOR + MAINVIEW — UI wiring
────────────────────────────────────────

### Editor (`PluginEditor.h`/`.cpp`)

- Inherit from `juce::Timer` (or hold one as a member).
- `startTimerHz(30)` in constructor.
- `timerCallback()` calls `repaint()` on the meter strip (and any other
  display elements that need refresh). Avoid blanket `repaint()` on
  the whole editor.
- Pass the `MasterLimiterAudioProcessor&` reference into MainView (it
  needs the processor accessors).

### MainView (`Source/ui/MainView.{h,cpp}`)

- Existing parameter slider grid stays in place but moves to the LEFT
  area; the existing header text stays at top.
- Right-side meter strip (~200 px wide) contains, top to bottom:
  1. **Input** label + stereo `mdsp_ui::meters::LevelMeter` (L+R bars)
     + numeric L/R dB readouts.
  2. **GR** label + `mdsp_ui::meters::GainReductionMeter` (single bar,
     downward) + current GR dB readout.
  3. **Output** label + stereo `LevelMeter` (L+R bars) + numeric readouts.
     A small "TP" indicator next to or above the output meter shows the
     current output TP dB; when in SP mode, label it "SP" to be honest
     about what's being measured.
  4. **LUFS** numeric cluster (no bar): three text rows showing
     Momentary / Short-term / Integrated, formatted as e.g. "M: -14.2 LUFS".
     Polls `processor.getLoudnessAnalyzer().getSnapshot()` on each timer
     tick.
- Click-to-reset for peak-hold (Q8-6):
  - If the HQ widget exposes a `resetPeakHold()` or similar API, wire
    a `MouseListener` on each meter that calls it on click.
  - If the widget doesn't expose it, instead provide a small "Reset" button
    next to the meter cluster that calls reset on all meters AND
    `processor.getLoudnessAnalyzer().resetPeak()` + a similar atomic
    `resetPeakHoldRequested_` flag the processor honors on next block.
  - Choose the cleaner of the two based on Trinity inspection.
- Editor size: bump default size from 720 × 360 to **920 × 420** to
  accommodate the meter strip without compressing the parameter grid.
  Update `setSize` in the editor and any cached default in MainView.

### Style

- Use existing `mdsp_ui::LookAndFeel` / theme (already installed at
  Slice 1). Do not author new colors or paint code.
- Numeric readouts: `juce::Label` with theme defaults.
- Bracket the meter strip with a subtle vertical separator from the
  parameter area — a single `juce::Line` or a thin `juce::Rectangle`
  fill from theme tokens. Don't over-design.

────────────────────────────────────────
7. BUILD & VERIFY
────────────────────────────────────────

### Build

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```

Zero new warnings from §4 files.

### Bench regression — all three slices must still PASS

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE08_REGRESSION_S$SLICE"
done
```

Expected: all three end with `PASS N/N` (no DSP changed in this slice).

### Manual audition

- Load the Release VST3 in Ableton Live on a stereo track with a real mix.
- Verify: input meter L/R move with the input audio; GR meter rises when
  audio is being limited; output meters track post-limit audio; LUFS
  numbers update smoothly.
- Toggle `ceiling_mode` SP ↔ TP: meters keep working, no glitching, no
  audio dropouts.
- Click peak-hold on each meter (or reset button): peak-hold resets.
- Visual sanity check: meter strip doesn't overlap or clip parameters;
  numeric readouts are legible at theme defaults.

────────────────────────────────────────
8. GATE CHECKLIST
────────────────────────────────────────

- [ ] HQ + product build clean; only §4 files touched.
- [ ] Bench Slice 3 / 4 / 5 regression PASS unchanged.
- [ ] Plugin loads in Live as VST3 and AU.
- [ ] All four meter clusters render and update at ~30 Hz.
- [ ] Input meter shows correct L/R level (pre-input-gain).
- [ ] GR meter shows correct reduction (matches `currentGrDb_` at 30 Hz).
- [ ] Output meters track post-limit audio; show 0 dB at full-scale output.
- [ ] LUFS Momentary / Short-term / Integrated update; reset on plugin
      reload.
- [ ] Peak-hold engages on transients; click-to-reset works.
- [ ] No visual flicker or partial repaints.
- [ ] SP↔TP mode switch: meters smooth across the transition.
- [ ] No RT-safety violations (no allocations in `processBlock` —
      confirm by reading the diff, not just trusting `getMagnitude`).
- [ ] No new compiler warnings.

────────────────────────────────────────
9. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. RETRIEVAL LOG (§1 format).
2. File list created / modified, one-line purpose each.
3. Build summary lines (HQ + product, both Debug and Release).
4. Bench regression: invocation + final lines for Slice 3, 4, 5 runs.
5. Screenshot OR text description of the live editor with meters
   active (Cursor can't take screenshots; describe the layout it
   produced: which widgets are where, sizes, what readouts show).
6. Self-check against §8.
7. Open questions.

DO NOT skip the bench regression. Even though no DSP changed, the
regression confirms the new processor wiring didn't accidentally
alter timing or buffer state.

────────────────────────────────────────
10. POST-SLICE NOTES (architect)
────────────────────────────────────────

When this slice passes:
- Architect copies bench `results.*` only if anything shifted (which
  shouldn't happen this slice). Otherwise just update PROGRESS with the
  "Slice 8 closed; meters wired" entry.
- Slice 7 interview begins (stereo / M-S link).
