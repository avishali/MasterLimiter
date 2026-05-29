# Cursor Task — Slice 9.4: 4× oversample the limiter chain (Clipper + Envelope + Gain)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-side architectural addition. No HQ DSP source change required
> (LimiterEnvelope is already sampleRate-parameterised). No bench run
> this slice; bench recalibration consolidated into Slice 9.5 covering
> 9.2 + 9.3a + 9.3a.1 + 9.4 together.**
>
> avishali's audition: with release fixed (9.3a.1) the limiter feels
> right at low GR, but pushing Drive +8–10 dB to engage 3+ dB GR
> introduces audible distortion — present in **all three Character
> modes** and in **both SP and TP Ceiling modes**, ruling out the
> attack-ramp HF content as the source. The remaining mechanism is the
> snap-on-attack discontinuity in the smoothing stage
> (`if inp < s2 - eps: s1 = inp; s2 = inp;` in
> `LimiterEnvelope::process`): a hard step in the gain envelope at every
> peak event, infinite bandwidth, aliasing when applied to audio at 48k
> base rate. Industry-standard fix is 4× oversampling around the gain-
> application path so the discontinuity lands at 192k and the
> downsample FIR removes aliases before they re-enter the audible band.
>
> Pro-L 2 "Standard" = 4×. Ozone Maximizer IRC IV = 4×–8×. We match
> Pro-L Standard. Always-on, reuse the same
> `juce::dsp::Oversampling<float>` halfband-FIR-equiripple primitive
> already in `IspTrimStage` and `TruePeakDetector`.
>
> **Latency increases** by the oversampler round-trip (~10–20 base-rate
> samples per the JUCE Oversampling spec). PDC reported correctly.
> **Product-only commit.** No HQ touch. No push. No amend.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Product
files only this slice. No HQ source edits. No bench edits. No product
push. No amend of prior product commits.

────────────────────────────────────────
1. WHY (full math)
────────────────────────────────────────

`LimiterEnvelope` smoothing stage:

```cpp
if (inp < s2 - 1.0e-12f) {
    s1 = inp;
    s2 = inp;
}
```

On every peak event the gain envelope makes an instantaneous step. A
discrete step at the base sample rate has infinite frequency content;
multiplying it into the audio signal creates IMD products at every
sum-and-difference frequency, and any product above Nyquist folds back
into the audible band as alias-style roughness. Character mode affects
the *attack ramp shape* (slow raised cosine in Clean, sharp in
Aggressive) but does NOT remove the snap event itself — present in every
mode. Ceiling mode (SP/TP) doesn't affect the snap either; only the
final true-peak trim differs. avishali's audition confirms this:
distortion present uniformly across all 3 × 2 combinations of Character
and Ceiling.

At 4× rate the snap event still has infinite bandwidth in principle,
but the downsample FIR has a stopband above ~22 kHz that removes all
alias products below the original Nyquist. Net effect: limiter
distortion drops below audibility for normal mastering material.

The Clipper stage (when `clipper_drive_db > 0`) also benefits — hard
clipping at 4× and downsampling suppresses clipper aliasing. The
Clipper Drive readout/LED logic is unchanged in spirit; the
`maxPreClipAbs` tracker just runs over the OS sample count.

The Peak Detector benefits — it now catches intersample peaks for free
inside the OS region.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (product only):**
- `Source/PluginProcessor.h` — add `juce::dsp::Oversampling<float>
  limiterOversampler_` (4×, halfband FIR equiripple, like the existing
  ispTrim/TP oversamplers). Add `int limiterOsLatencySamples_ = 0;`.
- `Source/PluginProcessor.cpp` — in `prepareToPlay`:
  - Initialise `limiterOversampler_` with channel count = 2, factor = 2
    (juce dsp Oversampling specifies factor as `numStages`; 2 stages =
    4× since each stage is 2×). Use the same FIR equiripple filter type
    as the existing oversamplers.
  - Call `limiterOversampler_.initProcessing (maxBlockSize)` and capture
    `limiterOsLatencySamples_ = (int) std::round (limiterOversampler_.getLatencyInSamples())`.
  - Prepare `envelope_` with `sampleRate = host × 4`,
    `lookaheadSamples = baseLookaheadSamples × 4`,
    `maxBlockSize = hostMaxBlockSize × 4`.
  - Resize `peakBuf_` and `gainBuf_` to `hostMaxBlockSize × 4` samples
    (single channel, as today).
  - Resize/prepare `lookahead_` with `lookaheadSamples × 4` and
    `maxBlockSize × 4` (or whatever `mdsp_dsp::LookaheadDelay::prepare`
    expects — match its existing signature).
  - Recompute `baseLatencySamples_` = `baseLookaheadSamples +
    limiterOsLatencySamples_`. Existing TP branch still adds
    `osLatencySamples_` (ispTrim) on top.
  - `setLatencySamples (...)` with the recomputed value.
- `Source/PluginProcessor.cpp` — in `processBlock`, restructure the
  `if (limiterActive_->get())` block to:
  1. Apply Drive (`input_gain_db`) at 1×. (unchanged)
  2. Wrap the audio buffer in a `juce::dsp::AudioBlock<float>`.
  3. `auto osBlock = limiterOversampler_.processSamplesUp (block);` —
     osBlock is the 4× version, with N×4 samples.
  4. Clipper stage at OS rate: iterate `osBlock` per channel per
     sample, compute `scaled = sample × driveGain`, track
     `maxPreClipAbs` across all OS samples and channels, hard-clip to
     ±1.0. Same skip when `clipperDriveDb <= 0`. Store
     `currentClipDb_` from `maxPreClipAbs > 1 ? gainToDecibels(...) :
     0`.
  5. Peak detection at OS rate: iterate `osBlock` filling `peakBuf_`
     (N×4 entries), using existing `peakDetector_.process` per OS
     sample.
  6. Envelope at OS rate:
     `envelope_.setThresholdLinear(thresholdLin); setReleaseMs(...);
     setReleaseSustainRatio(...); setMode(...); process(peakBuf, gainBuf,
     osNumSamples);`
  7. Lookahead delay + gain multiply at OS rate: for each OS channel,
     for each OS sample, `delayed = lookahead_.pushPop(ch, osSample);
     osSample = delayed * gain[osSampleIndex];`
  8. `limiterOversampler_.processSamplesDown (block);` — writes back
     into the original 1× buffer.
  9. Store `currentGrDb_ = envelope_.getLastBlockMaxGrDb()` (unchanged).
- `Source/PluginProcessor.cpp` — ispTrim stage and `cachedCeilingMode_`
  latency-update branch: keep behaviour, but the new total latency in
  TP mode is `baseLatencySamples_ + osLatencySamples_` where
  `baseLatencySamples_` now includes `limiterOsLatencySamples_`. Adjust
  the `setLatencySamples` call in the mode-change branch accordingly.

**DO NOT TOUCH:**
- `shared/mdsp_dsp/...` — HQ source untouched this slice.
- Any bench file.
- Any parameter ID/range/default. `release_ms = 30 ms` from 9.3a stands.
- I/O trim path, metering, loudness analyzer.
- UI source — Clipper meter/LED, Character/Clipper Drive controls,
  GR/peak/TP meters all read atomics that are still populated.

────────────────────────────────────────
3. WHAT TO DO — detailed pattern
────────────────────────────────────────

### 3.1 Header

In `PluginProcessor.h`, alongside the existing `mdsp_dsp::IspTrimStage
ispTrim_;` field:

```cpp
juce::dsp::Oversampling<float> limiterOversampler_ {
    2,                                                       // numChannels
    2,                                                       // numStages (2 stages => 4x)
    juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
    true                                                     // integerLatency
};
int limiterOsLatencySamples_ = 0;
```

Match the exact pattern used in `IspTrimStage` / `TruePeakDetector` for
filter type. Adjust `numChannels` if those existing wrappers use a
different convention (Cursor verifies by reading those headers).

### 3.2 prepareToPlay

In the order the existing code does it (find where `envelope_.prepare`
is currently called):

```cpp
const int osFactor = 4;
const double osSampleRate = sampleRate * osFactor;
const int osMaxBlock = samplesPerBlock * osFactor;
const int osLookahead = baseLookaheadSamples_ * osFactor;

limiterOversampler_.reset();
limiterOversampler_.initProcessing ((size_t) samplesPerBlock);
limiterOsLatencySamples_ = (int) std::round (limiterOversampler_.getLatencyInSamples());

mdsp_dsp::LimiterEnvelope::Spec spec;
spec.sampleRate = osSampleRate;
spec.lookaheadSamples = osLookahead;
spec.maxBlockSize = osMaxBlock;
envelope_.prepare (spec);

peakBuf_.setSize (1, osMaxBlock, false, true, true);
gainBuf_.setSize (1, osMaxBlock, false, true, true);

lookahead_.prepare (2, osLookahead, osMaxBlock);   // match LookaheadDelay::prepare signature

baseLatencySamples_ = baseLookaheadSamples_ + limiterOsLatencySamples_;
const int reportedLatency = (cachedCeilingMode_ == 1)
    ? (baseLatencySamples_ + osLatencySamples_)
    : baseLatencySamples_;
setLatencySamples (reportedLatency);
```

`baseLookaheadSamples_` should still be the 1×-domain lookahead derived
from `lookahead_ms` (typically 5 ms × hostRate). The envelope sees 4× of
that; the host PDC sees baseLookahead + OS-latency.

### 3.3 processBlock — limiterActive == true branch

Sketch:

```cpp
// 1. Drive at 1x (unchanged)
const float inGainLin = juce::Decibels::decibelsToGain (inputGainDbParam_->get());
for (int ch = 0; ch < nch; ++ch)
    buffer.applyGain (ch, 0, n, inGainLin);

// 2. Wrap and upsample
juce::dsp::AudioBlock<float> block (buffer);
auto osBlock = limiterOversampler_.processSamplesUp (block);
const int osN = (int) osBlock.getNumSamples();

// 3. Clipper at OS rate
const float clipperDriveDb = clipperDriveDb_->load (std::memory_order_relaxed);
if (clipperDriveDb > 0.0f)
{
    const float driveGain = juce::Decibels::decibelsToGain (clipperDriveDb);
    float maxPreClipAbs = 0.0f;
    for (int ch = 0; ch < nch; ++ch)
    {
        auto* d = osBlock.getChannelPointer ((size_t) ch);
        for (int i = 0; i < osN; ++i)
        {
            const float scaled = d[i] * driveGain;
            const float a = std::abs (scaled);
            if (a > maxPreClipAbs) maxPreClipAbs = a;
            d[i] = juce::jlimit (-1.0f, 1.0f, scaled);
        }
    }
    currentClipDb_.store (maxPreClipAbs > 1.0f
                              ? juce::Decibels::gainToDecibels (maxPreClipAbs)
                              : 0.0f,
                          std::memory_order_relaxed);
}
else
{
    currentClipDb_.store (0.0f, std::memory_order_relaxed);
}

// 4. Peak detect at OS rate
auto* peak = peakBuf_.getWritePointer (0);
const float* osCh0 = osBlock.getChannelPointer (0);
const float* osCh1 = (nch > 1) ? osBlock.getChannelPointer (1) : osCh0;
const float* const osPtrs[2] = { osCh0, osCh1 };
for (int i = 0; i < osN; ++i)
    peak[i] = peakDetector_.process (osPtrs, nch, i);

// 5. Envelope at OS rate (setters unchanged - they re-derive alpha
//    from sampleRate at OS rate already because prepare set that)
const float thresholdLin = juce::Decibels::decibelsToGain (ceilingDbParam_->get());
envelope_.setThresholdLinear (thresholdLin);
envelope_.setReleaseMs (readFloatParam (apvts, "release_ms"));
envelope_.setReleaseSustainRatio (releaseSustainRatio_->get());
envelope_.setMode (mapCharacterIndexToMode (characterChoice_->getIndex()));

auto* gain = gainBuf_.getWritePointer (0);
envelope_.process (peak, gain, osN);

// 6. Lookahead + gain multiply at OS rate
for (int ch = 0; ch < nch; ++ch)
{
    auto* d = osBlock.getChannelPointer ((size_t) ch);
    for (int i = 0; i < osN; ++i)
    {
        const float delayed = lookahead_.pushPop (ch, d[i]);
        d[i] = delayed * gain[i];
    }
}

currentGrDb_.store (envelope_.getLastBlockMaxGrDb(), std::memory_order_relaxed);

// 7. Downsample back to 1x in place
limiterOversampler_.processSamplesDown (block);

// 8. ispTrim (TP only) at 1x as today — internally OSes
if (modeIdx == 1) { ... unchanged ... }
```

The `else` (`limiterActive_` false) branch keeps storing 0 to
`currentGrDb_`, `currentTpTrimDb_`, `currentClipDb_` and bypasses the
oversampler entirely (no `processSamplesUp/Down` call) — direct
io_input → io_output as today.

### 3.4 cachedCeilingMode_ latency update

The existing branch:

```cpp
if (modeIdx != cachedCeilingMode_) {
    const int newLatency = (modeIdx == 1) ? (baseLatencySamples_ + osLatencySamples_)
                                          : baseLatencySamples_;
    setLatencySamples (newLatency);
    ispTrim_.reset();
    cachedCeilingMode_ = modeIdx;
}
```

`baseLatencySamples_` is now `baseLookaheadSamples_ + limiterOsLatencySamples_`
so this branch is correct as-is — just make sure `baseLatencySamples_`
is initialised in `prepareToPlay` to include the OS latency BEFORE the
first `processBlock` runs.

### 3.5 Notes on the lookahead delay

The existing `mdsp_dsp::LookaheadDelay` is a delay line. Increasing the
prepared `numSamples` to `osLookahead = baseLookahead × 4` makes it
delay by the same wall-clock 5 ms at the OS rate. Confirm that
`LookaheadDelay::prepare`'s second argument is "delay length in
samples" and the third is "max block size in samples" before plugging
in; if the signature differs, match the existing call site's intent.

────────────────────────────────────────
4. BUILD (no bench)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean, no new warnings. Verify the latency reported by
the plugin matches `baseLookaheadSamples_ + limiterOsLatencySamples_`
in SP mode and `... + osLatencySamples_` in TP mode — a quick host
report (Ableton's Plug-in Info pane, or AU validation log) should show
the new value.

**Do NOT run the bench this slice.** Bench recalibration is Slice 9.5
covering 9.2 + 9.3a + 9.3a.1 + 9.4 in one consolidated update.

────────────────────────────────────────
5. PRODUCT COMMIT (separate, no push, no amend)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect ONLY:
#    M Source/PluginProcessor.h
#    M Source/PluginProcessor.cpp

git add Source/PluginProcessor.h Source/PluginProcessor.cpp

git commit -m "$(cat <<'EOF'
MasterLimiter: 4x oversample the limiter chain (Clipper + Envelope + Gain)

avishali's audition shows audible distortion as soon as GR exceeds
1-2 dB, present uniformly across all three Character modes and both
Ceiling modes. Mode-independence rules out the attack-ramp HF content
as the source; the remaining mechanism is the snap-on-attack
discontinuity in the LimiterEnvelope smoothing stage (s1 = inp; s2 =
inp on falling edge) creating a hard step in the gain envelope at
every peak event, infinite bandwidth, aliasing when applied to audio
at 48k base rate.

Industry-standard fix: 4x oversampling around the gain-application
path so the discontinuity lands at 192k and the downsample FIR removes
alias products before they re-enter the audible band. Pro-L 2
"Standard" = 4x; Ozone Maximizer IRC IV = 4-8x. We match Pro-L
Standard, always-on, reusing the same juce::dsp::Oversampling<float>
halfband-FIR-equiripple primitive already proven in TruePeakDetector
and IspTrimStage.

The oversampled region covers: Clipper, Peak Detect, LimiterEnvelope
(prepared at sampleRate * 4 with lookaheadSamples * 4), Lookahead
delay, and gain multiplication. Drive (input_gain_db) stays at 1x
(linear). I/O trims, metering, and loudness analysis stay at 1x.
ispTrim stays at 1x with its internal 4x for TP soft-knee unchanged -
a future micro-slice can deduplicate the nested OS in TP mode.

Latency: baseLatencySamples_ = baseLookaheadSamples + limiterOs
latency. Reported correctly via setLatencySamples. TP mode still
stacks ispTrim's latency on top as before. Host PDC stays glitch-free
across Character / on-off toggles (limiter chain stays prepared even
when limiter_active = false; bypass branch goes io_input -> io_output
directly with no OS call).

Clipper Drive readout/LED: maxPreClipAbs tracking now runs across the
4x sample count per block. Threshold and meaning unchanged.

Peak Detector picks up intersample peaks for free inside the OS
region.

Slice 3/4/5 bench not run this slice; recalibration consolidated into
Slice 9.5 covering 9.2 + 9.3a + 9.3a.1 + 9.4 changes together.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push product. Do NOT amend any prior product commit.**

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Header diff (new oversampler field + latency var).
2. `prepareToPlay` diff (OS init, envelope prepared at 4× rate, buffer
   resizes, latency recomputation).
3. `processBlock` diff (up/down wrap, clipper/peak/envelope/gain at OS
   rate).
4. Reported latency in samples at 48k host rate for both SP and TP
   modes (Cursor reads `getLatencySamples()` after `prepareToPlay` to
   confirm).
5. Build success lines for Release and Debug.
6. Product `git log --oneline -3`.
7. Confirmation: HQ NOT touched; product NOT pushed; bench NOT run;
   no other files modified.
8. Open questions — especially flag any signature mismatch in
   `LookaheadDelay::prepare` that needed an interpretive call.

────────────────────────────────────────
7. AFTER THIS
────────────────────────────────────────

avishali auditions in Live (Release VST3):

- Push Drive +8 to +12 dB on a dense mix until 3–5 dB GR. Distortion
  at modest GR should be substantially reduced or gone. Compare A/B
  to the previous 9.3a.1 build if possible (toggle plugin off → on
  while comparing peak-loudness-matched).
- A/B against Ozone 11 Maximizer at matched LUFS — character should
  be in the same family now (clean limiting, no gritty IMD).
- Sweep Clipper Drive 0 → +12 dB — clipper itself should also sound
  cleaner (4× rate, downsampled). Heavy push still distorts (no soft-
  knee yet; that's a future slice) but the distortion should be
  harmonic/smooth rather than aliased/gritty.
- Verify Limiter on/off bypass is byte-equivalent at default state
  (no OS call in the off branch).
- Confirm reported latency in Live's Plugin Info matches expectations
  (5 ms lookahead + OS latency at host rate; +ispTrim OS in TP mode).

On pass → Slice 9.5 (consolidated bench recalibration). Slice 5
criteria need re-derivation against the new envelope semantics
(anchor-fix, max() combinator, OS chain). Slice 3/4 may need
recalibration too because the latency now differs.

After 9.5 bench PASS:
- The product side already has `ce9a478` as the consolidated Slice 9
  product commit. We add this 9.4 commit on top. Both ship as the
  Slice 9 close set.
- HQ side: stack of 9.1 + 9.2 + 9.3a + 9.3a.1 commits (no further HQ
  in 9.4) ships as the Slice 9 close HQ set.
- avishali pushes HQ + product after a final smoke audition.

On partial pass (less distortion but still some at heavy push) →
two follow-up options:
- Slice 9.6a: bump OS to 8× (one more halfband stage).
- Slice 9.6b: soft-knee on the gain envelope (e.g., apply a small
  smoothing IIR after `g = clamp(...)` to round the snap event;
  cheaper than higher OS).
