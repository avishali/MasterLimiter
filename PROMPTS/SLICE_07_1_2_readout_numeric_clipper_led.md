# Cursor Task — Slice 7.1.2: always-numeric readout + Clipper LED + output click detector

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only changes bundling three items.** No HQ source. No bench
> impact. No push.
>
> 1. **Mojibake idle readout** (`â€` sequence) — sidestep by always
>    showing numeric (`0.0 / 0.0`). avishali preference confirmed.
>
> 2. **Clipper LED overlaps readout text.** Relocate.
>
> 3. **Output click detector** — 7.1.1 timing watermark showed
>    Blk max 165 µs (3% of 5333 µs buffer budget at 48k/256), so the
>    audio thread is NOT starving. The pop must be from a sample-value
>    discontinuity in our output, an OS-preempted allocation/lock we
>    can't time, or a host-side issue outside our plugin. Add an
>    output-side delta detector and bounds check to find out which.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Product
UI files only this slice. No HQ source edits. No bench edits. No
processor / DSP changes. No push. No amend of any prior commit.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

### 1.1 Mojibake idle readout

The screenshot shows `â€  / 2.6` in the GR readout left value and
`Clip â€  / â€` in the Clipper readout. The `â€` sequence is the
classic UTF-8-as-Latin-1 misinterpretation of U+2014 EM DASH (encoded
as `0xE2 0x80 0x94`). Possible causes:
- Cursor's 7.1.1 substitution missed one occurrence.
- File saved with a BOM or encoding mismatch that breaks the literal.
- Build chain re-encoding string literals incorrectly.

Rather than chase the encoding bug, **drop the dash idea entirely**:
when current and/or max are zero, show the numeric value (`0.0`).
This sidesteps any character-encoding issue, matches avishali's
stated preference ("should be 0.0"), and is more informative (a user
glancing at the meter sees a value, not a placeholder).

### 1.2 Clipper LED overlap

The Slice 9.3a Clipper indicator placed a small round LED next to
the readout text. The 7.1 / 7.1.1 readout-style change (compact dual
format) shifted the readout's bounding box, and the LED now sits on
top of the text region.

Move the LED out of the text region. Options (Cursor picks the
cleaner of the two given existing layout):
- Right end of the readout line: `Clip 0.0 / 0.0  ●` with the LED
  positioned at the far-right edge of the readout row, with a small
  margin separating it from the max value text.
- Above the knob area near the "Clipper" label: small LED placed in
  the header region of the Clipper knob, away from the readout
  entirely.

Either placement is fine as long as the LED never overlaps the
readout text at any standard plugin scale.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (product only):**
- `Source/ui/meters/GainReductionMeter.cpp` — readout formatting:
  always numeric, no dash logic, no special idle text.
- `Source/ui/MainView.cpp/.h` — Clipper readout same numeric
  treatment; Clipper LED relocation away from readout text region;
  display the new output-click watermarks alongside the existing
  timing watermarks (or DBG log per Cursor's earlier 7.1.1 choice).
- `Source/PluginProcessor.h` — add output-click atomics + getters +
  reset method.
- `Source/PluginProcessor.cpp` — output-side delta detector and
  bounds check at the very end of processBlock, after all DSP work
  and before final return.

**DO NOT TOUCH:**
- `Source/ui/meters/GainReductionMeter.h` — only modify if the
  readout fix requires a member or method addition; otherwise leave.
- HQ source.
- Bench files.
- Any param.
- limiter_active button position — relocation from 7.1.1 confirmed
  correct in audition.
- The deferred `ms_link_pct` knob's UI presence — leave as-is.
- The 7.1.1 timing probe — stays in place; this slice adds to it.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

### 3.1 GainReductionMeter — always-numeric readout

Locate the readout text construction in
`Source/ui/meters/GainReductionMeter.cpp`. From Slice 7.1 the format
was:

```cpp
const auto current = (displayCurrentGrDb_ > 0.0f) ? formatDbBare (displayCurrentGrDb_) : juce::String ("—");
const auto maxVal  = (displayMaxGrDb_     > 0.0f) ? formatDbBare (displayMaxGrDb_)     : juce::String ("—");
const auto readoutText = current + " / " + maxVal;
```

(or similar — the actual code may differ; identify the dash branch
and the value formatting.)

Replace with always-numeric:

```cpp
const auto current = formatDbBare (displayCurrentGrDb_);  // returns "0.0" when value is 0
const auto maxVal  = formatDbBare (displayMaxGrDb_);
const auto readoutText = current + " / " + maxVal;
```

If `formatDbBare` doesn't already render "0.0" for zero (some helpers
return empty string for zero), update it OR use
`juce::String (val, 1)` directly to force one decimal place.

Result: idle GR readout reads `"0.0 / 0.0"`. Under reduction it
reads e.g. `"3.2 / 5.1"`.

No em-dash, no ASCII hyphen, no placeholder text — pure numeric.

### 3.2 Clipper readout — always-numeric

In `Source/ui/MainView.cpp` (the Clipper readout block from Slice
9.3a + 7.1.1):

Replace any conditional dash logic with:

```cpp
const auto current = formatDbBare (clipDbDisplay);   // "0.0" when zero
const auto maxVal  = formatDbBare (clipMaxDbDisplay);
const auto clipText = juce::String ("Clip ") + current + " / " + maxVal;
```

Result: idle Clipper readout reads `"Clip 0.0 / 0.0"`. Under push it
reads e.g. `"Clip 1.2 / 3.0"`.

### 3.3 Output click detector

In `PluginProcessor.h`:

```cpp
std::atomic<float>   outputMaxDeltaSeen_ { 0.0f };  // max |s[i+1] - s[i]| ever seen
std::atomic<float>   outputMaxAbsSeen_   { 0.0f };  // max |s| ever seen (catches NaN/Inf as inf or huge)
std::atomic<int64_t> outputClickCount_   { 0 };     // count of blocks where any delta > kClickThreshold

float   getOutputMaxDeltaSeen() const noexcept { return outputMaxDeltaSeen_.load (std::memory_order_relaxed); }
float   getOutputMaxAbsSeen()   const noexcept { return outputMaxAbsSeen_.load   (std::memory_order_relaxed); }
int64_t getOutputClickCount()   const noexcept { return outputClickCount_.load   (std::memory_order_relaxed); }

void resetOutputClickStats() noexcept
{
    outputMaxDeltaSeen_.store (0.0f, std::memory_order_relaxed);
    outputMaxAbsSeen_.store   (0.0f, std::memory_order_relaxed);
    outputClickCount_.store   (0,    std::memory_order_relaxed);
}
```

In `PluginProcessor.cpp`, at the very end of processBlock (after all
DSP work, after loudness analyzer, just before final return):

```cpp
// Output click detector — catches sample-value discontinuities
// regardless of CPU timing. A real audible click is typically a
// sample-to-sample delta > 0.3-0.5 in linear; NaN/Inf push
// outputMaxAbsSeen_ to a huge value.
constexpr float kClickThreshold = 0.5f;
float blockMaxDelta = 0.0f;
float blockMaxAbs   = 0.0f;
bool  blockHasClick = false;

for (int ch = 0; ch < nch; ++ch)
{
    const float* d = buffer.getReadPointer (ch);
    const int    n = buffer.getNumSamples();
    float prev = d[0];
    if (std::abs (prev) > blockMaxAbs) blockMaxAbs = std::abs (prev);
    for (int i = 1; i < n; ++i)
    {
        const float cur   = d[i];
        const float dlt   = std::abs (cur - prev);
        const float absCur = std::abs (cur);
        if (dlt    > blockMaxDelta) blockMaxDelta = dlt;
        if (absCur > blockMaxAbs)   blockMaxAbs   = absCur;
        if (dlt    > kClickThreshold) blockHasClick = true;
        prev = cur;
    }
}

const float prevDelta = outputMaxDeltaSeen_.load (std::memory_order_relaxed);
if (blockMaxDelta > prevDelta)
    outputMaxDeltaSeen_.store (blockMaxDelta, std::memory_order_relaxed);

const float prevAbs = outputMaxAbsSeen_.load (std::memory_order_relaxed);
if (blockMaxAbs > prevAbs)
    outputMaxAbsSeen_.store (blockMaxAbs, std::memory_order_relaxed);

if (blockHasClick)
    outputClickCount_.fetch_add (1, std::memory_order_relaxed);
```

This loop is per-sample but cheap (a subtract, abs, two compares per
sample per channel). At 48k stereo with a 256-sample block it's
~512 abs+compares per block, well under the existing block timing
budget.

Surface in the same debug readout as the 7.1.1 timing probe:

```
Blk 165 | Drv 6 / Up 103 / Clp 9 / Env 125 / GMul 21 / Dn 28 / Out 11 us
Clicks 0 | MaxΔ 0.04 | MaxAbs 0.89
```

(Or the same DBG log fallback Cursor used in 7.1.1.) Reset via
`resetOutputClickStats()` exposed alongside `resetAudioBlockMaxUs()`.

### 3.4 Clipper LED relocation

Locate the LED's current bounding rectangle (the 8 px round
`theme.warning` indicator with the ~200 ms decay). Identify its
current position relative to the readout text.

Move it to one of:

**Option A: right-edge inline with readout text.** Make the readout
slot wider (or trim the text horizontal padding) so there's room at
the far right for the LED:

```
[Clip 0.0 / 0.0]    [●]
```

Position the LED at `readoutBounds.getRight() + small_margin`. Keep
the LED's existing draw code, just change its bounds rectangle.

**Option B: above the readout in the Clipper knob's header area.**
Position the LED near the "Clipper" label, completely separated from
the readout:

```
Clipper ●
[ knob ]
Clip 0.0 / 0.0
```

The LED sits in the label area's right edge, far from any readout
text.

Cursor picks based on the current layout — whichever placement
preserves the panel's overall aesthetic and doesn't require resizing
the knob block. Either is fine functionally.

Ensure the LED's hit region (if any — currently non-interactive per
7.1) doesn't overlap the readout's click-to-reset hit region.

────────────────────────────────────────
4. BUILD
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean.

────────────────────────────────────────
5. NO BENCH RUN
────────────────────────────────────────

UI text and layout only. No DSP / atomic / param change. No bench
impact.

────────────────────────────────────────
6. PRODUCT COMMIT (separate, no push)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect:
#    M Source/PluginProcessor.h
#    M Source/PluginProcessor.cpp
#    M Source/ui/meters/GainReductionMeter.cpp
#    M Source/ui/MainView.cpp
#    M Source/ui/MainView.h    (if LED relocation or click-stats UI needed it)

git add Source/PluginProcessor.h Source/PluginProcessor.cpp \
        Source/ui/meters/GainReductionMeter.cpp Source/ui/MainView.*

git commit -m "$(cat <<'EOF'
MasterLimiter Slice 7.1.2: numeric readouts, Clipper LED, output click detector

Three fixes from the Slice 7.1.1 audition:

1. Always-numeric readouts.
   - Slice 7.1's em-dash idle text rendered as mojibake ("â€"
     sequence) in the shipped build, indicating a font glyph
     gap and/or a source-encoding chain issue. Sidestepped by
     dropping the dash idea entirely: GR reads "0.0 / 0.0",
     Clipper reads "Clip 0.0 / 0.0" at idle. Numeric is more
     informative and immune to glyph/encoding issues.

2. Clipper LED relocation.
   - The 8 px round Clipper LED from Slice 9.3a was overlapping
     the readout text after the 7.1 readout-style change shifted
     the text bounding box. Moved to a non-overlapping position
     adjacent to but separate from the readout.

3. Output click detector.
   - Slice 7.1.1's per-section timing watermarks showed block max
     of 165 us, ~3% of the 5333 us buffer budget at 48k/256: the
     audio thread is not starving. The pop must therefore come
     from a sample-value discontinuity in our output, an OS-preempted
     allocation/lock the timing probe can't see, or a host-side
     issue outside our processBlock.
   - Added an output-side detector that runs at the very end of
     processBlock and computes per-block max sample-to-sample
     delta and max |sample|, plus a click counter (blocks where
     any delta exceeds 0.5). Atomic high-water-marks exposed via
     getters; reset via resetOutputClickStats(). Surfaced
     alongside the 7.1.1 timing watermarks in the same debug
     readout / DBG log.
   - Cost: ~512 abs+compare ops per block at 48k/256 stereo;
     negligible vs the existing budget.

Diagnostic outcome:
- Clicks > 0 with magnitudes correlating to audible pops → DSP
  glitch in our chain; we target the source in a follow-up slice.
- Clicks = 0 and audible pops persist → our plugin output is
  clean; pop is host-side or system-wide and not addressable in
  our code.

Product-only changes. No HQ / params / bench. Slice 7.1.1's
timing probe stays in place.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push.** Slice 7 close will cover 7 + 7.1 + 7.1.1 + 7.1.2 +
any pop-fix slice once everything's resolved and auditioned.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. GainReductionMeter diff: dash logic removed, formatDbBare always
   called, idle text confirmed as "0.0 / 0.0".
2. MainView diff: Clipper readout dash logic removed, always
   numeric, LED relocation specifics (which option Cursor chose).
3. Build success lines for Release and Debug.
4. Product `git log --oneline -5` showing this commit on top of the
   7.1.1 commit.
5. Confirmation: HQ NOT touched; product NOT pushed; bench NOT run;
   no params; no processor changes.
6. Open questions.

────────────────────────────────────────
8. AFTER THIS
────────────────────────────────────────

avishali audition:
- GR readout idle: should now show `"0.0 / 0.0"`.
- Clipper readout idle: should now show `"Clip 0.0 / 0.0"`.
- Clipper LED: visible, decays as before, no overlap with readout
  text.
- Pop investigation continues in parallel — when avishali shares the
  watermark numbers from 7.1.1's timing probe, the next slice
  targets whichever section is spiking.

On UI visuals confirmed AND pop diagnosed → final pop-fix slice
(7.1.3), then revised Slice 7 close prompt covers all 7.x slices
in one ship.
