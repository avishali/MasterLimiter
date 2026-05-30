# Cursor Task — Slice 14.4: serial two-lookahead — restore the ceiling guarantee

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Correctness fix, on the existing Slice 14 branches.** Slice 14.1's
> single-lookahead "band-limited estimate" for wideband detection does NOT
> guarantee the ceiling on dense/fast material: the estimate is built from
> UNdelayed bands but applied to DELAYED bands, so on pink noise the wideband
> under-limits and leaks overs (Slice 3: 28,376 sample-peak overs, IMD
> 19.81%). The slow pumping signal masked it. This slice replaces the estimate
> with a **serial two-lookahead topology**: the band stage produces a real
> band-limited signal, and the wideband brickwall detects and limits *that
> actual signal*, guaranteeing the ceiling. Latency roughly doubles (~10 ms),
> within the reserved mastering budget. The Slice 14.3 headroom win
> (hr3 ≈ Ozone) must survive. **Do NOT commit, do NOT push.**

> **Continue on the existing Slice 14 branches**: product
> `slice-14-multiband-2band`, HQ `slice-14-multiband-bench`. No new branches.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product source edits permitted (`Source/PluginProcessor.{h,cpp}`): the
  multiband apply restructure, one new lookahead delay, one new buffer, and
  the reported-latency update.
- HQ `shared/dsp_bench/` edits only if needed to re-run existing tests; no new
  bench features required.
- No `mdsp_dsp` source edits. Do NOT edit ADR-0009 (architect handles the §5
  latency revision).
- RT-safety: all new buffers/delays sized in `prepareToPlay`; no audio-thread
  allocation/logging/locks.
- **Do NOT commit, do NOT push.** `MCP` stays dirty.

────────────────────────────────────────
1. ISOLATION CHECK FIRST (confirm the diagnosis)
────────────────────────────────────────

Before rewriting, confirm the overs are the 14.1 estimate, not the headroom.
Build a headroom-0 variant at BOTH link values and run Slice 3 quick:

```bash
# hr0 already exists at kBandLink 0.0 (build-release-hr0). Also build link 0.5:
cmake -B build-release-hr0-link05 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-DMDSP_BAND_LINK=0.5f -DMDSP_BAND_HEADROOM_DB=0.0f"
cmake --build build-release-hr0-link05 -j

cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
for B in build-release-hr0 build-release-hr0-link05; do
  python bench.py --driver master_limiter --slice 3 --quick \
    --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/$B/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
    --output-dir "runs/SLICE14_4_ISO_$B"
done
```

Expected: **both** show large sample-peak overs (the estimate fails to bound
the ceiling regardless of headroom or link). Report the SP-overs counts. If
hr0 is clean and only headroom>0 overs, STOP and report — the diagnosis is
wrong and the fix below does not apply.

────────────────────────────────────────
2. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `Source/PluginProcessor.cpp` multiband region (~571–708): the detection
  pass, band envelopes, blend, the 14.1 wideband estimate
  (`bandLow·gLowOut + bandHigh·gHighOut` feeding `peakBuf_`), the wideband
  fast-path/unlinked envelope, and the apply pass.
- `Source/PluginProcessor.h`: `lookahead_` (`mdsp_dsp::LookaheadDelay`),
  `envelope_`/`envelope_R_`/`envelopeLow_`/`envelopeHigh_`, the band buffers,
  `baseLatencySamples_`, the `MDSP_BAND_LINK`/`MDSP_BAND_HEADROOM_DB` macros.
- `mdsp_dsp::LookaheadDelay` API (`prepare`, `pushPop(ch, x)`, latency).
- How `baseLatencySamples_` is currently computed in `prepareToPlay` and where
  `setLatencySamples` is called.

Output the retrieval log.

────────────────────────────────────────
3. THE SERIAL TWO-LOOKAHEAD REWRITE (product)
────────────────────────────────────────

Replace the 14.1 estimate-based wideband detection with a true serial chain:
the band stage emits a real band-limited signal; the wideband stage detects
and limits THAT signal with its own lookahead.

### 3.1 New members (`PluginProcessor.h`)

```cpp
mdsp_dsp::LookaheadDelay<float> lookaheadWide_;   // 2nd lookahead: delays the band-limited signal for the wideband brickwall
juce::AudioBuffer<float> bandLimitedBuf_;          // (2 ch, osMax) clean band-limited signal (post band gain, L1-delayed)
```

Prepare in `prepareToPlay` with the same lookahead length and OS rate as
`lookahead_`:
```cpp
lookaheadWide_.prepare (/* same spec as lookahead_ : 2 ch, osSampleRate, lookaheadSamples */);
bandLimitedBuf_.setSize (2, osMaxBlockSize, false, true, true);
```
Reset `lookaheadWide_` wherever `lookahead_` resets.

### 3.2 Reported latency (host PDC)

The audio now passes through **two** lookahead delays in series, so reported
latency increases by one lookahead. Update `baseLatencySamples_` to include
the second lookahead (e.g. `+ lookaheadSamples` at OS rate, converted to host
rate exactly as the first lookahead is). Verify the latency round-trips: a
dry impulse through the plugin should report the new, larger latency
consistently, and the bench `latency_samples` measurement should match
`setLatencySamples`.

### 3.3 The four-pass structure (processBlock)

Restructure the multiband region into:

**Pass A — band detection (undelayed) + band envelopes (unchanged from 14.x):**
detect bands via `detectCrossover_` → `peakLow`/`peakHigh` (stereo-linked) →
`envelopeLow_`/`envelopeHigh_` → `gainLow`/`gainHigh` → blend into
`gLowOut`/`gHighOut` buffers (keep the existing `kBandLink` blend and the
`bandThresholdLin` headroom from 14.3).

**Pass B — band apply → real band-limited signal (replaces the old estimate):**
```cpp
for (int i = 0; i < osN; ++i)
    for (int ch = 0; ch < nch; ++ch)
    {
        const float delayed = lookahead_.pushPop (ch, osBlock.getChannelPointer((size_t)ch)[i]);
        float lo = 0.0f, hi = 0.0f;
        applyCrossover_.processSample (ch, delayed, lo, hi);
        bandLimitedBuf_.getWritePointer (ch)[i] = lo * gLowOut[i] + hi * gHighOut[i];
    }
applyCrossover_.snapToZero();
```
`bandLimitedBuf_` is now the clean band-limited signal, delayed by L1. This is
exactly what the wideband stage will limit — no estimate.

**Pass C — wideband detection FROM the real band-limited signal:**
```cpp
auto* peakWideL = peakBuf_.getWritePointer (0);
auto* peakWideR = peakBufR_.getWritePointer (0);
for (int i = 0; i < osN; ++i)
{
    peakWideL[i] = std::abs (bandLimitedBuf_.getReadPointer(0)[i]);
    peakWideR[i] = (nch > 1) ? std::abs (bandLimitedBuf_.getReadPointer(1)[i]) : peakWideL[i];
}
```
Then run the EXISTING fast-path / unlinked wideband envelope block unchanged
(fast-path takes `max(peakWideL, peakWideR)`; unlinked keeps per channel with
the link blend) → `gainBuf_` / `gainBufR_` and the GR snapshots, exactly as
now — only its input changed to the real band-limited peaks.

**Pass D — wideband apply (brickwall) through the 2nd lookahead:**
```cpp
for (int i = 0; i < osN; ++i)
    for (int ch = 0; ch < nch; ++ch)
    {
        const float bl = bandLimitedBuf_.getReadPointer (ch)[i];
        const float wideDelayed = lookaheadWide_.pushPop (ch, bl);
        const float wideGain = (ch == 0) ? gainBuf_.getReadPointer(0)[i]
                                          : (fastPath ? gainBuf_.getReadPointer(0)[i]
                                                      : gainBufR_.getReadPointer(0)[i]);
        osBlock.getChannelPointer((size_t)ch)[i] = wideDelayed * wideGain;
    }
```

The wideband envelope detects `bandLimitedBuf_` (Pass C) and limits the
`lookaheadWide_`-delayed copy of the same buffer (Pass D), with its own
lookahead — so it limits exactly what it detected. **Ceiling guaranteed.**

### 3.4 Invariants

- Order is fixed: bands → wideband. Never the reverse.
- Two physical lookahead delays in series (`lookahead_` then `lookaheadWide_`).
  Reported latency reflects both.
- GR snapshots stay wideband-driven. No UI change. No new params.
- With kBandHeadroomDb large and kBandLink 0, the bands do most of the work
  and the wideband is a light catch — but it still bounds every sample to the
  ceiling because it limits the real summed signal.

────────────────────────────────────────
4. BUILD & VERIFY
────────────────────────────────────────

### 4.1 Build
```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j   # default: kBandLink 0.0, kBandHeadroomDb 2.0
```
Zero new `Source/` warnings.

### 4.2 Ceiling restored — Slice 3/4/5 (the gate that failed)

Re-run Slice 3/4/5 quick on the default build. **Sample-peak / true-peak overs
must collapse back to the small single-band counts** (the 28k overs must be
gone). Then re-tighten the Slice 14 threshold loosenings to the smallest that
still pass; transient crest should recover well off −10 dB now that the
ceiling holds and the over-squash is gone. Report before→after; IMD/THD must
not be loosened.
```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE14_4_REGRESSION_S$SLICE"
done
```
If overs do NOT collapse, STOP and report — the serial chain is mis-wired.

### 4.3 Pumping win must survive — re-run the headroom sweep

Rebuild the headroom sweep set (hr0/1/2/3/4 at kBandLink 0.0) with the serial
chain and re-run the Slice 14.2/14.3 pumping diagnostic. Confirm
`hf_ducking_modulation_db` still reaches ~Ozone (−22.8) around hr3, and that
TP dBFS now stays under the ceiling at every headroom (the ceiling fix should
make TP compliant everywhere). Report the table; flag if the win regressed.

### 4.4 Latency sanity
Confirm reported latency increased by ~one lookahead and that the bench's
measured latency matches `setLatencySamples` (no PDC mismatch / no smearing).

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. §1 isolation result (SP overs at hr0 link 0.0 and link 0.5).
3. Product diff: new `lookaheadWide_` + `bandLimitedBuf_`, the four-pass
   restructure (before/after of wideband detection + apply), latency update.
4. Build summary (Debug + Release; sweep rebuild).
5. §4.2 Slice 3/4/5: overs counts (must collapse), before→after thresholds,
   IMD/THD not loosened.
6. §4.3 pumping sweep table with the serial chain (hr0–hr4) — HF ducking +
   TP dBFS; confirm hr3 ≈ Ozone survives and TP is compliant.
7. §4.4 latency before/after and measured-vs-reported match.
8. `git status --short` product + HQ; list extra build dirs.
9. Open questions.

────────────────────────────────────────
6. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

Architect confirms the ceiling holds (overs gone), the pumping win survived,
and the latency is sane, then picks the final `kBandHeadroomDb` + `kBandLink`
from the (now ceiling-correct) sweep. avishali auditions the candidate build
(de-pumping audible, tone stable, no overs/clicks, latency acceptable). On
approval → architect revises ADR-0009 §5 (latency) + writes the Slice 14
consolidated close. **Do not self-close, do not pick the final constants, do
not edit ADR-0009.**
