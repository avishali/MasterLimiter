# Cursor Task — Slice 14.1: wideband detects the band-limited signal (fix multiband de-coupling)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Fix-up of Slice 14, on the same uncommitted branches.** Slice 14
> built the 2-band stage correctly but wired the final wideband brickwall
> to detect the **pre-band-reduction** signal (`low + high`), so the
> brickwall re-ducks the full spectrum on every bass peak and structurally
> undoes the decoupling (IMD moved only 0.62 dB; transient crest had to be
> loosened 8 dB → over-squashing from double-limiting). This slice rewires
> wideband detection to react to the **band-limited** signal, fixes the
> cross-band IMD test's Ozone operating point so its numbers are
> trustworthy, and re-tightens the Slice 3/4/5 thresholds now that the
> over-squash is gone. **Still automatic, zero new params. Do NOT commit,
> do NOT push.**

> **Continue on the existing uncommitted Slice 14 branches**:
> product `slice-14-multiband-2band`, HQ `slice-14-multiband-bench`.
> Do not create new branches. The Slice 14 working-tree changes are the
> starting point.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product source edits permitted (`Source/PluginProcessor.{h,cpp}`).
- HQ edits permitted **only** under `shared/dsp_bench/`.
- RT-safety: no allocation, no locks, no logging, no OS calls on the audio
  thread. All new buffers sized in `prepareToPlay`.
- No new params, no new frozen IDs, no UI changes, no `mdsp_dsp` source
  edits, no ADR edits (ADR-0009 already describes this — see §1).
- **Do NOT commit, do NOT push.** Architect closes.
- HQ `MCP` pointer stays dirty — untouched.

────────────────────────────────────────
1. WHY (architect review of Slice 14)
────────────────────────────────────────

ADR-0009 §2 already states the wideband stage should "catch band-sum
overshoot" — i.e. it must see the signal **after** band limiting, so that
once the bass band is reduced the brickwall ducks less and spares the
highs. The "conservative, detect from the un-reduced sum" simplification
noted in ADR-0009 §2 / the Slice 14 prompt was wrong: it is precisely the
mechanism that re-couples the bands.

Current code (`PluginProcessor.cpp`, detection pass ~line 614):

```cpp
const float wide = std::abs (low + high);   // PRE-band-reduction — re-ducks everything
```

…and the apply pass multiplies that wideband gain onto the band-limited
sum. Net effect: the kick still ducks the highs, and both stages reduce in
series → over-squashing. Evidence: cross-band IMD improved only 0.62 dB
vs the single-band baseline, and the Slice 3/4/5 transient-crest threshold
had to be loosened from −2.0 to −10.3 dB.

Fix: the wideband envelope must detect a **band-limited estimate** of the
signal, so its gain reacts to the post-reduction bass.

────────────────────────────────────────
2. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `Source/PluginProcessor.cpp` — the entire multiband region you wrote in
  Slice 14 (detection pass ~596–630, fast-path/unlinked wideband
  ~636–683, apply pass ~685–708).
- `Source/PluginProcessor.h` — the Slice 14 members
  (`detectCrossover_`, `applyCrossover_`, `envelopeLow_`, `envelopeHigh_`,
  `peakLowBuf_/gainLowBuf_/peakHighBuf_/gainHighBuf_`, `kCrossoverHz`,
  `kBandLink`).
- `third_party/melechdsp-hq/docs/DECISIONS/ADR-0009-...md` §2 (final
  wideband brickwall = catches band-sum overshoot), §5 (single shared
  lookahead → constant latency), §7 (CPU).
- `shared/dsp_bench/drivers/ozone_maximizer.py` — its character/IRC mode,
  ceiling, and how the test drives it (operating point — see §4.2).

Output the retrieval log.

────────────────────────────────────────
3. THE WIDEBAND REWIRE (product)
────────────────────────────────────────

### 3.1 New members (`PluginProcessor.h`)

```cpp
juce::AudioBuffer<float> bandLowBuf_;    // (2 ch, osMax) per-channel LOW band, undelayed
juce::AudioBuffer<float> bandHighBuf_;   // (2 ch, osMax) per-channel HIGH band, undelayed
juce::AudioBuffer<float> gLowOutBuf_;    // (1 ch, osMax) blended low-band gain
juce::AudioBuffer<float> gHighOutBuf_;   // (1 ch, osMax) blended high-band gain
```

Size them in `prepareToPlay` alongside the existing band buffers:
```cpp
bandLowBuf_.setSize  (2, osMaxBlockSize, false, true, true);
bandHighBuf_.setSize (2, osMaxBlockSize, false, true, true);
gLowOutBuf_.setSize  (1, osMaxBlockSize, false, true, true);
gHighOutBuf_.setSize (1, osMaxBlockSize, false, true, true);
```

### 3.2 Detection pass — store per-channel band signals, drop the pre-reduction `wide`

Replace the current detection loop so it **stores** each channel's low/high
band values and computes only the band peaks. Remove the `low + high`
wideband peak computation entirely (it moves to §3.4):

```cpp
auto* peakLow  = peakLowBuf_.getWritePointer (0);
auto* peakHigh = peakHighBuf_.getWritePointer (0);
float* bandLow[2]  = { bandLowBuf_.getWritePointer (0),  nch > 1 ? bandLowBuf_.getWritePointer (1)  : nullptr };
float* bandHigh[2] = { bandHighBuf_.getWritePointer (0), nch > 1 ? bandHighBuf_.getWritePointer (1) : nullptr };

for (int i = 0; i < osN; ++i)
{
    float lowMax = 0.0f, highMax = 0.0f;
    for (int ch = 0; ch < nch; ++ch)
    {
        float lo = 0.0f, hi = 0.0f;
        detectCrossover_.processSample (ch, osBlock.getChannelPointer((size_t)ch)[i], lo, hi);
        bandLow[ch][i]  = lo;
        bandHigh[ch][i] = hi;
        lowMax  = std::max (lowMax,  std::abs (lo));
        highMax = std::max (highMax, std::abs (hi));
    }
    peakLow[i]  = lowMax;     // band detection stays stereo-linked
    peakHigh[i] = highMax;
}
detectCrossover_.snapToZero();

envelopeLow_.process  (peakLow,  gainLowBuf_.getWritePointer (0),  osN);
envelopeHigh_.process (peakHigh, gainHighBuf_.getWritePointer (0), osN);
```

### 3.3 Blend the band gains ONCE, into buffers (reused by §3.4 and the apply pass)

```cpp
const float* gainLow  = gainLowBuf_.getReadPointer (0);
const float* gainHigh = gainHighBuf_.getReadPointer (0);
auto* gLowOut  = gLowOutBuf_.getWritePointer (0);
auto* gHighOut = gHighOutBuf_.getWritePointer (0);
const float oneMinusBandLink = 1.0f - kBandLink;
for (int i = 0; i < osN; ++i)
{
    const float gLinked = std::min (gainLow[i], gainHigh[i]);
    gLowOut[i]  = kBandLink * gLinked + oneMinusBandLink * gainLow[i];
    gHighOut[i] = kBandLink * gLinked + oneMinusBandLink * gainHigh[i];
}
```

### 3.4 Wideband peak from the BAND-LIMITED estimate (the fix)

Build the wideband peak stream from `bandLow·gLowOut + bandHigh·gHighOut`
at the **detection timeline** (all index `i`). This is the band-limited
estimate of the signal the brickwall is responsible for. Stereo handling
matches the existing fast-path (linked) vs unlinked split:

```cpp
auto* peakWideL = peakBuf_.getWritePointer (0);
auto* peakWideR = peakBufR_.getWritePointer (0);
for (int i = 0; i < osN; ++i)
{
    const float sumL = bandLow[0][i] * gLowOut[i] + bandHigh[0][i] * gHighOut[i];
    peakWideL[i] = std::abs (sumL);
    if (nch > 1)
    {
        const float sumR = bandLow[1][i] * gLowOut[i] + bandHigh[1][i] * gHighOut[i];
        peakWideR[i] = std::abs (sumR);
    }
    else
    {
        peakWideR[i] = peakWideL[i];
    }
}
```

Then the **existing** fast-path / unlinked wideband envelope code runs
unchanged on `peakBuf_` / `peakBufR_` → `gainBuf_` / `gainBufR_`, producing
`gainWideL` / `gainWideR` and driving the GR snapshots exactly as before.
(Fast-path takes `max(peakWideL, peakWideR)`; unlinked keeps them per
channel with the link blend.) Do not change that block other than its
input now being the band-limited peaks.

### 3.5 Apply pass — unchanged in shape, reuse the blended gains

The apply pass keeps using `applyCrossover_` on the **lookahead-delayed**
audio, and now reads the pre-computed `gLowOut`/`gHighOut` buffers instead
of recomputing the blend:

```cpp
for (int i = 0; i < osN; ++i)
{
    for (int ch = 0; ch < nch; ++ch)
    {
        auto* d = osBlock.getChannelPointer ((size_t) ch);
        const float delayed = lookahead_.pushPop (ch, d[i]);
        float lo = 0.0f, hi = 0.0f;
        applyCrossover_.processSample (ch, delayed, lo, hi);
        const float bandSum  = lo * gLowOut[i] + hi * gHighOut[i];
        const float wideGain = (ch == 0) ? gainBuf_.getReadPointer(0)[i]
                                         : (fastPath ? gainBuf_.getReadPointer(0)[i]
                                                     : gainBufR_.getReadPointer(0)[i]);
        d[i] = bandSum * wideGain;
    }
}
applyCrossover_.snapToZero();
```

### 3.6 Invariants to preserve

- **Single lookahead only** (`lookahead_`). No second delay line. Reported
  latency stays `baseLatencySamples_` (ADR-0009 §5).
- The wideband peak estimate uses the band gains at the same index `i` as
  the band audio — alignment-consistent. The only approximation is the
  band envelope's lookahead pre-ramp slightly over-reducing the estimate
  near onsets; this is conservative and small (ADR-0009 §2). Do not try to
  "fix" it with a second delay.
- GR snapshots still driven by the wideband envelope. No UI change.

────────────────────────────────────────
4. BENCH FIXES (HQ, shared/dsp_bench)
────────────────────────────────────────

### 4.1 Re-run the cross-band IMD test after the rewire

Re-measure MasterLimiter vs Ozone vs the pre-Slice-14 single-band baseline
on the 60 Hz + 10 kHz two-tone. Expect the rewire to move our number
**substantially more** than the prior 0.62 dB. Report all three.

### 4.2 Fix the Ozone operating point (its number was untrustworthy)

Slice 14 reported Ozone at −16.56 dBc ≈ **14.9 % IMD**. In the Slice 9.6c
reference Ozone read ~6.99 %. IRC IV does not produce ~15 % intermod on a
clean two-tone unless it is being slammed into heavy GR or mis-configured.
Before trusting the comparison:

- Confirm/print the **GR Ozone is actually doing** at the test signal
  level. If it is being driven far past MasterLimiter's GR, the comparison
  is invalid.
- **Match the operating point**: calibrate the two-tone level (or the
  per-plugin ceiling/threshold) so MasterLimiter and Ozone do **the same
  GR** (target ~3 dB) at the measurement. Confirm the Ozone driver uses a
  transparent IRC IV / Modern setting (check `ozone_maximizer.py`).
- Re-report Ozone's IMD at the matched operating point. A sane IRC IV
  number on this signal should land in the single-digit-percent range; if
  it still reads ~15 % at matched 3 dB GR, STOP and report — the driver or
  signal is wrong and we must not gate against a bogus reference.

### 4.3 Sensitivity sanity check (kBandLink sweep)

To confirm the rewired multiband actually does frequency-selective work,
measure cross-band IMD at `kBandLink ∈ {0.0, 0.25, 0.5}` (temporary local
sweep — leave the shipped constant at 0.5 unless §6 audition says
otherwise). Expect IMD to **improve monotonically as kBandLink decreases**
(more decoupling). If it stays flat across the sweep, the band stage still
is not decoupling — STOP and report.

### 4.4 Re-tighten Slice 3/4/5 (the over-squash should be gone)

Re-run Slice 3/4/5 quick. With the brickwall no longer double-reducing,
the transient-crest and residual thresholds should pull back **most of the
way** toward their pre-Slice-14 values:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE14_1_REGRESSION_S$SLICE"
done
```

- Tighten every threshold you loosened in Slice 14 back to the **smallest
  loosening that still passes**. Only the genuine static-LR4-allpass
  residual should remain (expect a *modest* null/residual nudge, NOT an
  8 dB crest blowout).
- If transient crest still needs anywhere near −10 dB of loosening to pass,
  STOP and report — that means the build is still over-squashing and the
  rewire did not take.
- Report before (Slice 14) → after (Slice 14.1) for every threshold, and
  confirm IMD/THD criteria were not loosened.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new `Source/` warnings.

Then run §4.1–4.4 and report.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Product diff: new buffers, the detection/blend/wideband-estimate/apply
   restructure (before/after of the wideband peak source).
3. Build summary (Debug + Release, warnings).
4. Cross-band IMD after the rewire: MasterLimiter vs Ozone (at matched
   operating point, with Ozone's GR printed) vs pre-Slice-14 baseline.
5. kBandLink sweep table (0.0 / 0.25 / 0.5) showing monotonic improvement.
6. Slice 3/4/5: before→after thresholds; confirmation crest pulled back
   and IMD/THD not loosened.
7. `git status --short` on product and HQ (still uncommitted on the
   Slice 14 branches; `MCP` dirty).
8. Open questions / proposed final `kBandLink` if the numbers argue for it.

────────────────────────────────────────
7. ARCHITECT AUDITION + CLOSE
────────────────────────────────────────

Architect reviews the rewired numbers. If the IMD improvement is now
material, Ozone reads a sane matched-GR number, and Slice 3/4/5 pulled
back, **then** avishali auditions (de-pumping audible, no tonal tilt, no
clicks at 48 k/256 Clean heavy-GR). On approval the architect writes the
Slice 14 consolidated close (ADR-0009 commit + product DSP commit + bench
commit + PROGRESS/PLAN + prompt archive). **Do not self-close.**
