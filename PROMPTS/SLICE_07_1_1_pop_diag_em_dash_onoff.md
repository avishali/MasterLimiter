# Cursor Task — Slice 7.1.1: CPU pop diagnostic + em-dash glyph fix + on/off button relocation

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only fixes + audio-thread instrumentation. No HQ source
> change. No bench impact.** Bundles three issues surfaced after Slice
> 7.1 audition:
>
> 1. **CPU pop glitch** during playback even at default link = 100,
>    with correlated CPU spikes. Need instrumentation to find which
>    processBlock section spikes. Defensive fix included for the
>    envelope_R_ state-divergence hypothesis (warm-keep).
>
> 2. **"— / —" idle readout shows a tofu glyph** in the GR meter and
>    Clipper readout. JUCE's default font is missing the em-dash glyph.
>    Replace with ASCII hyphen.
>
> 3. **limiter_active on/off button obstructs the now-wider GR meter**
>    (2-bar version is wider than the original 1-bar). Relocate the
>    button to a position above and between the Gain (`input_gain_db`)
>    and Ceiling (`ceiling_db`) knobs.
>
> Product-only commit. No push from this slice — Slice 7 close prompt
> will cover 7 + 7.1 + 7.1.1 together once 7.1.1 ships and audition
> confirms pops are addressed.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Product
files only this slice. No HQ source edits. No bench edits. No product
push. No amend of `f05c062` (Slice 7) or `9ca9d61` (Slice 7.1) — this
slice stacks on top.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

### 1.1 Pop investigation

avishali reports random pops at default link = 100 (i.e., during the
single-envelope fast-path, NOT during link sweep). DAW CPU meter shows
correlated momentary spikes. This rules out the link-transition
hypothesis (envelope_R_ stale state at the fast↔full threshold).

Candidate audio-thread spike sources:
- An occasional expensive branch in processBlock (some lookahead /
  envelope rebalancing on certain inputs).
- Atomic contention with the UI thread reading multiple atomics.
- The new latched-max comparison loop accidentally calling a
  non-RT-safe function.
- Cache-pressure: envelope_R_'s 4× OS state (ext_ at 960 samples,
  historyPost_ at 960, attackTable_ at 960 — ~12 KB per envelope,
  ~24 KB total + the lookahead and gain buffers) might evict L1/L2
  cache lines used by audio-thread hot code intermittently.
- Repaint pressure from the new 2-bar GR meter starving audio
  (unlikely on macOS but possible).

We instrument first (max-time-per-processBlock over a rolling window,
exposed as an atomic readable from the UI for live observation), then
reason from the numbers.

Defensive: also implement envelope_R_ warm-keep — run envelope_R_ on
the R-channel peak stream even in fast-path mode, discarding its
gain output. Eliminates the state-divergence pop source as a side
effect, regardless of whether it's the random-pop culprit. Cost: ~2×
envelope CPU at default link = 100 (mitigated by Slice 9.4.1's inner-
loop fast-path which skips non-peak samples; net cost typically only
+10–30% on real material).

### 1.2 Em-dash glyph fix

Slice 7.1's `"— / —"` idle text uses the U+2014 EM DASH character.
JUCE's default font (the one the readout component uses) is missing
this glyph, so it renders as a tofu rectangle. Replace with ASCII
`"- / -"` (hyphen-minus).

### 1.3 limiter_active relocation

The Maximizer panel header's on/off button visually crowds the
now-wider 2-bar GR meter. avishali specifies: relocate it to "between
and above the Gain and Ceiling knobs" — i.e., positioned above the
knob row where Drive and Ceiling sit, centered between their
horizontal positions.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (product only):**
- `Source/PluginProcessor.h` — add audio-thread timing probe atomic
  + getter; envelope_R_ warm-keep is a code change in .cpp, no new
  fields needed.
- `Source/PluginProcessor.cpp` — instrument processBlock with
  `std::chrono::steady_clock`-based timing; rolling max over a 1-second
  window stored as `audioBlockMaxUs_`. Implement warm-keep for
  envelope_R_ in fast-path.
- `Source/ui/meters/GainReductionMeter.cpp` — `"— / —"` → `"- / -"`.
- `Source/ui/MainView.cpp/.h` — `"— / —"` in Clipper readout → `"- / -"`;
  limiter_active button relocation to above-between Gain and Ceiling
  knobs.
- (Optional) `Source/ui/MainView.cpp` — small UI display of
  `audioBlockMaxUs_` (e.g., in a header text field or a debug overlay
  togglable by a key combo) for live observation during audition. If
  adding a debug UI would disturb the layout, log to `juce::Logger`
  via `DBG()` instead and show no UI — Cursor picks the lower-friction
  option.

**DO NOT TOUCH:**
- `shared/mdsp_dsp/...` — HQ DSP source.
- Bench files.
- Any param ID, range, default. No new params.
- Input / output peak meters or other meter components.
- Param surface in general — this is a polish + instrumentation slice.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

### 3.1 Audio-thread timing probe

In `PluginProcessor.h`:

```cpp
std::atomic<int64_t> audioBlockMaxUs_ { 0 };

int64_t getAudioBlockMaxUs() const noexcept
{
    return audioBlockMaxUs_.load (std::memory_order_relaxed);
}

void resetAudioBlockMaxUs() noexcept
{
    audioBlockMaxUs_.store (0, std::memory_order_relaxed);
}
```

In `processBlock` (top of function):

```cpp
const auto blockStart = std::chrono::steady_clock::now();
```

At the very bottom of `processBlock` (after loudness analysis):

```cpp
const auto blockEnd = std::chrono::steady_clock::now();
const auto blockUs  = std::chrono::duration_cast<std::chrono::microseconds>
                          (blockEnd - blockStart).count();

// Rolling 1-second max: reset whenever the wall-clock window rolls.
// Simpler approach: hold max, reset only on explicit reset OR if a
// new block exceeds prior max. The "max-ever-seen since reset" is the
// most useful watermark for catching spikes.
const int64_t prev = audioBlockMaxUs_.load (std::memory_order_relaxed);
if ((int64_t) blockUs > prev)
    audioBlockMaxUs_.store ((int64_t) blockUs, std::memory_order_relaxed);
```

For per-section timing during the diagnostic, add intermediate
timestamps:

```cpp
const auto t0 = std::chrono::steady_clock::now();    // start
// ... drive at 1× ...
const auto t1 = std::chrono::steady_clock::now();    // post-drive
// ... upsample ...
const auto t2 = std::chrono::steady_clock::now();    // post-upsample
// ... clipper + peak detect ...
const auto t3 = std::chrono::steady_clock::now();    // post-clipper-peak
// ... envelope_L_ (+ envelope_R_ in full-path or warm-keep) ...
const auto t4 = std::chrono::steady_clock::now();    // post-envelope
// ... lookahead + gain multiply ...
const auto t5 = std::chrono::steady_clock::now();    // post-gain-mul
// ... downsample ...
const auto t6 = std::chrono::steady_clock::now();    // post-downsample
// ... output gain + metering + loudness ...
const auto t7 = std::chrono::steady_clock::now();    // end
```

Compute section deltas (us) and store per-section max in additional
atomics:

```cpp
std::atomic<int64_t> sectionMaxUsDrive_       { 0 };
std::atomic<int64_t> sectionMaxUsUpsample_    { 0 };
std::atomic<int64_t> sectionMaxUsClipperPeak_ { 0 };
std::atomic<int64_t> sectionMaxUsEnvelope_    { 0 };
std::atomic<int64_t> sectionMaxUsGainMul_     { 0 };
std::atomic<int64_t> sectionMaxUsDownsample_  { 0 };
std::atomic<int64_t> sectionMaxUsOutput_      { 0 };
```

Update each via the same "max-watermark" pattern as
`audioBlockMaxUs_`.

Output mechanism for avishali's audition:
- **Preferred**: small debug text in the plugin header / footer
  showing the seven max values in microseconds, refreshed at the
  meter timer rate. Format example:
  `"Block 152 | Drv 2 / Up 18 / Clp 41 / Env 67 / GMul 14 / Dn 16 / Out 3 us"`
- **Fallback**: `DBG()` log line every N blocks (e.g., every 100
  blocks) so the timing prints to Console.app / Xcode log without
  disturbing UI layout.

Cursor picks the lower-friction option given the current UI density.
The debug readout can be removed after we identify the spike source.

### 3.2 envelope_R_ warm-keep

In the fast-path branch of processBlock (link ≥ 99.95), after the
existing `envelope_L_` processing, ALSO run envelope_R_ on the R
channel's peak stream and discard its gain output:

```cpp
if (fastPath)
{
    // Existing: single envelope on max(|L|,|R|), gain applied to both.
    auto* peak = peakBuf_.getWritePointer (0);
    const float* const osPtrs[2] = { osCh0, osCh1 };
    for (int i = 0; i < osN; ++i)
        peak[i] = peakDetector_.process (osPtrs, nch, i);

    auto* gain = gainBuf_.getWritePointer (0);
    envelope_L_.process (peak, gain, osN);

    // NEW: warm-keep envelope_R_ on R-channel peak stream. Output
    // discarded — fast-path applies envelope_L_'s gain to both
    // channels. This keeps envelope_R_'s internal state coherent so
    // transitions into full-path (link < 99.95) don't snap.
    auto* peakR = peakBufR_.getWritePointer (0);
    for (int i = 0; i < osN; ++i)
        peakR[i] = std::abs (osCh1[i]);

    envelope_R_.setThresholdLinear (thresholdLin);
    envelope_R_.setReleaseMs (readFloatParam (apvts, "release_ms"));
    envelope_R_.setReleaseSustainRatio (releaseSustainRatio_->get());
    envelope_R_.setMode (mapCharacterIndexToMode (characterChoice_->getIndex()));

    auto* gainR = gainBufR_.getWritePointer (0);
    envelope_R_.process (peakR, gainR, osN);   // output discarded

    // Apply envelope_L_'s gain to both channels (unchanged).
    for (int ch = 0; ch < nch; ++ch)
    {
        auto* d = osBlock.getChannelPointer ((size_t) ch);
        for (int i = 0; i < osN; ++i)
        {
            const float delayed = lookahead_.pushPop (ch, d[i]);
            d[i] = delayed * gain[i];
        }
    }

    // currentGrLDb_/currentGrRDb_ still both = envelope_L_'s GR
    // (matches what's actually applied to the signal).
    const float gr = envelope_L_.getLastBlockMaxGrDb();
    currentGrDb_.store  (gr, std::memory_order_relaxed);
    currentGrLDb_.store (gr, std::memory_order_relaxed);
    currentGrRDb_.store (gr, std::memory_order_relaxed);
}
```

Latched max update (after the if/else) and clipper logic are
unchanged.

### 3.3 Em-dash → ASCII hyphen

In `Source/ui/meters/GainReductionMeter.cpp`, locate the readout text
construction. Wherever the U+2014 EM DASH character appears in a
string literal, replace it with ASCII `-` (U+002D HYPHEN-MINUS).

Specifically:
- `"— / —"` → `"- / -"`
- Any other em-dash usage → ASCII hyphen.

In `Source/ui/MainView.cpp` (Clipper readout block from Slice 9.3a +
Slice 7.1):
- `"Clip — / —"` → `"Clip - / -"`
- Any other em-dash → ASCII hyphen.

The visual result is functionally identical (a dash character) but
renders with the available glyph.

### 3.4 limiter_active button relocation

Find the limiter_active toggle button's current placement in MainView
(likely in the Maximizer panel header area; locate by searching for
`limiterActive` or the on/off param attachment).

Move it to a new position:
- Above the knob row where `input_gain_db` (Drive / Gain) and
  `ceiling_db` (Ceiling) knobs sit.
- Horizontally centered between those two knobs' x-positions.

Approximate layout sketch (varies with current MainView structure):

```
+-------------------------------------+
| I/O panel                           |
|                                     |
|  Gain knob              Ceiling knob|
|  [  Drive  ]    [ON/OFF]   [  Ceil ]|
|                                     |
+-------------------------------------+
```

The on/off button sits above and between the two knobs. Compact —
matches the existing button size and styling from Slice 9.

Remove the button from the Maximizer panel header (its previous
location). Verify no visual gap is left in the Maximizer header; if
removing exposes empty space, recenter the remaining header content.

────────────────────────────────────────
4. BUILD
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean, no new warnings.

────────────────────────────────────────
5. BENCH (no rerun)
────────────────────────────────────────

No DSP behaviour change. Warm-keep adds CPU but produces identical
audio output (envelope_R_'s gain is discarded in fast-path; envelope_L_
drives both channels exactly as in Slice 7). Slice 3/4/5 PASS
unchanged at default link = 100; no rerun required.

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
#    M Source/ui/MainView.h     (if hit-region or member added)

git add Source/PluginProcessor.h Source/PluginProcessor.cpp \
        Source/ui/meters/GainReductionMeter.cpp \
        Source/ui/MainView.cpp Source/ui/MainView.h

git commit -m "$(cat <<'EOF'
MasterLimiter Slice 7.1.1: pop diag, em-dash fix, on/off relocation

Three Slice 7.1 audition follow-ups bundled:

1. CPU pop diagnostic + envelope_R_ warm-keep.
   - Added audio-thread timing probe in processBlock: a per-block
     watermark for total duration and per-section deltas (drive,
     upsample, clipper+peak, envelope, gain multiply, downsample,
     output+meter). Exposed via atomics + getters; debug readout in
     UI (or DBG log fallback) for live observation during audition.
     Watermarks reset via resetAudioBlockMaxUs(). Removable after
     spike source identified.
   - envelope_R_ now runs on R-channel peak stream in the fast-path
     branch (link >= 99.95) with its gain output discarded. Keeps
     envelope_R_ state coherent so transitions into full-path don't
     snap. Defensive fix for the link-transition pop hypothesis even
     if the random link=100 pops are caused by something else.
   - Cost: ~2x envelope CPU at default link = 100. Mitigated by
     Slice 9.4.1's outer-loop fast-path which skips non-peak samples.

2. Em-dash glyph fix.
   - JUCE default font is missing U+2014 EM DASH, so Slice 7.1's
     idle readouts ("— / —") render as tofu boxes. Replaced with
     ASCII hyphen ("- / -") in GainReductionMeter and the Clipper
     readout block in MainView.

3. limiter_active button relocation.
   - The 2-bar GR meter from Slice 7.1 is wider than the original
     1-bar; the on/off button in the Maximizer panel header was
     visually crowding it. Moved the button to a new position above
     and between the Drive (input_gain_db) and Ceiling (ceiling_db)
     knobs in the I/O panel area.

No HQ source, no params, no bench impact. Default link = 100 produces
bit-exact same output as Slice 7.1 (warm-keep's envelope_R_ gain is
discarded). Slice 3/4/5 PASS unchanged.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push.** Slice 7 close prompt will cover 7 + 7.1 + 7.1.1
together.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Header diff: new atomics, getters, reset method.
2. processBlock diff: timing probe insertion points, warm-keep
   envelope_R_ in fast-path.
3. GainReductionMeter / MainView diff summaries: em-dash → hyphen
   substitutions (count + locations).
4. MainView diff: limiter_active button relocation — before/after
   placement.
5. Decision: debug readout in UI vs DBG log — which one Cursor chose
   and where it lives.
6. Build success lines for Release and Debug.
7. Product `git log --oneline -4` showing the new commit on top of
   `9ca9d61`.
8. Confirmation: HQ NOT touched; product NOT pushed; bench NOT run;
   no params added or modified.
9. Open questions.

────────────────────────────────────────
8. AFTER THIS — audition + diagnostic readout interpretation
────────────────────────────────────────

avishali auditions in Live (Release VST3) at default link = 100 with
the same source that produced the pop. Watches the per-section
microsecond watermarks (UI readout or Console log) and reports:

- **Does the pop persist?** If yes, which section's watermark spikes
  when the pop occurs? Typical block-time budget at 48k / 256-sample
  buffer is ~5333 us; any single section > 1000 us is suspect, any
  block total > 4000 us risks underrun.
- **Did the pop disappear after warm-keep?** If yes, the link-
  transition hypothesis was correct (envelope_R_ stale-state); we
  ship warm-keep as the permanent fix.
- **Em-dash readout**: idle readouts should now show `"- / -"`
  instead of tofu boxes.
- **On/off button**: positioned above-between Drive and Ceiling
  knobs, no longer crowding the GR meter.

Three outcomes:

**A. Pop gone, em-dash + on/off fixed.** → Slice 7 close prompt
   revised to cover 7 + 7.1 + 7.1.1 together. Remove the timing
   probe in the close commit (or keep as a permanent debug aid —
   avishali's call).

**B. Pop persists, watermark identifies the spike section.** →
   targeted fix slice based on which section spikes (e.g., if
   `sectionMaxUsEnvelope_` is the culprit, investigate envelope's
   inner branches; if `sectionMaxUsDownsample_`, the OS chain;
   etc.). Slice 7.1.2.

**C. Pop persists but watermark shows no spike.** → audio thread
   spike is in something the probe isn't measuring (e.g., the
   loudness analyzer, or memory allocation from another component).
   Widen instrumentation to cover IO trims, metering, loudness, and
   any deferred callbacks. Slice 7.1.2 with broader scope.
