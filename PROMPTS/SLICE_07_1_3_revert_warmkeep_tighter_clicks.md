# Cursor Task — Slice 7.1.3: revert envelope_R_ warm-keep + tighter click threshold + per-click section log

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only investigation continuation. No HQ source. No bench
> impact. No push.**
>
> The 7.1.2 readout confirmed:
> - Audio thread isn't starving (Blk 1059 µs, 20% of 5333 µs budget at
>   48k/256).
> - No catastrophic output values (Abs 0.95, no NaN/Inf).
> - Clicks 0 at the 0.5 threshold, but D 0.45 is suspiciously close —
>   subtler clicks below 0.5 may be the audible pops.
> - Env section jumped 125 → 845 µs after 7.1.1's warm-keep, with no
>   evidence the warm-keep was preventing actual pops (random pops at
>   link = 100 were never link-transition-related; envelope_R_ wasn't
>   even being used at link = 100 before warm-keep).
>
> Three changes this slice:
>
> 1. **Revert envelope_R_ warm-keep** in fast-path. envelope_R_ runs
>    only in the full-path (link < 99.95) again, as in Slice 7.0.
>
> 2. **Lower the click threshold from 0.5 to 0.2.** Catches subtler
>    output discontinuities that may be the audible pops.
>
> 3. **Per-click event log** — when a block has a click event,
>    `DBG()` a single line including the section timings for that
>    block so we can see what spiked at the moment of pop. Click
>    counter and high-water-marks remain in the UI debug header as
>    before.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Product
files only. No HQ source. No bench source. No params. No push. No
amend.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

### 1.1 Revert warm-keep

7.1.1's warm-keep ran envelope_R_ on the R-channel peak stream in
fast-path mode, discarding its gain output. Defensive measure for
the link-transition-into-full-path hypothesis: envelope_R_ state
diverges during fast-path → snap at the transition.

Empirical evidence after 7.1.2:
- avishali reports random pops at default link = 100. The fast-path
  is in effect; envelope_R_ wouldn't be exercised by a transition
  even without the warm-keep.
- Output click counter at 0 (at threshold 0.5) AND Abs 0.95 →
  output is mathematically clean → envelope_R_ state divergence is
  NOT producing audible glitches.
- Env section spiked from 125 µs (pre-warm-keep) to 845 µs (post)
  with no proven pop-prevention benefit.

Conclusion: warm-keep is paying CPU for a hypothesis that doesn't
match the data. Revert.

If link-sweep pops surface as a separate issue later, address with
a one-shot state-copy `envelope_R_ ← envelope_L_` at the moment of
transition (cheaper, cleaner) — that's a different slice when it
matters.

### 1.2 Tighter click threshold

D = 0.45 high-water-mark with Clicks = 0 at threshold 0.5 means the
detector almost-but-not-quite caught something. A click at the
output of ~0.45 inter-sample-delta is audible — that's roughly a
−7 dB transient between adjacent samples, far steeper than music
typically produces in a continuous passage.

Lower threshold to **0.2** to catch subtler discontinuities. A
0.2 sample-to-sample delta is about −14 dB transient — still well
above steady-state music content but capable of catching genuine
clicks that 0.5 missed.

If after the revert + tighter threshold the click counter still
reads 0 during pops, the pop is conclusively NOT in our output and
we move toward host-side / system-side investigation.

### 1.3 Per-click section log

When a block triggers a click, we want to know **what was happening
in that block** — which section spiked, what the input was like,
what state we were in.

Add a `DBG()` log line on every block where `blockHasClick == true`,
formatted as:

```
ML CLICK Blk=NNN Drv=N Up=N Clp=N Env=N GMul=N Dn=N Out=N | D=N.NN AbsIn=N.NN AbsOut=N.NN
```

This makes the Console.app / Xcode log capture exactly the conditions
of each click event. If clicks correlate with Env spikes, we know
envelope is producing the discontinuity. If clicks correlate with
specific input characteristics (e.g., AbsIn jumping), we know the
source is at the boundary. If clicks happen in apparently-normal
blocks (all sections nominal, no input spike), the source is more
subtle.

Add input absolute-value tracking for the current block (the existing
output AbsSeen is a high-water-mark; we need a per-block input
snapshot for the click log).

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (product only):**
- `Source/PluginProcessor.cpp` — revert warm-keep block; lower
  `kClickThreshold` constant from `0.5f` to `0.2f`; add per-block
  input absolute tracking and `DBG()` line on click events.

**DO NOT TOUCH:**
- `Source/PluginProcessor.h` — no new atomics. The existing click
  atomics and getters from 7.1.2 stay.
- `Source/ui/*` — no UI changes. The debug header readout continues
  to show the same counters and watermarks.
- HQ source.
- Bench files.
- Any param.
- Any other meter / DSP / OS / envelope code.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

### 3.1 Revert envelope_R_ warm-keep in fast-path

In `processBlock`, the fast-path branch (`if (fastPath) { ... }`)
currently runs envelope_R_ on the R-channel peak stream and discards
its gain output (added in 7.1.1). Remove that block entirely.

Before:

```cpp
if (fastPath)
{
    // ... existing single-envelope peak/gain processing ...

    // 7.1.1 warm-keep: run envelope_R_ on R-channel peak, discard gain
    auto* peakR = peakBufR_.getWritePointer (0);
    for (int i = 0; i < osN; ++i)
        peakR[i] = std::abs (osCh1[i]);
    envelope_R_.setThresholdLinear (thresholdLin);
    envelope_R_.setReleaseMs (...);
    envelope_R_.setReleaseSustainRatio (...);
    envelope_R_.setMode (...);
    auto* gainR = gainBufR_.getWritePointer (0);
    envelope_R_.process (peakR, gainR, osN);   // discarded

    // ... existing gain application using envelope_L_'s gain ...
}
```

After:

```cpp
if (fastPath)
{
    // Existing single-envelope peak/gain processing ONLY.
    // envelope_R_ is not exercised in fast-path; its state remains
    // at its last-set state (initial or last full-path run).
    // If link-sweep transition pops surface, handle via state-copy
    // in a future slice. The random-pop hypothesis from 7.1.1 was
    // disproven by the 7.1.2 output click data.

    auto* peak = peakBuf_.getWritePointer (0);
    const float* const osPtrs[2] = { osCh0, osCh1 };
    for (int i = 0; i < osN; ++i)
        peak[i] = peakDetector_.process (osPtrs, nch, i);

    auto* gain = gainBuf_.getWritePointer (0);
    envelope_L_.process (peak, gain, osN);

    for (int ch = 0; ch < nch; ++ch)
    {
        auto* d = osBlock.getChannelPointer ((size_t) ch);
        for (int i = 0; i < osN; ++i)
        {
            const float delayed = lookahead_.pushPop (ch, d[i]);
            d[i] = delayed * gain[i];
        }
    }

    const float gr = envelope_L_.getLastBlockMaxGrDb();
    currentGrDb_.store  (gr, std::memory_order_relaxed);
    currentGrLDb_.store (gr, std::memory_order_relaxed);
    currentGrRDb_.store (gr, std::memory_order_relaxed);
}
```

The full-path branch (link < 99.95) is unchanged — envelope_R_ runs
there as designed by Slice 7.

### 3.2 Lower click threshold

Locate the `constexpr float kClickThreshold = 0.5f;` line added in
7.1.2. Change to:

```cpp
constexpr float kClickThreshold = 0.2f;
```

Everything else in the click detector logic stays the same.

### 3.3 Per-click section log

Add per-block input absolute tracking near the top of processBlock
(after the early returns, before the limiter chain), capturing the
max |sample| across input channels for THIS block:

```cpp
float blockInputAbs = 0.0f;
for (int ch = 0; ch < nch; ++ch)
{
    const float* d = buffer.getReadPointer (ch);
    const int n = buffer.getNumSamples();
    for (int i = 0; i < n; ++i)
    {
        const float a = std::abs (d[i]);
        if (a > blockInputAbs) blockInputAbs = a;
    }
}
```

(Or use `buffer.getMagnitude(...)` if available and equivalent —
single existing JUCE helper; either way the value is the per-block
input peak.)

At the end of processBlock, in the existing click-detection block,
when `blockHasClick == true`, emit a DBG line:

```cpp
if (blockHasClick)
{
    outputClickCount_.fetch_add (1, std::memory_order_relaxed);

    // Per-click section snapshot (this block's deltas, not the
    // running maxes). Compute the section deltas for THIS block
    // from the t0..t7 timestamps already captured.
    const auto drvUs   = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    const auto upUs    = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    const auto clpUs   = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
    const auto envUs   = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
    const auto gMulUs  = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();
    const auto dnUs    = std::chrono::duration_cast<std::chrono::microseconds>(t6 - t5).count();
    const auto outUs   = std::chrono::duration_cast<std::chrono::microseconds>(t7 - t6).count();

    DBG ("ML CLICK Blk=" << (int) blockUs
         << " Drv=" << (int) drvUs
         << " Up="  << (int) upUs
         << " Clp=" << (int) clpUs
         << " Env=" << (int) envUs
         << " GMul=" << (int) gMulUs
         << " Dn=" << (int) dnUs
         << " Out=" << (int) outUs
         << " | D=" << blockMaxDelta
         << " AbsIn=" << blockInputAbs
         << " AbsOut=" << blockMaxAbs);
}
```

Adjust the variable names to match what 7.1.2 used in
`PluginProcessor.cpp`. The `DBG()` macro prints to Console.app on
macOS and the Xcode console in Debug builds; in Release builds it
compiles out. **For this investigation, leave DBG active in both
builds by using `juce::Logger::writeToLog()` instead of `DBG()`** so
the click events appear in the Logger output regardless of build
mode:

```cpp
juce::Logger::writeToLog (juce::String ("ML CLICK Blk=") + ...);
```

avishali can view the log via Console.app filtered on "ML CLICK" or
by setting a custom log file via `juce::FileLogger`.

### 3.4 No UI change

The existing debug header readout (Blk / per-section / Clicks / D /
Abs) continues to work as in 7.1.2. The click logging is in addition
to, not replacement of, the visible counters.

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

Default link = 100 → fast-path → bit-exact same audio output as
Slice 7.0 (warm-keep had no audible effect since its gain was
discarded). Slice 3/4/5 bench remains PASS unchanged.

If avishali wants paranoia confirmation, a single Slice 3 quick run
suffices — must PASS 13/13.

────────────────────────────────────────
6. PRODUCT COMMIT (separate, no push)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect ONLY:
#    M Source/PluginProcessor.cpp

git add Source/PluginProcessor.cpp

git commit -m "$(cat <<'EOF'
MasterLimiter Slice 7.1.3: revert warm-keep, tighter click threshold, per-click log

Three pop-investigation refinements based on Slice 7.1.2 readout
data:

1. Reverted envelope_R_ warm-keep from 7.1.1.
   - Slice 7.1.2 readout showed output click counter at 0 with
     Abs 0.95 (no NaN/Inf), proving envelope_R_ state divergence
     during fast-path is NOT producing audible glitches.
   - Random pops at link = 100 cannot be from a link transition —
     the fast-path is steady, envelope_R_ isn't exercised by a
     transition at all.
   - Warm-keep was paying ~6.8x envelope CPU (125 us -> 845 us in
     the Env section watermark) for a hypothesis the output data
     disproved. Reverted.
   - envelope_R_ now runs only in full-path (link < 99.95) as
     designed in Slice 7.0. If link-sweep pops ever surface as a
     separate issue, address with a one-shot state-copy
     envelope_R_ <- envelope_L_ at transition (cleaner than
     warm-keep, no steady-state CPU cost).

2. Lowered click threshold 0.5 -> 0.2.
   - 7.1.2 measured D = 0.45 high-water-mark with Clicks = 0 at
     threshold 0.5. The detector almost-but-not-quite caught
     something. A 0.2 delta is ~-14 dB transient between adjacent
     samples - still well above steady-state music content but
     captures subtler discontinuities the 0.5 threshold missed.

3. Per-click section log.
   - When a block exceeds the click threshold, emit a
     juce::Logger::writeToLog line with this block's section
     timings, current delta, input abs, and output abs. Catches
     the exact conditions of each click event for diagnosis.
   - Visible in Console.app on macOS filtered on "ML CLICK", or
     in a custom FileLogger if avishali sets one up.
   - In addition to the existing UI debug header counters, not
     replacing.

Investigation outcome paths after audition:
- Clicks > 0 with consistent log lines -> we know what processBlock
  state correlates with output glitches and target accordingly.
- Clicks = 0 even at threshold 0.2 during audible pops -> pop is
  conclusively not in our output; investigate host-side / system-
  wide (reproduce in Logic / Reaper / standalone JUCE host).

No HQ, no bench, no params. Default link = 100 is bit-exact same
audio output as Slice 7.0 (warm-keep gain was discarded; removing
it changes only state freshness of envelope_R_ during fast-path).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. processBlock diff: warm-keep block removed (before/after of the
   fast-path branch).
2. Click threshold constant change: `0.5f` → `0.2f`.
3. Per-block input absolute tracking insertion point + the
   DBG/Logger writeToLog formatted line.
4. Build success lines for Release and Debug.
5. Product `git log --oneline -6` showing the new commit on top of
   `b0240d7`.
6. Confirmation: HQ NOT touched; product NOT pushed; no params; no
   bench; only `PluginProcessor.cpp` modified.
7. Open questions.

────────────────────────────────────────
8. AFTER THIS
────────────────────────────────────────

avishali audition checklist:

- Reset click stats via the existing UI button / interaction (or
  by relaunching the plugin).
- Confirm Env watermark drops back to the pre-7.1.1 range
  (~125 us at OS-rate, possibly less). Block max should fall
  proportionally.
- Play the source that produced the pops. Watch the debug header's
  Clicks counter.
- After ~30 seconds of pop-producing audio, open Console.app and
  filter on "ML CLICK". Paste back:
  - **Clicks counter final value** (UI debug header).
  - **First few ML CLICK log lines** if any exist — these tell us
    exactly what section was timing on the click block.
  - **Subjective: did pops still happen, and how often?**

Three outcomes:

**A. Clicks > 0, log lines correlate with Env or other spike** →
   Slice 7.1.4 targets the section identified.

**B. Clicks > 0, log lines show normal-section timings but high
   D and AbsOut** → the discontinuity is in our output but not
   from a CPU spike. Likely candidates: param-change zipper at OS
   rate, OS chain edge-case at specific block boundaries, lookahead
   state. We'd need finer-grained instrumentation (per-sample
   logging on the suspect block).

**C. Clicks = 0 even with pops audible** → our DSP output is
   provably clean. The pop is host-side (Ableton-specific or
   system-wide). Reproduce in another host to confirm. We move
   toward Slice 7 close with the pop documented as host-side.
