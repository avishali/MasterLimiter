# Cursor Task — Slice 14: Multiband (2-band) frequency-selective limiting (ADR-0009)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-repo DSP slice + HQ bench evolution.** Adds a 2-band LR4
> pre-shave stage inside the existing 4× OS region so a sub-120 Hz
> transient stops ducking the whole spectrum (cross-band de-pumping,
> the one remaining Ozone-Maximizer gap per ADR-0009). The existing
> single-band limiter becomes the final wideband brickwall. **Automatic,
> zero new parameters, zero new frozen IDs.** HQ side: one new bench
> test (cross-band IMD vs the `ozone_maximizer` driver) + a documented
> treatment-B recalibration of the Slice 3/4/5 null criteria for the
> always-on crossover's static allpass phase. Branch off product `main`
> and HQ `master`. **Do NOT push** — architect handles push + close.

> **Ordering:** This slice assumes Slice 13 (LUFS calibration) has
> already closed and both trees are clean. If Slice 13 is still open,
> STOP and say so.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product-repo source edits permitted (DSP orchestration lives in the
  product processor, consistent with the Slice 7 stereo-unlink and the
  `juce::dsp::Oversampling` precedent — both use `juce::dsp` primitives
  directly in the product).
- HQ edits permitted **only** under `shared/dsp_bench/` (new test +
  criteria recal). **No HQ `mdsp_dsp` source changes** — the two extra
  `LimiterEnvelope` instances reuse the existing HQ primitive unchanged.
- RT-safety on the audio thread: no allocation, no locks, no logging,
  no OS calls. All buffers sized in `prepareToPlay`.
- Branch `slice-14-multiband-2band` on the product repo, and
  `slice-14-multiband-bench` on HQ. **Do NOT commit, do NOT push** on
  either (architect closes).
- HQ `MCP` nested-submodule pointer stays dirty as it has since the
  ADR-0007 close — leave it untouched.

────────────────────────────────────────
1. WHY (ADR-0009)
────────────────────────────────────────

Read `third_party/melechdsp-hq/docs/DECISIONS/ADR-0009-masterlimiter-multiband-detection.md`
in full. Summary of the decision this slice implements:

- The one remaining audition gap vs Ozone 11 Maximizer (IRC IV) is
  **cross-band pumping**: a kick/bass transient drives the single
  broadband gain envelope, ducking the *entire* spectrum (incl. vocal/
  cymbals) on every beat. Structurally unfixable single-band; it is also
  the dominant IMD source (8.8 % vs Ozone 6.99 %).
- Fix = **frequency-selective gain reduction**: split off the bass into
  its own band so its limiting no longer ducks the highs.
- **2 bands**, LR4 @ ~120 Hz, inside the existing 4× OS region. The
  existing single-band envelope chain is reused as the **final wideband
  brickwall** that guarantees the ceiling and catches band-sum overshoot.
- **Tonal-preserving band link** (the Slice 7 stereo `min()`-then-blend
  math, transposed to frequency) prevents tonal tilt under GR.
- Bands run **stereo-linked**; the wideband final stage keeps the Slice 7
  stereo unlink, so width is preserved there.
- **Always-on, no params.** Defaults are intentionally **not bit-exact**
  (always-on LR4 is allpass) → Slice 3/4/5 null criteria recalibrate
  treatment-B; the *real* gate is the new cross-band IMD test vs Ozone.

────────────────────────────────────────
2. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `Source/PluginProcessor.h` — members (`lookahead_`, `peakDetector_`,
  `envelope_`, `envelope_R_`, `limiterOversampler_`, `peakBuf_`/`gainBuf_`
  /`peakBufR_`/`gainBufR_`, `baseLatencySamples_`).
- `Source/PluginProcessor.cpp` `processBlock` — the OS region
  (`processSamplesUp` → clipper → Drive → detection/apply → `processSamplesDown`),
  specifically the fast-path (link ≥ 0.9995) and unlinked branches.
- `third_party/melechdsp-hq/shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h`
  — `Spec`, `prepare`, `process`, `getLastBlockMaxGrDb`.
- `juce::dsp::LinkwitzRileyFilter<float>` API: `prepare(ProcessSpec)`,
  `setType(...)` (not needed — see below), `setCrossoverFrequency(f)`,
  `processSample(int channel, float in, float& outLow, float& outHigh)`,
  `reset()`, `snapToZero()`. The 3-output `processSample` overload yields
  both bands from one instance.
- `shared/dsp_bench/criteria.py`, `shared/dsp_bench/bench.py`,
  `shared/dsp_bench/drivers/ozone_maximizer.py` — for the bench changes.

Output the retrieval log.

────────────────────────────────────────
3. SCOPE — files
────────────────────────────────────────

**MODIFY (product):**
- `Source/PluginProcessor.h` — new members (§4.1).
- `Source/PluginProcessor.cpp` — `prepareToPlay` prep + `processBlock`
  multiband region (§4.2–4.4).

**MODIFY / ADD (HQ, bench only):**
- `shared/dsp_bench/` — new cross-band IMD test + Slice 3/4/5 criteria
  recal (§5.2–5.3). Exact file layout follows the existing bench
  convention; do not invent a new framework.

Do NOT touch: any `mdsp_dsp` source/header, any parameter file
(`ParameterIDs.h`, `Parameters.cpp`), any UI file, any ADR (ADR-0009 is
already written by the architect). No new params. No new frozen IDs.

────────────────────────────────────────
4. WHAT TO BUILD (product DSP)
────────────────────────────────────────

### 4.1 New members (`PluginProcessor.h`)

```cpp
#include <juce/...>   // juce_dsp already available via JUCE module

// --- Multiband (ADR-0009) ---
static constexpr float kCrossoverHz = 120.0f;   // tunable in-slice
static constexpr float kBandLink    = 0.5f;     // tonal-preserving blend; tune in-slice

juce::dsp::LinkwitzRileyFilter<float> detectCrossover_;  // splits UNdelayed audio (detection)
juce::dsp::LinkwitzRileyFilter<float> applyCrossover_;   // splits lookahead-DELAYED audio (application)

mdsp_dsp::LimiterEnvelope envelopeLow_;
mdsp_dsp::LimiterEnvelope envelopeHigh_;

juce::AudioBuffer<float> peakLowBuf_,  gainLowBuf_;       // 1 ch, OS-rate, stereo-linked
juce::AudioBuffer<float> peakHighBuf_, gainHighBuf_;
```

### 4.2 `prepareToPlay`

Right after the existing `envelope_R_.prepare(spec)` / oversampler prep:

```cpp
juce::dsp::ProcessSpec xoSpec { osSampleRate, (juce::uint32) osMaxBlockSize, 2 };
detectCrossover_.prepare (xoSpec);
applyCrossover_.prepare (xoSpec);
detectCrossover_.setCrossoverFrequency (kCrossoverHz);
applyCrossover_.setCrossoverFrequency  (kCrossoverHz);
detectCrossover_.reset();
applyCrossover_.reset();

// Band envelopes share the SAME Spec as the wideband envelopes
// (same OS rate, same lookahead, same maxBlock) so all three look
// ahead by the same window → constant latency, no extra PDC.
envelopeLow_.prepare (spec);
envelopeHigh_.prepare (spec);

peakLowBuf_.setSize  (1, osMaxBlockSize, false, true, true);
gainLowBuf_.setSize  (1, osMaxBlockSize, false, true, true);
peakHighBuf_.setSize (1, osMaxBlockSize, false, true, true);
gainHighBuf_.setSize (1, osMaxBlockSize, false, true, true);
```

Use whatever the local variable names are for the OS sample rate / OS max
block in the existing prep (match the `peakBuf_.setSize` line exactly).

In `releaseResources`/`reset` (wherever the existing envelopes reset),
add `detectCrossover_.reset(); applyCrossover_.reset();
envelopeLow_.reset(); envelopeHigh_.reset();`.

### 4.3 `processBlock` — multiband region

This replaces the **detection + apply** region only (from where
`envelope_.setThresholdLinear(thresholdLin)` is set through the per-sample
gain-application loops, i.e. lines ~558–638). The clipper, Drive, OS
up/down, threshold/TP-headroom computation, and the GR-max bookkeeping
stay as they are. Keep both the wideband linked (fast-path) and unlinked
branches — they now compute the **wideband gain** `gWide` and apply it to
the **band-limited sum** instead of the raw delayed input.

Recipe (two passes over the OS block):

**Set up band envelopes** (mirror the wideband setup — same threshold,
release, ratio, mode):
```cpp
for (auto* e : { &envelopeLow_, &envelopeHigh_ }) {
    e->setThresholdLinear (thresholdLin);
    e->setReleaseMs (readFloatParam (apvts, "release_ms"));
    e->setReleaseSustainRatio (releaseSustainRatio_->get());
    e->setMode (mapCharacterIndexToMode (characterChoice_->getIndex()));
}
```

**Pass A — band detection (stereo-linked) from UNdelayed OS audio:**
```cpp
auto* pLow  = peakLowBuf_.getWritePointer (0);
auto* pHigh = peakHighBuf_.getWritePointer (0);
for (int i = 0; i < osN; ++i) {
    float loMax = 0.0f, hiMax = 0.0f;
    for (int ch = 0; ch < nch; ++ch) {
        float lo, hi;
        detectCrossover_.processSample (ch, osBlock.getChannelPointer((size_t)ch)[i], lo, hi);
        loMax = std::max (loMax, std::abs (lo));
        hiMax = std::max (hiMax, std::abs (hi));
    }
    pLow[i]  = loMax;
    pHigh[i] = hiMax;
}
auto* gLow  = gainLowBuf_.getWritePointer (0);
auto* gHigh = gainHighBuf_.getWritePointer (0);
envelopeLow_.process  (pLow,  gLow,  osN);
envelopeHigh_.process (pHigh, gHigh, osN);
```
Note: `processSample` signature is `(channel, input, float& outLow,
float& outHigh)`; verify the exact JUCE overload arg order/return in your
version and adapt. Detection here is stereo-linked (max over channels).

**Wideband gain** (`gWide`) is produced by the **existing** fast-path /
unlinked envelope code — keep it verbatim **except do not apply it to the
audio yet**. In the fast-path: `gWide[i] = gain[i]` (single stream applied
to both channels). In the unlinked branch: keep `gOutL[i]`/`gOutR[i]` as
the per-channel wideband gains. Stash these in the existing `gainBuf_`
(and `gainBufR_`) — do not multiply into the audio in that loop anymore.

**Pass B — apply (band gains stereo-linked, wideband per-channel):**
```cpp
for (int i = 0; i < osN; ++i) {
    const float gLink   = std::min (gLow[i], gHigh[i]);
    const float gLowOut  = kBandLink * gLink + (1.0f - kBandLink) * gLow[i];
    const float gHighOut = kBandLink * gLink + (1.0f - kBandLink) * gHigh[i];

    for (int ch = 0; ch < nch; ++ch) {
        const float delayed = lookahead_.pushPop (ch, osBlock.getChannelPointer((size_t)ch)[i]);
        float lo, hi;
        applyCrossover_.processSample (ch, delayed, lo, hi);
        const float bandSum = lo * gLowOut + hi * gHighOut;

        const float gWideCh = /* fast-path: gain[i]; unlinked: ch==0 ? gOutL : gOutR */;
        osBlock.getChannelPointer((size_t)ch)[i] = bandSum * gWideCh;
    }
}
```

Key invariants:
- `lookahead_` is the **only** lookahead delay (reused). `detectCrossover_`
  splits undelayed audio (detection); `applyCrossover_` splits the
  lookahead-delayed audio (application). Both crossovers are identical
  filters with independent state → consistent group delay → alignment
  holds. **Reported latency = `baseLatencySamples_`, unchanged.**
- The wideband gain is computed from the **undelayed wideband peak**
  (existing behaviour). Applied to the band-limited sum it is *conservative*
  (the sum ≤ input once bands limit), so it may over-reduce by a hair under
  heavy band limiting — this only happens where a touch more GR is
  inaudible, and it never under-limits the ceiling. Accepted v1
  simplification (ADR-0009 §2 / refinement note).

### 4.4 GR metering

Leave the existing GR snapshots driven by the **wideband** envelope
(`currentGrDb_`, `currentGrLDb_`, `currentGrRDb_`, `maxGrSinceResetDb_`)
exactly as today. The band pre-shave reduces how hard the wideband stage
works, so at the same loudness the GR meter will read **less** than before —
that lower reading *is* the visible evidence the pre-shave is relieving the
brickwall. Per-band GR metering is out of scope for this slice (no new
meters, no UI change). Do **not** add UI.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

### 5.1 Build
```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout -b slice-14-multiband-2band
cd third_party/melechdsp-hq && git checkout -b slice-14-multiband-bench && cd ../..
# edit per §4 and §5.2–5.3
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new warnings in `Source/`.

### 5.2 New bench test — cross-band IMD (the real gate)

Add a test that drives a **two-tone 60 Hz + 10 kHz** signal (equal
amplitude, scaled to produce ~3–6 dB GR through the limiter) and measures
the **10 kHz region sideband energy** — the intermodulation products of
the 60 Hz envelope modulating the 10 kHz tone (sidebands at 10 kHz ± k·60
Hz). Report:
- MasterLimiter sideband energy (dB below the 10 kHz fundamental).
- `ozone_maximizer` driver sideband energy on the **same** signal.
- Pass criterion: MasterLimiter within a documented margin of Ozone
  (target: our IMD sidebands ≤ Ozone + 3 dB; tune the margin once you
  see the first numbers and record the chosen value in `criteria.py`
  with an ADR-0009 comment).

Compare against the **pre-Slice-14 single-band build** too (a before/after
delta on the same metric) so the architect can see the improvement
magnitude. Follow the existing driver/criteria/runner structure in
`shared/dsp_bench/`; reuse the corpus-generation helpers rather than
inventing new infra.

### 5.3 Slice 3/4/5 recalibration (treatment-B, documented)

The always-on LR4 crossover is allpass → the null-vs-dry residual in the
Slice 3/4/5 criteria shifts (inaudible static phase). Run all three:
```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE14_REGRESSION_S$SLICE"
done
```
- For any criterion that fails **only** because of the static crossover
  phase (null residual / phase-sensitive metrics), loosen it the minimum
  amount with an inline `# ADR-0009: always-on LR4 allpass phase` comment.
  This is treatment-B (worse in a known, documented, inaudible direction).
- IMD / THD criteria must **not** be loosened — they should improve or
  hold. If any of those regress, STOP and report (that is treatment-C,
  unexplained — do not paper over it).
- Report the before/after of every threshold you touch.

### 5.4 CPU / underrun close-gate check

ADR-0009 §7: the extra envelope + two crossovers are the main risk given
the Slice 7 deadline-miss class. In Ableton at **48 kHz / 256 samples,
Clean character, heavy GR** (the worst case), confirm no audible clicks
and CPU headroom comparable to the pre-Slice-14 build. Report the Ableton
CPU figure before/after if observable.

### 5.5 Audition (avishali)

A/B the Slice 14 build vs the pre-Slice-14 build on bass-heavy program
with audible kick-driven pumping:
1. The highs/vocal should stay **steadier** under the kick (less pumping).
2. Tonal balance should **not** visibly tilt as GR increases (the
   `kBandLink` blend doing its job) — if it does, note it; the slow-makeup
   refinement (ADR-0009 §3) is the named follow-up.
3. No clicks at any link setting or character mode.

If pumping is reduced but tone tilts, or the effect is too subtle, report
numbers — `kCrossoverHz` and `kBandLink` are tunable in-slice before close.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Files modified (product + bench), one-line purpose each.
3. Product diff summary: new members, `prepareToPlay` prep, the
   `processBlock` multiband region (before/after of the apply loops).
4. Build summary (Debug + Release, warnings).
5. Cross-band IMD test: MasterLimiter vs Ozone vs pre-Slice-14, the
   chosen pass margin, pass/fail.
6. Slice 3/4/5: invocation + result; every recalibrated threshold with
   before/after and the ADR-0009 comment; confirmation IMD/THD did not
   regress.
7. CPU/underrun note (§5.4).
8. `git status --short` on product (dirty on slice branch, no commit) and
   HQ (`MCP` dirty + bench changes on slice branch, no commit).
9. Open questions, especially proposed `kCrossoverHz` / `kBandLink`
   final values if the audition suggests tuning.

────────────────────────────────────────
7. ARCHITECT AUDITION + CLOSE
────────────────────────────────────────

avishali auditions per §5.5. On approval, the architect writes a close
prompt: HQ bench commit → push → FF `master`; product DSP commit + HQ
submodule bump → push → FF `main`; PROGRESS.md entry; PLAN.md marks
ADR-0009 / Slice 14 shipped and names the next rung (3-band, or the
linear-phase/STFT "Max Transparency" upgrade, or per-band user controls)
per the audition outcome. **Do not self-close.**
