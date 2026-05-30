# Cursor Task — Slice 14.7: GR meter shows TOTAL reduction (fix the ~4 dB cap)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Metering-only fix, on the existing Slice 14 branches. No audio-path
> change.** The GR meter currently reports only the wideband stage's
> reduction. In the serial multiband chain the bands limit to ceiling−2 dB and
> the wideband only ever catches the band-sum overshoot (≤ ~4 dB), so the meter
> flatlines at ~4 dB no matter how hard the user pushes — even though the bands
> keep taking more reduction and the output is correctly limited. This slice
> makes the GR snapshot report the **total** applied reduction (band ×
> wideband). Audio is unchanged → Slice 3/4/5 unaffected. **Do NOT commit, do
> NOT push.**

> **Continue on the existing Slice 14 branches**: product
> `slice-14-multiband-2band`, HQ `slice-14-multiband-bench`. No new branches.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product edit only: `Source/PluginProcessor.cpp` (the GR snapshot
  computation). No `mdsp_dsp`, no params, no UI/meter-widget change (the
  meter already renders `currentGr*` atomics; we only change what we store in
  them). No ADR edits.
- RT-safe: compute from values already in hand in the apply loop; atomic
  stores only (as today).
- This is metering only — **no change to any audio sample**. Confirm Slice
  3/4/5 still PASS unchanged (the gains applied to audio are identical).
- **Do NOT commit, do NOT push.** `MCP` stays dirty.

────────────────────────────────────────
1. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `Source/PluginProcessor.cpp` multiband region: where `currentGrDb_`,
  `currentGrLDb_`, `currentGrRDb_`, `maxGrSinceResetDb_` are currently stored
  (from `envelope_.getLastBlockMaxGrDb()` / `envelope_R_`), the band-gain
  blend producing `gLowOut`/`gHighOut`, the wideband gain (`gainBuf_` /
  `gainBufR_`), and the apply pass (`bandLimitedBuf_` × wide gain).
- Confirm the per-channel wide gain selection (fast-path: shared `gainBuf_`;
  unlinked: `gainBufR_` for R).

Output the retrieval log.

────────────────────────────────────────
2. THE FIX — total reduction per channel
────────────────────────────────────────

The total linear gain applied along the deepest band path, per channel `ch`
at sample `i`, is:

```
gDeepBand   = std::min (gLowOut[i], gHighOut[i])     // the more-reduced band
gWideCh     = (ch == 0) ? gainBuf_[i] : (fastPath ? gainBuf_[i] : gainBufR_[i])
gTotalCh[i] = gDeepBand * gWideCh                    // band reduction × wideband reduction
```

Track, per block, the **minimum** `gTotalCh` for each channel (the deepest
total reduction), then convert to positive dB GR:

```cpp
// during the per-sample apply loop, accumulate:
minTotalL = std::min (minTotalL, gTotalL);
minTotalR = std::min (minTotalR, gTotalR);   // = minTotalL when mono/fast-path as appropriate

// after the block:
const float grL = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minTotalL, -120.0f));
const float grR = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minTotalR, -120.0f));
currentGrLDb_.store (grL, std::memory_order_relaxed);
currentGrRDb_.store (grR, std::memory_order_relaxed);
currentGrDb_.store  (std::max (grL, grR), std::memory_order_relaxed);
// maxGrSinceResetDb_ updated from max(grL, grR) exactly as today
```

Initialise `minTotalL/minTotalR = 1.0f` each block (no reduction → GR 0).
Replace the existing `envelope_.getLastBlockMaxGrDb()` based stores with this.
Keep the same atomics, same reset/latched-max logic, same UI — only the
*value* changes.

Notes:
- `gLowOut`/`gHighOut` are the **blended** (post-Color) band gains actually
  applied, so the meter tracks what the user hears, including the Color knob.
- Using `min(gLowOut, gHighOut)` reports the deepest band's reduction (standard
  "max GR" meter behaviour). The high band is usually near 1.0, so on
  bass-heavy pushes this tracks the low band × wideband — and now grows
  without the 4 dB cap as the user pushes.
- Do not change the band/wideband gains applied to the audio — only what is
  measured and stored.

────────────────────────────────────────
3. VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```

- **Audio unchanged:** Slice 3/4/5 quick must PASS with the **same** numbers
  as Slice 14.6 (no threshold changes — if any audio metric moves, the apply
  loop was altered by mistake; STOP).
```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE14_7_REGRESSION_S$SLICE"
done
```
- **Meter behaviour:** in a host, push Drive/input hard on bass-heavy material
  — GR meter should now climb past 4 dB (well beyond, into 8–12 dB+) as you
  push, instead of flatlining. At Color = Open vs Glued the reading should
  differ (more decoupling → deeper low-band GR shown).

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Diff of the GR snapshot computation (before/after).
3. Build summary (Debug + Release, warnings).
4. Slice 3/4/5: PASS with numbers **identical** to 14.6 (proving audio
   untouched).
5. Note confirming the meter now climbs past 4 dB on a hard push (if you can
   observe it offline, otherwise leave for avishali's audition).
6. `git status --short` product + HQ.
7. Open questions.

────────────────────────────────────────
5. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali re-checks the GR meter tracks push depth and the Color sweep. On
approval the slice folds into the pending Slice 14 consolidated close (ADR
revisions + cleanup). **Do not self-close.**
