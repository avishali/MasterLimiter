# Cursor Task — Slice 7: Stereo Unlink (per-channel parallel envelopes + Link % param)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Slice 7: stereo width preservation under limiting via per-channel
> parallel envelopes with continuous Link % blend.** Closes the
> dominant width gap vs Ozone documented in the Slice 9.6c reference
> run.
>
> **Two commits this slice:**
> 1. **HQ:** ADR-0008 documenting the detection topology decision
>    (per-channel parallel envelopes, gain blend, single Link % param,
>    M/S deferred, latency constant). No HQ DSP source changes —
>    `LimiterEnvelope` is reused as a stateful component, two
>    instances on the product side.
> 2. **Product:** New `stereo_link_pct` param (frozen ID, 0..100, default
>    100), second `LimiterEnvelope` instance, per-channel peak buffers
>    + gain buffers, blend in processBlock, fast-path at link == 100,
>    UI knob in Maximizer panel.
>
> **Default behaviour is byte-identical to Slice 9.** At link = 100
> the fast-path runs the single max(|L|,|R|) envelope as today; Slice
> 3/4/5 bench stays bit-exact PASS without recalibration.
>
> Audition + bench gate before push.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ ADR
written first, separate commit. Product change second, separate
commit. No HQ DSP source edits (envelope reused). No bench source
edits. Both commits stay local until avishali approves audition; close
prompt handles pushes.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

Slice 9.6c bench reference run measured Ozone 11 Maximizer IRC IV on
the same synthetic corpus as MasterLimiter and showed:
- Null residual K-weighted (pink): Ozone −22.3 dB vs MasterLimiter
  −15 dB (~7 dB tighter).
- Sample peak overs (Slice 3 SP path): Ozone 0 vs MasterLimiter 1044.

avishali's perceptual audition characterised this as Ozone sounding
"more open and wide" on the same source. The decomposition (from the
Slice 9 close brainstorm) attributes the "wide" component primarily to
**stereo unlinking**: Ozone applies independent gain envelopes per
channel (or per M/S), so a transient on only one channel doesn't drag
both channels down. MasterLimiter currently links 100%: `PeakDetector`
returns `max(|L|, |R|)` and one envelope's gain stream multiplies both
channels. Any one-sided transient narrows the stereo image toward
mono.

Pro-L 2 ("Channel Linking %") and Ozone Maximizer ("Stereo Unlink %")
both expose this as a continuous percentage. We match.

The "open" component (HF/spectral preservation, the residual gap)
attributes to multiband detection, addressed in a future ADR-0008
companion slice — outside Slice 7 scope.

────────────────────────────────────────
2. ARCHITECTURE DECISIONS (ADR-0008 content)
────────────────────────────────────────

These are the locked-in choices to write into ADR-0008 verbatim.

**Decision 1: per-channel parallel envelopes.**

Two `mdsp_dsp::LimiterEnvelope` instances on the product side
(`envelope_L_`, `envelope_R_`), prepared identically (same sample
rate at OS, same lookaheadSamples, same maxBlockSize). Each runs on
its channel's own peak stream (`|L|` for envelope_L, `|R|` for
envelope_R).

Per-channel peak detection: inline in processBlock,
`peakL[i] = |osL[i]|`, `peakR[i] = |osR[i]|`. Reusing
`mdsp_dsp::PeakDetector` is optional — its current API computes
`max(|L|, |R|)`; a manual per-channel loop is simpler than refactoring
the HQ class.

**Decision 2: gain blend after independent envelopes.**

```
gain_linked = min(gain_L, gain_R)              // deeper reduction wins
gain_out_L  = link * gain_linked + (1-link) * gain_L
gain_out_R  = link * gain_linked + (1-link) * gain_R
```

At link = 1.0: both channels get `gain_linked` (the deeper of the
two). At link = 0.0: each channel gets its own envelope's gain
independently. Continuous blend between.

Gain values are 0..1 where 1 = no reduction, 0 = full mute. `min` of
two gains picks the lower value = deeper reduction. This guarantees
the linked channel never under-attenuates a peak that occurred on
either channel.

**Decision 3: fast-path at link = 100 for bit-exact compatibility.**

When `stereo_link_pct >= 99.95` (effectively 100 with float epsilon),
skip the second envelope entirely. Run only the single max-peak
envelope as today and apply its gain to both channels. This preserves:
- Bit-exact behaviour at the default 100 setting (Slice 3/4/5 bench
  PASS unchanged).
- ~½× CPU at the default (only one envelope runs, like today).
- Zero perceptual discontinuity at the 99.95 fast-path threshold (the
  two paths converge analytically as link → 1).

Below 99.95: full per-channel path. ~2× envelope CPU. Documented as
the cost of unlinking.

**Decision 4: single continuous param, no mode selector, M/S deferred.**

`stereo_link_pct` is the only new param. `AudioParameterFloat`, range
`0.0 … 100.0`, step `0.1`, default `100.0`. Frozen ID per system
rules. Displayed as a percentage in UI.

M/S detection is a separate axis (orthogonal to per-channel unlink)
and a separate user mental model. Defer to Slice 7b or later if
demanded. Not in Slice 7 scope.

**Decision 5: constant latency across all link values.**

Both envelopes use the same lookahead. The fast-path single-envelope
run uses the same lookahead. No PDC change on link % automation.

**Decision 6: detection-bus only, no audio-path crossover.**

Stereo split lives entirely in the gain-computation path. Audio is
not band-split, M/S-encoded, or otherwise pre-processed. Audio passes
through the existing OS region (Drive → Clipper → Lookahead → gain
multiply → downsample) untouched except for the two-channel gain
multiplication at the end.

This mirrors ADR-0005's reasoning for T/S split (avoid LR4 phase
issues by keeping the split in detection only). Future multiband
detection (ADR-0008 companion slice) can layer on top without
disturbing this stereo topology.

────────────────────────────────────────
3. SCOPE — files
────────────────────────────────────────

**CREATE (HQ):**
- `third_party/melechdsp-hq/docs/DECISIONS/ADR-0008-masterlimiter-stereo-unlink.md`
  — write the full ADR per §4 below.

**MODIFY (product):**
- `Source/parameters/ParameterIDs.h` — add `stereo_link_pct`.
- `Source/parameters/Parameters.cpp` — declare `stereo_link_pct`
  AudioParameterFloat, range 0..100, step 0.1, default 100.0.
- `Source/PluginProcessor.h` — add `mdsp_dsp::LimiterEnvelope
  envelope_R_;` alongside the existing `envelope_` (rename existing
  to `envelope_L_` for clarity, OR keep `envelope_` as the L channel
  and name the new one `envelope_R_`; Cursor picks the cleaner of the
  two and is consistent). Add `juce::AudioBuffer<float>
  peakBufR_, gainBufR_;` for the R channel buffers (the existing
  `peakBuf_` / `gainBuf_` become the L channel). Add pointer
  `std::atomic<float>* stereoLinkPct_` initialised in `prepareToPlay`.
- `Source/PluginProcessor.cpp`:
  - `prepareToPlay`: prepare `envelope_R_` with identical spec to the
    L envelope. Allocate per-channel buffers identically. Look up
    `stereoLinkPct_` raw atomic from APVTS.
  - `processBlock`: replace the single peak/envelope path inside the
    `limiterActive == true` branch with the §5.3 sketch below.
    Maintain the OS up/down wrap, Clipper, Drive, ispTrim removal,
    etc. all unchanged.
- `Source/ui/MainView.h/.cpp` (or wherever the Maximizer panel knob
  row lives): add a new horizontal slider / rotary "Link" with
  `stereo_link_pct` APVTS attachment, formatted as `XX%`. Place it
  in the Maximizer panel alongside Character / Clipper Drive. Match
  the existing knob style. No layout re-flow — fit it into the
  existing knob row (Maximizer panel has space per the Slice 10 UI
  shell).

**DO NOT TOUCH:**
- `shared/mdsp_dsp/...` — HQ DSP source untouched. `LimiterEnvelope` is
  reused as-is.
- Bench files. Default 100 = fast-path = bit-exact, no recalibration
  needed.
- Any other parameter ID, range, step, or default.
- I/O trim path, metering, GR meter math (the GR meter atomic now
  pulls `max(envelope_L_.getLastBlockMaxGrDb(),
  envelope_R_.getLastBlockMaxGrDb())` instead of one envelope — see
  §5.3).

────────────────────────────────────────
4. ADR-0008 CONTENT
────────────────────────────────────────

Write `ADR-0008-masterlimiter-stereo-unlink.md` with this structure
(adapt to the prior ADR house style by reading ADR-0006 / ADR-0007
first):

```markdown
# ADR-0008: MasterLimiter — Stereo Link / detection topology

## Status
Accepted (2026-05-29)

## Context
[Summarise the Slice 9.6c reference run: Ozone width gap, 7 dB null
residual gap, perceptual "wide" attribution. Cite the in-repo
location of the run output for traceability.]

## Decision

### 1. Per-channel parallel envelopes
[Decision 1 from §2 of the slice prompt, expanded.]

### 2. Gain blend after independent envelopes
[Decision 2, including the math formulae verbatim.]

### 3. Fast-path at link = 100 for bit-exact default
[Decision 3, justifying the 99.95 threshold.]

### 4. Single continuous param; no mode selector; M/S deferred
[Decision 4, justifying the choice vs Pro-L's "Channel Linking %"
naming and Ozone's "Stereo Unlink %".]

### 5. Constant latency
[Decision 5.]

### 6. Detection-bus only, no audio-path crossover
[Decision 6, referencing ADR-0005's parallel reasoning.]

### 7. CPU cost
- link = 100 (default): same as Slice 9 (one envelope).
- link < 99.95: ~2x envelope CPU (two envelopes run). Acceptable
  trade-off; user opts in by dialing down.

### 8. UI
- Maximizer panel, knob row, "Link" label, "XX%" readout.
- Default reads "100%" (full link).

## Consequences

**Easier:**
- Closes the dominant "wide" gap vs Ozone with a single user-facing
  knob.
- Bit-exact backward compatibility at default.

**Harder / risks:**
- 2x envelope CPU when unlinked. On heavy material in Clean mode
  (already the most expensive after Slice 9.4.2) this could push
  CPU into uncomfortable territory; mitigated by fast-path at
  default and the Slice 9.4.1 outer-loop fast-path.
- At link = 0, asymmetric stereo material can produce different
  loudness per channel post-limit (each channel sees its own
  ceiling enforcement). This is the intended behaviour but worth
  noting for documentation.

## Alternatives Considered

- M/S detection. Rejected for Slice 7 (deferred to 7b or later) on
  the grounds that per-channel unlink is the more direct response
  to the observed "wide" gap, and adding two stereo topologies in
  one slice doubles scope.

- Single-envelope blended-detection. Rejected: blending detection
  inputs while still applying one gain to both channels does not
  actually unlink the output; it's a half-measure that costs
  complexity without delivering the perceptual win.

- Crossfeed detection (asymmetric). Rejected: more complex param
  surface (which channel feeds how much into which), and the gain-
  blend formulation captures the same behavioural continuum more
  simply.

## References
- ADR-0005 (T/S split, detection-bus reasoning).
- ADR-0006 (character modes, T/S inertness under max).
- Slice 9.6c reference bench run (Ozone IRC IV vs MasterLimiter on
  same corpus): `runs/SLICE09_6c_OZONE_*`.
- PLAN row for Slice 7.
```

────────────────────────────────────────
5. WHAT TO DO — implementation details
────────────────────────────────────────

### 5.1 ParameterIDs.h

Add:
```cpp
inline constexpr auto kStereoLinkPct = "stereo_link_pct";
```
(or match the existing naming convention for ID constants — Cursor
verifies by reading the file).

### 5.2 Parameters.cpp

Add to the params layout vector:
```cpp
params.push_back (std::make_unique<juce::AudioParameterFloat> (
    juce::ParameterID { ParameterIDs::kStereoLinkPct, 1 },
    "Stereo Link",
    juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
    100.0f,
    juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([](float v, int) {
            return juce::String (v, 0) + "%";
        })));
```

ID, range, default, label as specified. Match the surrounding param-
declaration style.

### 5.3 PluginProcessor.cpp — processBlock structure

Inside `if (limiterActive_->get())` (current Slice 9.6 wiring):

```cpp
// ... Drive at 1× (unchanged) ...
// ... upsample to OS (unchanged) ...
// ... Clipper at OS (unchanged) ...

const int osN = (int) osBlock.getNumSamples();
const float linkPct  = stereoLinkPct_->load (std::memory_order_relaxed);
const float link     = juce::jlimit (0.0f, 1.0f, linkPct * 0.01f);
const bool  fastPath = (link >= 0.9995f);

// Per-channel peak streams (inline; PeakDetector reused only for the
// fast-path single-envelope case to preserve bit-exactness with Slice 9).
auto* osCh0 = osBlock.getChannelPointer (0);
auto* osCh1 = (nch > 1) ? osBlock.getChannelPointer (1) : osCh0;

// Envelope thresholds for both envelopes (same headroom logic from 9.6)
const float ceilingLin   = juce::Decibels::decibelsToGain (ceilingDbParam_->get());
const float thresholdLin = (modeIdx == 1) ? (ceilingLin * 0.965f) : ceilingLin;

envelope_L_.setThresholdLinear (thresholdLin);
envelope_L_.setReleaseMs (readFloatParam (apvts, "release_ms"));
envelope_L_.setReleaseSustainRatio (releaseSustainRatio_->get());
envelope_L_.setMode (mapCharacterIndexToMode (characterChoice_->getIndex()));

if (fastPath)
{
    // Bit-exact Slice 9.6 path: single envelope on max(|L|, |R|), shared gain.
    auto* peak = peakBuf_.getWritePointer (0);   // existing L buffer reused as the "max" buffer
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

    currentGrDb_.store (envelope_L_.getLastBlockMaxGrDb(), std::memory_order_relaxed);
}
else
{
    // Per-channel envelopes + gain blend
    envelope_R_.setThresholdLinear (thresholdLin);
    envelope_R_.setReleaseMs (readFloatParam (apvts, "release_ms"));
    envelope_R_.setReleaseSustainRatio (releaseSustainRatio_->get());
    envelope_R_.setMode (mapCharacterIndexToMode (characterChoice_->getIndex()));

    auto* peakL = peakBuf_.getWritePointer (0);
    auto* peakR = peakBufR_.getWritePointer (0);
    for (int i = 0; i < osN; ++i)
    {
        peakL[i] = std::abs (osCh0[i]);
        peakR[i] = std::abs (osCh1[i]);
    }

    auto* gainL = gainBuf_.getWritePointer (0);
    auto* gainR = gainBufR_.getWritePointer (0);
    envelope_L_.process (peakL, gainL, osN);
    envelope_R_.process (peakR, gainR, osN);

    // Blend + apply
    auto* dL = osBlock.getChannelPointer (0);
    auto* dR = (nch > 1) ? osBlock.getChannelPointer (1) : dL;
    const float oneMinusLink = 1.0f - link;

    for (int i = 0; i < osN; ++i)
    {
        const float gLinked = std::min (gainL[i], gainR[i]);
        const float gOutL   = link * gLinked + oneMinusLink * gainL[i];
        const float gOutR   = link * gLinked + oneMinusLink * gainR[i];

        const float delayedL = lookahead_.pushPop (0, dL[i]);
        dL[i] = delayedL * gOutL;

        if (nch > 1)
        {
            const float delayedR = lookahead_.pushPop (1, dR[i]);
            dR[i] = delayedR * gOutR;
        }
    }

    currentGrDb_.store (
        std::max (envelope_L_.getLastBlockMaxGrDb(),
                  envelope_R_.getLastBlockMaxGrDb()),
        std::memory_order_relaxed);
}

// ... downsample (unchanged) ...
// ... 9.6 has no ispTrim block ...
```

In the `limiterActive == false` branch, no envelope state changes
needed.

### 5.4 prepareToPlay

Where the existing envelope is prepared, prepare envelope_R_ with the
identical Spec. Allocate `peakBufR_` and `gainBufR_` to `osMaxBlock`
samples (same as the L buffers).

Look up the new param atomic:
```cpp
stereoLinkPct_ = apvts.getRawParameterValue ("stereo_link_pct");
jassert (stereoLinkPct_ != nullptr);
```

### 5.5 UI knob

In whichever component owns the Maximizer panel knob row (locate by
searching for `"character"` or `"clipper_drive_db"` attachments):
- Add a `juce::Slider stereoLinkSlider_` member.
- Add a `juce::AudioProcessorValueTreeState::SliderAttachment
  stereoLinkAttach_` member.
- In `setupSliders()` / constructor, set the slider style to match the
  existing Character / Clipper knobs (rotary or horizontal, whichever
  the row uses), suffix formatting "%", and attach to `stereo_link_pct`.
- In `resized()` / layout, allocate a slot next to Clipper Drive.
  Maintain the existing locked aspect / scale behaviour from Slice 10.
- Label "Link" or "Stereo Link" — Cursor picks the cleaner of the two
  given the available label width.

────────────────────────────────────────
6. BUILD
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean.

────────────────────────────────────────
7. BENCH (must remain PASS at default)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE07_REGRESSION_S$SLICE"
done
```

All three must PASS at the same counts as Slice 9 close (13/13, 14/14,
25/25). Default link = 100 → fast-path → bit-exact identical to Slice
9.6. If any bench row fails, the fast-path is not bit-exact — STOP
and report.

The master_limiter driver does NOT need to pin `stereo_link_pct`; the
default is 100 which is what the bench assumes. If a future bench
slice tests unlinked behaviour the driver will be updated then.

────────────────────────────────────────
8. COMMITS — two separate, no push, no amend
────────────────────────────────────────

### 8.1 HQ ADR commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# expect ONLY:
#    ?? docs/DECISIONS/ADR-0008-masterlimiter-stereo-unlink.md

git add docs/DECISIONS/ADR-0008-masterlimiter-stereo-unlink.md

git commit -m "$(cat <<'EOF'
ADR-0008: MasterLimiter stereo unlink (per-channel parallel envelopes)

Locks the Slice 7 detection topology: per-channel parallel envelopes
with gain blend across a continuous Link % param. Bit-exact at the
default (link = 100, single-envelope fast-path). Detection-bus only,
no audio-path crossover (mirrors ADR-0005 reasoning). Constant
latency. M/S detection deferred to a later slice as an orthogonal
axis. Closes the dominant "wide" gap vs Ozone documented in the
Slice 9.6c reference run.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### 8.2 Product commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect (approximately):
#    M Source/parameters/ParameterIDs.h
#    M Source/parameters/Parameters.cpp
#    M Source/PluginProcessor.h
#    M Source/PluginProcessor.cpp
#    M Source/ui/<MainView or relevant panel>.h/.cpp

git add Source/parameters/ParameterIDs.h Source/parameters/Parameters.cpp \
        Source/PluginProcessor.h Source/PluginProcessor.cpp \
        Source/ui/*

git commit -m "$(cat <<'EOF'
MasterLimiter Slice 7: Stereo Link % (per-channel parallel envelopes)

Adds stereo width preservation under limiting via per-channel
parallel envelopes with continuous Link % blend, per ADR-0008.

New param:
- stereo_link_pct (float, 0..100, step 0.1, default 100, frozen ID).
  Displayed as XX% in UI.

DSP:
- Second LimiterEnvelope instance (envelope_R_) prepared identically
  to the existing envelope (envelope_L_). Per-channel peak / gain
  buffers (peakBufR_, gainBufR_) sized to OS block.
- processBlock fast-path at link >= 99.95: bit-exact Slice 9.6
  behaviour (single envelope on max(|L|, |R|), shared gain). Default
  100 lands here.
- processBlock full path at link < 99.95: per-channel envelopes,
  gain blend
      g_linked = min(g_L, g_R)
      g_out_L  = link * g_linked + (1 - link) * g_L
      g_out_R  = link * g_linked + (1 - link) * g_R
  ~2x envelope CPU when unlinked (one-time opt-in cost).

UI:
- New "Link" knob in Maximizer panel knob row, alongside Character
  and Clipper Drive. APVTS-attached, "XX%" readout.

GR meter:
- currentGrDb_ now reports max across both envelopes (whichever
  channel hit deeper reduction). At default 100 / fast-path, this
  equals the single-envelope GR as today.

Bench:
- Slice 3/4/5 PASS 13/13, 14/14, 25/25 unchanged. Default 100 takes
  the fast-path, so bit-exact identical to Slice 9.6.

Constant latency across link values (both envelopes share lookahead;
fast-path uses same lookahead). No PDC change on Link %
automation.

Detection-bus only - audio path is not band-split or M/S-encoded.
Mirrors ADR-0005's reasoning for T/S split: keep the split in
detection only to avoid LR4-style phase issues.

M/S detection (a separate stereo topology axis) deferred per
ADR-0008 §Alternatives. Future Slice 7b if ever demanded.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push HQ. Do NOT push product. Do NOT amend any prior commit.**
Slice 7 close prompt handles pushes after avishali audition.

────────────────────────────────────────
9. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. New ADR file: line count + section headings.
2. Diff summary per modified file (params, processor, UI).
3. Build success lines for Release and Debug.
4. Bench PASS lines for Slice 3 / 4 / 5 — must all pass at the prior
   counts (13/13, 14/14, 25/25). If any fail, STOP and report.
5. HQ `git log --oneline -3` showing the new ADR commit on top.
6. Product `git log --oneline -4` showing the new Slice 7 commit on
   top.
7. Reported latency at 48k for both SP and TP — should be unchanged
   from Slice 9.6 (301 / 301).
8. Confirmation: no HQ DSP source touched; no bench source touched;
   neither repo pushed; no amends.
9. Open questions.

────────────────────────────────────────
10. AFTER THIS
────────────────────────────────────────

avishali auditions in Live (Release VST3):

- **Default 100% link**: should sound identical to Slice 9.6. If
  any audible difference at link = 100, the fast-path is not bit-
  exact and we investigate.
- **Sweep Link 100 → 50 → 0** on a wide-stereo source (drums with
  panned cymbal hits, or a wide mix with sided percussion):
  - 100: current narrowing on transients.
  - 50: noticeable width preservation, mix stays wider on hits.
  - 0: full preservation, transients only duck the channel they
    occur on. Possibly audible loudness imbalance on heavily-sided
    material (intended; the trade-off of full unlink).
- **A/B vs Ozone Stereo Unlink** at matched LUFS: width sensation
  should now be in family. Any residual gap is the "open"
  (multiband / spectral) axis, addressed in a future slice.
- **CPU at link < 100**: expect noticeably higher than default
  (~2x envelope), particularly in Clean (already the most
  expensive). If CPU climbs unacceptably, fast-path Slice 7
  follow-up: per-channel SIMD on the envelope inner loop, or
  short-circuit second envelope when both channels' peaks
  identically equal the max (rare in real material but free win).
- **Param automation**: drag link 100 → 0 mid-playback. Should
  glide smoothly without zipper / clicks. The fast-path threshold
  at 99.95 is one discontinuity; smooth blend below that should
  hide it.

On audition pass → Slice 7 close (push HQ, push product, PROGRESS +
PLAN updates marking Slice 7 done and bumping multiband detection
to "next" or whatever you decide is next).

On partial pass (works but CPU too high, or audible glitch on
automation) → small follow-up slice within 7 family before close.
