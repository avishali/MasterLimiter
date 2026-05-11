# Cursor Task — Slice 8.1: UI polish (adopt AnalyzerPro pattern, redesign layout, fix bugs)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only.** No HQ changes, no DSP changes. Copy AnalyzerPro's
> polished UI components into MasterLimiter, adapt for limiter semantics
> (GR meter is new), redesign the layout, fix the ASCII LUFS-label bug,
> stabilize numeric readout ballistics.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- ONE focused diff. Stay strictly within §4 file scope.
- No HQ modifications. AnalyzerPro is **read-only** in this slice — do
  not touch its files; copy patterns into MasterLimiter.
- No DSP changes. APVTS unchanged. No bench code changes.
- RT-safety preserved on the audio thread (no new processor work this
  slice; product-side changes are UI-thread only).

────────────────────────────────────────
1. WHY (read once)
────────────────────────────────────────

The Slice 8 audition surfaced UI issues that masked operational
correctness:

1. LUFS labels render as `77:`, `83:`, `73:` — ASCII codes for 'M', 'S',
   'I'. Label construction bug.
2. GR meter grows bottom-up (wrong for GR semantics); should grow
   **top-down from 0 dB at top**.
3. GR is a single bar; user wants **per-channel L/R**.
4. Numeric readouts flicker at the audio block rate — need slower
   visual ballistics on the displayed values.
5. Overall layout is cramped, doesn't match the MelechDSP brand polish
   shipped in AnalyzerPro.

AnalyzerPro has product-local polished components that wrap HQ widgets
with proper sizing, layout, ballistics, peak-hold UX, and theme
integration:

- `AnalyzerPro/Source/ui/meters/MeterComponent.{h,cpp}`
- `AnalyzerPro/Source/ui/meters/MeterGroupComponent.{h,cpp}`
- `AnalyzerPro/Source/ui/loudness/LoudnessNumericPanel.{h,cpp}`

These are what we adopt. Note: **AnalyzerPro has no GR meter**, so we
must extend our local copy of `MeterComponent` to support GR semantics
(top-down direction, GR scale, dB readout in positive numbers).

────────────────────────────────────────
2. TRINITY (lightweight)
────────────────────────────────────────

Read before editing:

- **AnalyzerPro source (read-only reference):**
  - `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro/Source/ui/meters/MeterComponent.{h,cpp}`
  - `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro/Source/ui/meters/MeterGroupComponent.{h,cpp}`
  - `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro/Source/ui/loudness/LoudnessNumericPanel.{h,cpp}`
  - `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro/Source/PluginEditor.{h,cpp}` (theme/LookAndFeel pattern, resize behavior)
  - `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro/Source/ui/MainView.{h,cpp}` (layout idiom)
- **MasterLimiter existing (to extend / replace):**
  - `Source/PluginProcessor.h` (snapshot accessors — keep)
  - `Source/PluginEditor.{h,cpp}` (theme application, Timer)
  - `Source/ui/MainView.{h,cpp}` (current placeholder layout)
- **HQ widgets (no edits):**
  - `shared/mdsp_ui/include/mdsp_ui/meters/widgets/LevelMeter.h`
  - `shared/mdsp_ui/include/mdsp_ui/meters/widgets/GainReductionMeter.h`
  - `shared/mdsp_dsp/include/mdsp_dsp/loudness/LoudnessAnalyzer.h`
  - `shared/mdsp_ui/include/mdsp_ui/{Theme,LookAndFeel,ThemeVariant,UiContext}.h`

Output the retrieval log. If AnalyzerPro's components have dependencies
on AnalyzerPro-specific types we can't import (e.g. AnalyzerEngine,
specific snapshot structures), STOP and report — we'll adapt the
imports list or extract just the visual pattern.

────────────────────────────────────────
3. TASK TITLE
────────────────────────────────────────

Adopt AnalyzerPro's polished meter and loudness UI components into
MasterLimiter (local copies, no HQ lift this slice). Extend the
`MeterComponent` copy to support a GR meter variant (top-down, per-channel
L/R, GR scale). Redesign `MainView` and the editor sizing/layout to
match AnalyzerPro's visual hierarchy and proportions. Stabilize numeric
readout ballistics so values are readable at 30 Hz. Fix the ASCII LUFS
labels bug as a side-effect of using the proper `LoudnessNumericPanel`.

────────────────────────────────────────
4. SCOPE — files you may CREATE / MODIFY
────────────────────────────────────────

### Product repo (`MasterLimiter/`)

**CREATE (local copies of AnalyzerPro patterns):**
- `Source/ui/meters/MeterComponent.h`
- `Source/ui/meters/MeterComponent.cpp`
- `Source/ui/meters/MeterGroupComponent.h`
- `Source/ui/meters/MeterGroupComponent.cpp`
- `Source/ui/loudness/LoudnessNumericPanel.h`
- `Source/ui/loudness/LoudnessNumericPanel.cpp`
- `docs/TODO.md` — record candidates for future HQ lift (§9)

**MODIFY:**
- `CMakeLists.txt` — add the four new `.cpp` files to the plugin sources
- `Source/PluginEditor.h`
- `Source/PluginEditor.cpp`
- `Source/ui/MainView.h`
- `Source/ui/MainView.cpp`

### HQ (`melechdsp-hq/`)

**Do not modify anything.** No HQ widget edits, no theme edits, no
bench edits. If you find an HQ-side issue blocking the slice, STOP and
ask.

### AnalyzerPro (read-only reference)

**Do not modify.** Only read its files to understand patterns. Copy
content into MasterLimiter; don't symlink, don't include.

NON-GOALS:
- NO new DSP, no APVTS changes.
- NO new bench metrics, no DSP regression risk → existing Slice 3/4/5
  benches must continue to PASS without re-baselining.
- NO rich-tooltip system (per Q8.1-3 = JUCE-native tooltips only).
- NO HQ widget extensions (peak-hold reset, etc.) — local workarounds
  if needed; record HQ lift candidates in TODO.md.
- NO new mdsp_ui components.
- NO Character / saturator UI (Slice 10).

────────────────────────────────────────
5. COMPONENT ADAPTATION DETAILS
────────────────────────────────────────

### 5.1 `MeterComponent` (local copy)

Start from AnalyzerPro's MeterComponent. Strip any analyzer-engine-specific
hooks. Drive from a simple **snapshot poller** interface — the editor
calls `setLevelDb(float)` (or per-channel pair) on each timer tick.

**Add a GR variant.** AnalyzerPro doesn't ship this; we need it. Options
(pick the cleaner one based on AnalyzerPro's existing design):

- (a) `MeterComponent` gains a `Kind { Level, GainReduction }` enum;
  internal paint logic switches direction (bottom-up for Level, top-down
  for GR), color scale, scale label.
- (b) Subclass: `GainReductionMeterComponent` inheriting from
  `MeterComponent`, overriding paint direction and scale.

I prefer **(a)** — single component, fewer files, cleaner consumer
code. Implementation:

```cpp
enum class MeterKind { Level, GainReduction };

void setKind (MeterKind kind);
void setLevelDb (float db);                  // for Level: input from -inf..0 dB
                                             // for GainReduction: input >= 0 dB (reduction amount)
void setStereoLevelsDb (float l, float r);   // both kinds support stereo
void setRangeDb (float minDb, float maxDb);  // Level: -60..0, GR: 0..20
```

**GR specifics:**
- Two channels (L, R) — both fed the same `currentGrDb_` value in v1
  (the envelope is mono detection; GR is a single channel internally).
  Visually we render two stacked bars so it matches the I/O meter
  layout, but they read the same source. This is honest: it's the same
  GR applied to both channels.
- **Top-down direction.** 0 dB at the top of the bar, growing downward
  as `gr_db` increases. Filled region = how much reduction.
- Color: use the theme's "reduction" / negative / red-ish color token,
  not the same green as level meters.
- Scale ticks (optional): at 3, 6, 12 dB if a scale is drawn.

### 5.2 `MeterGroupComponent` (local copy)

Wraps a set of `MeterComponent`s with a title and numeric readout. Use
as-is from AnalyzerPro with import path adapted. If it depends on
analyzer-specific theme tokens, swap to the equivalent `mdsp_ui::Theme`
tokens available in our `UiContext`.

### 5.3 `LoudnessNumericPanel` (local copy)

Three text rows for Momentary / Short-term / Integrated LUFS. Polls
the processor's `LoudnessAnalyzer::getSnapshot()` on tick. **Fixes the
ASCII bug**: labels are `juce::String("M: ")`, not `juce::String('M') + ": "`
(which is what produced "77:" in Slice 8). Use the AnalyzerPro
implementation as the reference.

### 5.4 Numeric readout ballistics (fix the flicker)

Numeric values change too fast at 30 Hz. Apply a short visual smoother
to each displayed number (separate from the meter bar's own ballistics):

- For peak readouts: peak-hold for 1 s, then exponential decay (~300 ms)
- For LUFS readouts: update at most every 200 ms (so values stay
  legible for at least 5 frames)
- For GR readout: smooth with ~100 ms attack/release (slower than the
  bar, so the bar still snaps but the number is readable)

Implementation: a small `SmoothedValue<float>` per displayed text, ticked
on the editor timer. Don't change the meter bar's ballistics — those
come from `mdsp_ui::meters::MeterValueModel` and shouldn't be tuned in
the product layer.

### 5.5 Peak-hold UX

AnalyzerPro likely has its own peak-hold behavior in `MeterComponent`.
Adopt it. If per-meter click-to-reset is in the AnalyzerPro pattern,
inherit it. If not, a small "Reset Peaks" button (current Slice 8
approach) is acceptable. Record any HQ-side improvement candidate
in TODO.md (e.g. "lift per-meter resetPeakHold to mdsp_ui").

────────────────────────────────────────
6. LAYOUT REDESIGN — MainView + Editor
────────────────────────────────────────

### Editor

Mirror AnalyzerPro's editor wiring (adapt to MasterLimiter scale):

- Use `mdsp_ui::ThemeVariant::Custom` (AnalyzerPro pattern).
- Apply LookAndFeel via `juce::LookAndFeel::setDefaultLookAndFeel(&lnf_)`.
- **Resizable** with min size — propose **min 960 × 540**, default
  **1100 × 620** (aim for the AnalyzerPro family proportions without
  forcing the same large minimum). If your layout naturally fits a
  different size, choose what works for the content; document in TODO.md.
- Keep the existing 30 Hz Timer (Slice 8) — drives both meter widgets'
  snapshot setters and `LoudnessNumericPanel::refresh()`.
- JUCE-native tooltips on the parameter knobs (set tooltip text on each
  slider with a short description). No rich tooltip system.

### MainView layout (proposed; adapt if it looks bad)

```
┌─────────────────────────────────────────────────────────────────────┐
│  Header strip: "MasterLimiter v0.1.0"  (h ~40 px)                   │
├─────────────────────────────────────┬───────────────────────────────┤
│  Parameter area (knobs grid)        │  Meter strip                  │
│                                     │                               │
│  Row 1: Input Gain | Ceiling        │  ┌──────┐ ┌────┐ ┌──────┐    │
│  Row 2: Release | Lookahead         │  │ IN   │ │ GR │ │ OUT  │    │
│  Row 3: Stereo Link | M/S Link      │  │ L  R │ │L  R│ │ L  R │    │
│  Row 4: Ceiling Mode | Char | Auto  │  │  ▆▆  │ │▆▆ ▆▆│ │  ▆▆  │   │
│  Row 5: Release Sustain Ratio       │  │ ▆▆▆▆ │ │▆▆▆▆│ │ ▆▆▆▆ │   │
│         (with Release knob alt)     │  │ -10  │ │ 3.2│ │ -1.0 │    │
│                                     │  └──────┘ └────┘ └──────┘    │
│                                     │  ─────────────────────       │
│                                     │  LUFS                         │
│                                     │  M:  -14.2 LUFS               │
│                                     │  S:  -13.8 LUFS               │
│                                     │  I:  -13.5 LUFS               │
│                                     │                               │
│                                     │  [ Reset Peaks ]              │
└─────────────────────────────────────┴───────────────────────────────┘
```

Proportions: meter strip ~260–300 px wide; parameter area takes
remaining width. Padding/gutters per AnalyzerPro convention (look at
its MainView for spacing values).

### Implementation notes

- Place the three meter groups (`MeterGroupComponent` for Input,
  another for GR, another for Output) in a vertical column.
- `LoudnessNumericPanel` sits below the Output meter group.
- The Reset button at the bottom of the strip.
- Parameter knobs use `juce::Slider::RotaryVerticalDrag` (current Slice 1 idiom)
  with attached labels.
- Make sure the `release_sustain_ratio` knob (added in Slice 5) gets a
  proper label "Release × Ratio" or "Sustain Ratio" — clearer than the
  default. (This is just a knob label, NOT the APVTS parameter name.)

────────────────────────────────────────
7. PROCESSOR PASS-THROUGH (no change)
────────────────────────────────────────

`PluginProcessor.{h,cpp}` is not modified in this slice. The Slice 8
snapshots (atomics + LoudnessAnalyzer) stay exactly as committed.
UI just consumes them differently.

────────────────────────────────────────
8. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```

Zero new warnings from §4 files.

### Bench regression (DSP must not have changed)

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE08_1_REGRESSION_S$SLICE"
done
```

All three must continue to PASS unchanged. If any fails, you've
accidentally touched DSP — STOP.

────────────────────────────────────────
9. `docs/TODO.md` — HQ lift candidates
────────────────────────────────────────

Create `docs/TODO.md` in the product repo with at least these entries
(add more as you discover them while implementing):

```markdown
# MasterLimiter — UI HQ-lift candidates

Items copied locally from AnalyzerPro for Slice 8.1. To be lifted into
`melechdsp-hq/shared/mdsp_ui/` in a future HQ-refactor slice so future
products inherit.

- `Source/ui/meters/MeterComponent.{h,cpp}` (with GR variant added here)
- `Source/ui/meters/MeterGroupComponent.{h,cpp}`
- `Source/ui/loudness/LoudnessNumericPanel.{h,cpp}`
- Per-meter `resetPeakHold()` on `mdsp_ui::meters::widgets::LevelMeter`
  (currently no public API; Slice 8 workaround was widget recreation,
  Slice 8.1 may inherit the workaround or use AnalyzerPro's local
  reset mechanism).
- True-peak measurement in the snapshot path (Slice 8 uses
  max-sample-peak approximation for `outputTpDb_`).
- GR ballistics tuning — `mdsp_ui::meters::GainReductionRenderStateProvider`
  is hard-coded at 10 ms / 150 ms; Q8-4 asked for 0 ms / 200 ms. Defer.

These are intentional debt items, not bugs. Each gets its own slice
when we have a second product needing them.
```

────────────────────────────────────────
10. GATE CHECKLIST
────────────────────────────────────────

- [ ] Product builds clean in Debug and Release; no warnings from §4 files.
- [ ] HQ not modified.
- [ ] AnalyzerPro not modified.
- [ ] Bench Slice 3 / 4 / 5 PASS unchanged.
- [ ] Plugin loads in Live as VST3 and AU.
- [ ] LUFS labels show "M:", "S:", "I:" (not "77:", "83:", "73:").
- [ ] GR meter is **top-down** with 0 dB at top, fills downward.
- [ ] GR meter has **L and R bars** (both reading the same source for v1).
- [ ] Numeric readouts no longer flicker — values readable at a glance.
- [ ] Layout proportions: meters don't crowd parameters; parameters
      don't crowd meters; no overlap; sane padding.
- [ ] Theme + LookAndFeel applied via `ThemeVariant::Custom` (AnalyzerPro
      pattern).
- [ ] Editor resizable, with sensible min size.
- [ ] Parameter knobs have JUCE-native tooltips with short descriptions.
- [ ] Visual hierarchy reads cleanly: where to find each control,
      where to find each meter.
- [ ] Peak-hold reset works (button or per-meter click — your call).
- [ ] `docs/TODO.md` exists with HQ-lift candidates.

────────────────────────────────────────
11. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. RETRIEVAL LOG (§1 format).
2. File list created / modified, one-line purpose each.
3. Build summary (HQ + product, both configs, warnings).
4. Bench regression: invocation + final lines for Slice 3 / 4 / 5
   (must each end `PASS N/N`).
5. Text description of the editor layout you produced: editor size
   chosen, meter strip width, parameter grid layout, where the LUFS
   panel sits, theme defaults vs custom tweaks.
6. Self-check against §10.
7. Open questions.

Architect will audition in Live after Cursor reports. Slice closes on
audition pass.

────────────────────────────────────────
12. POST-SLICE NOTES (architect)
────────────────────────────────────────

When this slice passes audition:
- Commit both prior Slice 8 work and Slice 8.1 polish in one Slice 8
  close commit (or two clean commits — architect chooses at close time).
- Update PROGRESS with the polish narrative and the v1.1 backlog
  (TODO.md items).
- Slice 7 interview begins (stereo / M-S link).
