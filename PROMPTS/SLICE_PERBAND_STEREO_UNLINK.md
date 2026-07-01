# SLICE — Per-band stereo (L/R) unlink + L/R-per-band GR meter (2-band)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` only (no SDK — `LimiterEnvelope` already has everything). No submodule bump.
**Companion:** `docs/SIGNAL_FLOW.md` §2.10–2.14, §7. **ADR:** extends ADR-0009 §4 (deferred *per-band stereo unlink* rung) — Claude adds an ADR-0009 R8 note at close.
**Arc position:** **Slice A2.** Slice A (`a2e6f84`, audition PASSED) shipped per-band *mono* GR viz (N=3 reserved). This slice makes the bands process **L/R independently** and shows **L/R GR per band**. **M/S-per-band is a SEPARATE later slice** — here the band stage stays entirely L/R. Slice B (3rd band) later extends this to mid → 6 sub-bars.

---

## Why
Today the two bands are **stereo-linked**: detection is `peakLow = max(|L|,|R|)` and **one gain** is applied to both channels ([PluginProcessor.cpp:1305](Source/PluginProcessor.cpp:1305), [:1311](Source/PluginProcessor.cpp:1311), [:1356](Source/PluginProcessor.cpp:1356)), so per-band L/R GR would read identical. To show honest per-band L/R — and unlock real per-band stereo processing — the band limiter must compute **per-channel** gains, exactly as the **wideband** stage already does ([:1394–1429](Source/PluginProcessor.cpp:1394)). This slice transposes that proven pattern to the bands, **in Stereo mode only**.

---

## Allowed files to touch
```
Source/PluginProcessor.h / PluginProcessor.cpp
Source/parameters/ParameterIDs.h  Source/parameters/Parameters.cpp
Source/ui/DevControlsComponent.h / .cpp          # DEV Band Stereo Link slider
Source/ui/meters/GainReductionMeter.h / .cpp     # L/R per band, within current footprint
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_PERBAND_STEREO_UNLINK_CLOSE.md      (new, at close)
```
**Non-goals / STOP:**
- **No M/S-per-band.** In **M/S mode** (`stereo_mode==M/S`) the band stage keeps **exactly today's linked path** — untouched. Per-band unlink applies in **Stereo mode only**. (M/S-per-band is its own future slice.)
- No change to the wideband stage or its M/S ceiling-safety ([:1466–1503](Source/PluginProcessor.cpp:1466)).
- **No window/layout change.** The L/R meter fits the current 88px `meterGr_` footprint (tight — intentional). Widening the window + full relayout is the **FOLLOWING UI slice** (`SLICE_UI_WIDEN_RELAYOUT.md`). `meterGr_` bounds + `sync(float)` stay; `MainView` untouched.
- No 3rd band. No latency change. No new **frozen** IDs (DEV only). No history-format change (keep 3 per-band traces = band max of L/R).

---

## 1. DSP — per-band L/R unlink (Stereo mode; transpose the wideband pattern)

### 1a. Envelopes & buffers (`PluginProcessor.h`, prepared in `prepareToPlay`)
Add R-channel band envelopes (mirror `envelope_`/`envelope_R_`): existing `envelopeLow_`/`envelopeHigh_` are the **L / fast-mono** envelopes; add `envelopeLowR_`, `envelopeHighR_`, prepared with the band spec. Add per-channel scratch (mirror `gainBufR_`/`peakBufR_`): `peakLowLBuf_/peakLowRBuf_`, `peakHighLBuf_/peakHighRBuf_`, `gainLowLBuf_/gainLowRBuf_`, `gainHighLBuf_/gainHighRBuf_`, and R-channel Color outputs `gLowOutRBuf_`, `gHighOutRBuf_` (existing single buffers remain the L side). All allocations in `prepareToPlay` — none in the callback.

### 1b. Two band paths, chosen by mode
```cpp
const bool bandUnlink = (! useMsMode) && (nch > 1);   // per-channel path only in Stereo
```
- **`useMsMode` (or mono):** run the **existing linked band code unchanged** (peak = max over channels, single envelope, single-stream Color blend, single-gain apply). Nothing in §1c–1f applies. Publish per-channel GR = the linked value on both L and R (see §3).
- **Stereo (`bandUnlink`):** run the new per-channel path below.

### 1c. Per-channel band detection (Stereo path; replaces the peak collect at [:1284–1307](Source/PluginProcessor.cpp:1284))
Split each channel with `detectCrossover_` into `bandLow[ch]`, `bandHigh[ch]` as today, then take per-channel peaks: `peakLowL=|bandLow[0]|`, `peakLowR=|bandLow[1]|`; `peakHighL=|bandHigh[0]|`, `peakHighR=|bandHigh[1]|`.

### 1d. Per-band fast/slow gain (exact transpose of [:1394–1429](Source/PluginProcessor.cpp:1394))
```cpp
const float bandLinkPct = devBandStereoLinkPct_ ? devBandStereoLinkPct_->load() : 100.0f;
const float bandLink = juce::jlimit (0.0f, 1.0f, bandLinkPct * 0.01f);
const bool  bandFast = bandLink >= 0.9995f;
```
- **bandFast (default):** per band, `peak[i] = max(peakL[i], peakR[i])`, run the **single** existing envelope → `gainL == gainR`. **Bit-identical to today** (max(|L|,|R|) → one envelope).
- **slow (`bandLink<1`):** `envelopeLow_.process(peakLowL,gainLowL)`, `envelopeLowR_.process(peakLowR,gainLowR)` (same for high), then blend toward `min`:
  ```cpp
  const float g = std::min(gainLowL[i], gainLowR[i]);
  gainLowL[i] = bandLink*g + (1-bandLink)*gainLowL[i];
  gainLowR[i] = bandLink*g + (1-bandLink)*gainLowR[i];
  ```

### 1e. Per-channel Color blend (Stereo path; replaces [:1349–1354](Source/PluginProcessor.cpp:1349))
For each channel `c ∈ {L,R}`:
```cpp
gLinked_c    = min(gainLow_c[i], gainHigh_c[i]);
linkedGain_c = bl * gLinked_c;
gLowOut_c[i] = linkedGain_c + (1-bl)*gainLow_c[i];
gHighOut_c[i]= linkedGain_c + (1-bl)*gainHigh_c[i];
```

### 1f. Apply (Stereo path; replaces [:1356–1367](Source/PluginProcessor.cpp:1356))
Split delayed audio per channel via `applyCrossover_` (`lowL,highL,xDelL` / `lowR,highR,xDelR`) and apply each channel's own gains:
```cpp
bandLimitedBuf_[0][i] = duck*(linkedGain_L*xDelL + (1-bl)*(lowL*gainLow_L[i] + highL*gainHigh_L[i]));
bandLimitedBuf_[1][i] = duck*(linkedGain_R*xDelR + (1-bl)*(lowR*gainLow_R[i] + highR*gainHigh_R[i]));
```
`bandLimitedBuf_` stays L/R → the wideband stage is unchanged.

> Latency unchanged. RT-safe: extra scalar work + two more envelope passes only when `bandLink<1` and Stereo; buffers preallocated.

---

## 2. Param — DEV Band Stereo Link
`ParameterIDs.h`/`Parameters.cpp`: `dev_band_stereo_link_pct` — Float `0..100 %`, default **100** (→ bandFast → current behavior). Cache raw pointer + jassert. DEV dock (`DevControlsComponent`) slider **Band Stereo Link**, tooltip *"Per-band L/R unlink (Stereo mode) — 100 = linked (current), 0 = fully independent per band."*

---

## 3. GR taps — per band, per channel
Replace the Slice-A mono per-band current/max atomics with **per-channel**: `currentGrLow{L,R}Db_`, `currentGrHigh{L,R}Db_`, `currentGrMid{L,R}Db_` (mid = 0), plus `maxGrLow{L,R}Db_` etc.; getters `getCurrentGrLowLDb()` …; extend `resetMaxGr()`.

- **Stereo path:** compute `minLow{L,R}`, `minHigh{L,R}` across the block from the per-channel `gLowOut_{L,R}`/`gHighOut_{L,R}`; convert to dB; store current + latch max per channel.
- **M/S / mono path:** the band gain is single-stream — publish that one band GR value to **both** L and R atomics (sub-bars mirror until the M/S-per-band slice).

**History (unchanged 3 traces):** feed `frameMaxGrLowDb_` etc. from the **band max** `max(grLowL, grLowR)`. Total GR (`grL/grR`, `currentGrDb_`, `gDeepBand`) unchanged — in the Stereo path use each channel's own band gains for `gDeepBand`.

---

## 4. GR meter — L/R per band, WITHIN the current 88px footprint (`GainReductionMeter.{h,cpp}`)
Keep `kNumBands = 3`; each band hosts **two sub-bars** (L / R) = 6 thin sub-bars, drawn inside the existing `meterBounds_` — **no bounds/layout change** (the following UI slice gives it room). Members: `displayGrBandChanDb_[kNumBands][2]`, `ball_[kNumBands][2]`, `peakHold_[kNumBands][2]`.
- `sync()` reads per-channel getters for LO/MID/HI × {L,R}; MID = 0 (reserved). Keep bottom total `current/max` readout (unchanged getters).
- `paint()`: three band groups within the current width, each two thin sub-bars + hairline divider, small group captions **LO / MID / HI**, sub-bar micro-labels **L R** (drop the micro-labels if too tight — captions suffice). MID group = two reserved empty slots with the faint `–`. It will be visually tight at 88px — **acceptable and temporary**; the UI-widen slice fixes spacing.
- `resetPeakHolds()` loops `[band][chan]`.

> Labels **L/R** always in A2 (bands are L/R-domain). The later M/S-per-band slice switches them to M/S.

---

## 5. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
```
**Acceptance (Claude verifies 1–5; avishali auditions 6):**
1. Build clean, no new warnings; AU validates; **no latency change** vs `a2e6f84`.
2. **Stereo default (Band Link 100%) null vs `a2e6f84` ≤ −100 dBFS** (bandFast → max(|L|,|R|) → single envelope == old path).
3. **M/S mode is bit-identical to `a2e6f84` at every Band Link** (band path untouched in M/S); meter sub-bars mirror (L=R) in M/S — expected until the M/S-per-band slice.
4. **Independence (Stereo, Band Link < 100%):** hard-pan a transient → that channel's band bar drops while the other holds; both sub-bars track their channel.
5. True-peak ≤ ceiling across Band Link 0/50/100 (Stereo) in SP and TP (wideband unchanged).
6. **Audition:** per-band L/R read is legible; sweep Band Link for the de-correlation feel on a wide mix.

**Close gate:** ADR-0009 R8 note; update `docs/SIGNAL_FLOW.md` §2.10–2.12 + §7; `docs/PROGRESS.md`; `PROMPTS/PLAN.md`; commit plugin-only; archive CLOSE prompt.

---

## Notes for the architect (context, not for Cursor)
- Scoping M/S out keeps A2 a clean null in both modes (Stereo default + all M/S), touching only the Stereo band path. The M/S-per-band slice will encode→unlink→decode inside the band stage (wideband still untouched) and flip the meter labels to M/S.
- Slice B (3rd band) adds `envelopeMidL_/R_` and a MID band-pair to this exact structure; reserved MID meter slots + mid atomics are already here. `SLICE_3BAND_DSP.md` must be revised to per-channel mid before it runs.
