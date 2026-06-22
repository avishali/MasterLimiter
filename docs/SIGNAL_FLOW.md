# MasterLimiter — Signal Flow & Component Reference

**Version:** v0.3.1 (beta) · **Audience:** avishali + advanced testers (full mental model of the plugin) · **Maintained by:** Claude (architect)
**Source of truth:** `Source/PluginProcessor.cpp` (`processCore`), `Source/parameters/Parameters.cpp`, and the SDK `melechdsp-hq/shared/mdsp_dsp/`. If code and this doc disagree, the code wins — tell Claude to re-sync.

---

## 0. Mental model in one paragraph
MasterLimiter is a **mastering maximizer**. Audio comes in, optionally gets soft/hard **clipped**, gets an **input gain** push, is **4× oversampled** (so all limiting math runs at ~192 kHz to avoid aliasing), is split into **two bands at 120 Hz**, each band gets its own **limiter envelope** (a gain-reduction curve), the two bands are blended back toward wideband by the **Color** knob, then a **wideband limiter** catches whatever is left, an **output Ceiling** gain is applied, the signal is **downsampled** back to host rate, and a final **true-peak brickwall** guarantees nothing exceeds the ceiling. In parallel, a **dry copy** is kept delay-aligned for the **bypass cross-fade**, and **LUFS loudness** is measured for optional **auto gain-match**. Everything you see on the meters is tapped at specific points in this chain.

---

## 1. Oversampling & latency (the frame everything runs in)

- **Oversampler:** `juce::dsp::Oversampling<float>`, **4×**, built from **two half-band FIR equiripple stages** (`PluginProcessor.cpp` ~L140–152):
  - **Stage 1** — transition width `0.03`, attenuation `−110 dB`. Tight, because it straddles the audible top octave.
  - **Stage 2** — transition width `0.10`, attenuation `−100 dB`. Wider/cheaper, transparent at the higher rate.
  - Flat to 20 kHz; `setUsingIntegerLatency(true)` so latency is a whole number of samples.
- **Why 4×:** limiting is a nonlinear operation; doing it at 4× pushes the aliasing products far above the audible band so they can be filtered on the way down. (Known limit: at 4× some limiter harmonics still fold back — see §8.)
- **Total plugin latency** = `2 × max lookahead` + oversampler latency + FinalCeiling (lookahead 64 + its detector). The reported latency is fixed at the 6 ms maximum lookahead so DEV tuning does not force host PDC changes.
- **Lookahead** has two DEV-tunable active windows, both defaulting to 5 ms: `dev_lookahead_band_ms` drives the per-band delay/envelope window, and `dev_lookahead_wide_ms` drives the wideband delay/envelope window. Both sweep 0.00…6.00 ms in 0.01 ms steps; 0.00 ms maps internally to the one-oversampled-sample minimum so the delay and envelope window stay aligned. The wet path is padded by the unused slack so total latency stays constant.

---

## 2. Full signal flow, in order

Each numbered step is what actually happens to a block in `processCore`. Steps **2.1–2.5** run at host rate; **2.6–2.14** run inside the 4× oversampled block; **2.15+** are back at host rate.

**2.1 — I/O Input gain & trim.** `applyIoInputGain()` applies the I/O Input L/R (with optional link). This is your calibration trim before anything else.

**2.2 — Dry capture (delay-aligned).** A copy of the (post-input-trim) signal is pushed through `dryDelay_` (a delay line set to the plugin's total latency) into `dryScratch_`. This keeps the dry signal sample-aligned with the processed signal so the **Bypass cross-fade** (2.17) is phase-correct.

**2.3 — Input metering tap.** Input **peak**, **RMS** (smoothed mean-square), and **true-peak** (4× ISP) for L/R are measured here and published to atomics for the meters. Max-true-peak holds are latched.

**2.4 — Loudness reference.** `loudnessRef_` measures the dry input LUFS (used for the learn reference and dry compensation).

**2.5 — Ceiling-mode latency check.** SP↔TP mode switches keep latency fixed (no re-report glitch).

> If the **Limiter Active** switch is OFF, the chain skips to 2.15 with the dry signal copied through. Everything below is inside `if (limiterActive_)`.

**2.6 — Upsample 4×.** `processSamplesUp` → `osBlock` at 4× rate, `osN` samples.

**2.7 — Clipper (optional, pre-limiter).** Gated by **Clipper Active**. Per sample: multiply by **drive** (`clipper_drive_db`, applied as a *boost into* the clipper), then:
  - **Hard** (`mode 0`): `y = sign(x)` when `|x| > 1`.
  - **Soft** (`mode 1`): tanh soft-knee above **0.891** (≈ −1 dBFS): `y = sign(x)·(0.891 + 0.109·tanh(over))`. Smooth saturation, asymptotic to ±1.
  Then divide back out by the drive (un-drive) so the clipper adds harmonics/density without net level change. The peak attenuation it caused is metered as **Clip dB**.

**2.8 — Input gain.** `input_gain_db` (0–24 dB), smoothed, multiplied in. **This is the knob that drives gain reduction** (the limiter threshold is fixed at 1.0 — see §3).

**2.9 — 2-band split for detection.** `detectCrossover_` (Linkwitz-Riley, 120 Hz) splits each channel into **low/high**; per-sample band peaks (`peakLow`, `peakHigh`) are collected. `snapToZero()` clears filter state at block end.

**2.10 — Per-band limiter envelopes.** `envelopeLow_.process()` and `envelopeHigh_.process()` turn the band peak streams into **gain coefficients** (`gainLow`, `gainHigh`, each ≤ 1.0). These are *gain computers only* — they output a gain curve, they don't touch audio yet. (Details: §4.3.)

**2.11 — Color / band-link blend.** Per sample, `bandLinkSmoothed_` = `mapBandColorToLink(color)` = `1 − color/100`:
  - **Color 0% → fully linked** (`bl = 1`): both bands take `min(gainLow, gainHigh)` → behaves like a transparent wideband limiter.
  - **Color 100% → independent** (`bl = 0`): each band keeps its own gain → multiband character (the loudness-cap-breaking, low-IMD mode).
  The blend is applied both to the gains and to the audio reconstruction.

**2.12 — Lookahead delay + band gain application.** The audio is delayed by `lookahead_` using the active `dev_lookahead_band_ms` window (so the gain curve, computed from the *future* peak, lands *before* the peak arrives — that's the lookahead attack). `applyCrossover_` re-splits the delayed audio and applies `gainLow`/`gainHigh`, blended by `bl` against the un-split delayed signal → `bandLimitedBuf_`.

**2.13 — Wideband limiter.** The band-limited signal is peak-detected again (or M/S-encoded if **Stereo Mode = M/S**) and run through `envelope_` (and `envelope_R_` when stereo unlinked). **Stereo Link / M/S Link** (%) blends L/R gain toward `min(L,R)`; at ≥99.95% it takes a single-envelope fast path. This is the safety stage that catches anything the per-band stage let through.

**2.14 — Wideband lookahead + Ceiling + GR tap.** Audio is delayed by `lookaheadWide_` using the active `dev_lookahead_wide_ms` window, multiplied by the wideband gain and by **`ceilingLin`** (the output ceiling gain). In M/S mode a per-sample decoded-peak safety clamp guarantees the reconstructed L/R never exceeds threshold. The **total gain reduction** (`min(bandGain × wideGain)` over the block, L/R) is published to `currentGrDb_/LDb_/RDb_` and the max-since-reset is latched — **this is the GR the meters and the new history graph read.** `lookaheadPad_` then adds `(max−band) + (max−wide)` samples of wet-path delay before downsampling so the active windows remain latency-constant.

**2.15 — Downsample.** `processSamplesDown` returns to host rate.

**2.16 — Final ceiling brickwall.** `finalCeiling_.process()` — a residual **true-peak (or sample-peak) limiter** that catches inter-sample peaks created by downsampling, so the output true-peak equals the ceiling. (Details: §4.4.)

**2.17 — Loudness tracking, auto gain-match, bypass cross-fade.**
  - `loudnessTrack_` measures the processed output LUFS; `updateCompensationGainDb` derives a smoothed (~1 s) gain so output LUFS matches the learned reference (±12 dB clamp). Same applied to the dry path.
  - **Bypass cross-fade:** `bypassFade_` blends live↔dry sample-by-sample (click-free bypass), using the delay-aligned dry copy from 2.2.

**2.18 — I/O Output gain + output metering.** `applyIoOutputGain()`, then output peak/RMS/true-peak are measured and published.

---

## 3. The Ceiling model (Ozone-style decouple) — important
- The **limiter threshold is fixed at 1.0** (sample-peak) / **0.965** (true-peak). It does **not** move with the Ceiling knob.
- **Ceiling = an output gain** applied *after* limiting (`ceilingLin`, step 2.14), with the FinalCeiling brickwall guaranteeing it.
- Consequence: **gain reduction responds only to Input Gain, not to Ceiling.** Lowering the ceiling lowers output level without changing how hard the limiter works. `ceiling_db` default **0.0**.
- `MDSP_BAND_HEADROOM_DB = 0` (the old per-band pre-shave is removed).

---

## 4. Component deep-dives (plain-language)

### 4.1 Crossover (`detectCrossover_` / `applyCrossover_`)
`juce::dsp::LinkwitzRileyFilter`, 120 Hz, complementary low/high that sum back to flat. **Two instances on purpose:** one drives *detection* (measuring band peaks), the other drives *application* (splitting the delayed audio to apply gain) — separating them avoids state bleed between the measure and apply paths. `snapToZero()` flushes filter state at block boundaries.

### 4.2 LookaheadDelay (`lookahead_`, `lookaheadWide_`, `lookaheadPad_`, plus `dryDelay_`)
Simple per-channel circular buffers. `pushPop()` writes the new sample and returns the one from `delaySamples` ago — RT-safe, no allocation. `lookahead_` and `lookaheadWide_` make each gain curve line up with its active envelope window; `lookaheadPad_` absorbs the unused max-window slack so reported latency never changes while sweeping DEV lookahead.

### 4.3 LimiterEnvelope (the heart — gain computer)
Input = peak stream, output = gain coefficient stream (≤1). No audio delay here; the caller does the delaying.
- **Attack** = a **half-cosine ramp** (`attackTable_`, `0.5·(1−cos(π·k/n))`) applied across the lookahead window so gain eases down *into* each peak. The temporary **DEV Attack** knob now overrides Character-derived attack for all envelopes while tuning:
  - **DEV Attack** = `dev_attack_ms`, clamped per envelope to `1..active lookahead samples` so it can never exceed the LA Band/LA Wide window.
  - With the DEV override removed/disabled, attack length returns to **Character**:
  - **Clean** ≈ 3 ms · **Tight** ≈ 1 ms · **Aggressive** ≈ 0.3 ms.
- **Release — two selectable engines** (chosen by the DEV Release Engine toggle, §6):
  - **AdaptiveSigma (legacy):** two cascaded one-pole smoothers whose time-constant is blended between a *fast* and *slow* value by **`sigma`** (an estimate of how *sustained* the limiting is). Asymmetric: sigma rises fast (attack), decays slow. Per-mode timing table:

    | Auto Release mode | fast ms | slow ms | sigma ms |
    |---|---|---|---|
    | **Transparent** | 80 | 1200 | 250 |
    | **Balanced** | 50 | 600 | 200 |
    | **Reactive** | 20 | 250 | 150 |

    The audible downside: the *rate-switching itself* can be heard. This is what we're moving away from.
  - **LookaheadFollower (new, current best):** removes rate-switching. Recovery is **gated by the minimum gain visible within the lookahead window** (a sliding-window minimum) so the gain only recovers in genuine gaps — *program-dependence comes from the signal, not a guessed time constant*. The recovery slope is a **fixed-time N-pole cascade** (2–4 poles), giving the same smooth S-curve every release. Time = `dev_la_release_ms` (normalized per pole so pole-count changes smoothness, not speed).

### 4.4 FinalCeilingLimiter
A small, fixed brickwall after downsampling. 64-sample lookahead + its own 4× true-peak detector. In **TruePeak** mode it oversamples internally to catch inter-sample peaks; in **SamplePeak** mode it just clamps samples. Hardwired Aggressive attack, 100 ms release. Its job is only to mop up residual overshoot so output true-peak = ceiling.

### 4.5 TruePeakDetector (metering)
4× oversampled absolute-max estimate of inter-sample peak. Four instances (IN L/R, OUT L/R) feed the true-peak readouts; overs above 0 dBFS render red with a "+".

### 4.6 Loudness (`loudnessRef_`, `loudnessTrack_`, `LoudnessAnalyzer`)
ITU/EBU-style LUFS: K-weighting (high-shelf @1681.97 Hz +4 dB, then high-pass @38.13 Hz), `LUFS = −0.691 + 10·log10(meanSquare)`. Windows: Momentary 0.4 s, **Short-term 3 s** (used for gain-match), Integrated from reset. **Auto gain-match:** captures a learned reference LUFS, then nudges a smoothed (~1 s) compensation gain so processed LUFS tracks the reference (±12 dB) — lets you A/B at matched loudness.

---

## 5. User control surface (the shipping knobs)

| Control | ID | Range / values | Default | What it does |
|---|---|---|---|---|
| Input Gain | `input_gain_db` | 0…24 dB | 0 | Drives the limiter (more gain = more GR). |
| Ceiling | `ceiling_db` | −24…0 dB | 0 | Output gain after limiting (decoupled from GR). |
| Gain/Ceiling Link | `gain_ceiling_link` | bool | off | Move gain & ceiling together. |
| Auto/Track gain-match | `gain_match_auto` | bool | off | Loudness-match output to reference. |
| Release | `release_ms` | 1…1000 ms | 30 | Manual release (only when Auto off). |
| Release Sustain Ratio | `release_sustain_ratio` | 1…10 | 4 | Slow/fast split in Clean mode; exposed in the DEV window for manual-release audition. |
| Release Auto | `release_auto` | Off/Auto | Off | Program-dependent release. |
| Auto Release | `auto_release_mode` | Transparent/Balanced/Reactive | Transparent | Picks the timing table (§4.3). |
| Ceiling Mode | `ceiling_mode` | SamplePeak/TruePeak | SamplePeak | Final brickwall mode. |
| Stereo Mode | `stereo_mode` | Stereo/M-S | Stereo | Wideband detection domain. |
| Stereo Link | `stereo_link_pct` | 0…100% | 100 | L/R gain linking (Stereo). |
| M/S Link | `ms_link_pct` | 0…100% | 100 | M/S gain linking. |
| Color | `band_color` | 0…100% | 0 | 0 = linked/transparent, 100 = independent/multiband. |
| Character | `character` | Clean/Tight/Aggressive | Clean | Temporarily greyed out/inert while DEV Attack overrides attack speed (§4.3). |
| Clipper | `clipper_drive_db` | −12…0 dB | 0 | Pre-limiter clip drive. |
| Clipper Mode | `clipper_mode` | Hard/Soft | Hard | Clip shape. |
| Clipper Active | `clipper_active` | bool | on | Clipper power. |
| Limiter Active | `limiter_active` | bool | on | Whole limiter block power. |
| Bypass | `plugin_bypass` | bool | off | Click-free dry/wet bypass. |
| I/O In/Out L/R + links | `io_*` | ±24 dB | 0 | Calibration trims. |

Factory presets in the header menu remain curated voicing subsets. User presets
saved from the header **Save** button or preset menu are full APVTS state
snapshots written to `~/Library/Audio/Presets/MelechDSP/MasterLimiter/*.mlpreset`;
they round-trip every registered parameter, including the temporary DEV controls
listed in §6, for A/B comparison between complete voicings. The preset menu also
has **Load from file...**, which opens that presets folder and can load any
`.mlpreset` selected from disk.

The header **A/B** controls are scratch compare slots backed by full APVTS
snapshots. Switching to the inactive slot first captures the live state into the
current slot, then loads the target slot with `replaceState()`. The copy button
seeds the inactive slot from the active slot, so a common flow is **A→B**, tweak
B, then flip A/B to compare complete voicings. The A/B pair and active slot are
stored in the plugin state alongside the APVTS wrapper so host sessions restore
the current comparison pair.

---

## 6. DEV controls (TEMPORARY — remove before 0.4)
Live, RT-safe tuning knobs now live in an **embedded editor panel** opened by the header **DEV** button (not a separate OS window — required so AAX/Pro Tools commits parameter gestures). Controls are grouped by operating section. They drive release/lookahead/attack voicing experiments; defaults reproduce the baked constants where applicable. The main view intentionally has no inline DEV strip. User presets capture these DEV values as part of the full APVTS state. The History Graph remains a separate read-only window (no param writes).

| Window section | Controls |
|---|---|
| ATTACK | `dev_attack_ms` |
| LOOKAHEAD | `dev_lookahead_band_ms`, `dev_lookahead_wide_ms` |
| RELEASE · Engine | `dev_release_engine` |
| RELEASE · Lookahead engine | `dev_la_release_ms`, `dev_la_release_poles` |
| RELEASE · Adaptive engine | `dev_sigma_attack_ms`, `dev_sigma_decay_scale` |
| RELEASE · Band scaling | `dev_low_band_release_scale`, `dev_high_band_release_scale` |
| RELEASE · Manual | `release_sustain_ratio` |

| DEV control | ID | Range | Default | Drives | Notes |
|---|---|---|---|---|---|
| **Release Engine** | `dev_release_engine` | Adaptive / Lookahead | **Adaptive** | Chooses §4.3 engine | A/B switch. **Lookahead is the current winner.** |
| **LA Release** | `dev_la_release_ms` | 5…400 ms (skewed low) | 80 | LookaheadFollower recovery time | The knob to sweep. Expected sweet spot well under 80 ms. |
| **LA Poles** | `dev_la_release_poles` | 2 / 3 / 4 | 3 | Cascade order = recovery smoothness | Normalized so it changes smoothness, not speed. |
| **Low Release Scale** | `dev_low_band_release_scale` | 0.5…8.0× | 3.0 | Low-band release multiplier | Low band releases slower (default 3×). Applies to *both* engines. |
| **High/Wide Release Scale** | `dev_high_band_release_scale` | 0.5…8.0× | 1.0 | High/wideband release multiplier | Nominal 1×. |
| **Sigma Attack** | `dev_sigma_attack_ms` | 1…50 ms | 5 | AdaptiveSigma: how fast `sigma` rises | Legacy-engine only. |
| **Sigma Decay Scale** | `dev_sigma_decay_scale` | 0.5…8.0× | 1.0 | AdaptiveSigma: how slow `sigma` decays | Legacy-engine only. |
| **LA Band** | `dev_lookahead_band_ms` | 0…6 ms, 0.01 ms step | 5 | Per-band audio delay + low/high envelope window | 0.00 maps to one OS sample; latency stays fixed via wet-path padding. |
| **LA Wide** | `dev_lookahead_wide_ms` | 0…6 ms, 0.01 ms step | 5 | Wideband audio delay + wide envelope window | 0.00 maps to one OS sample; latency stays fixed via wet-path padding. |
| **Attack** | `dev_attack_ms` | 0.05…10 ms | 3 | All limiter envelope attack ramps | Overrides Character; capped by each active lookahead window. |
| **Sustain Ratio** | `release_sustain_ratio` | 1…10 | 4 | Manual-release sustain split | Active only when Release Auto is Off. |

**Plan:** once Attack, LA Band/Wide, LA Release ms, and Poles are chosen by ear, Claude bakes them as constants or promotes any keeper to a real user parameter, then deletes the temporary DEV params for 0.4.

---

## 7. Metering taps (what's measured where — read this before the history graph)
Most metering is **instantaneous scalar atomics** written by the audio thread and read by the UI at **30 Hz** (`PluginEditor` timer → `MainView::syncMetersFromProcessor`). The history graph additionally drains a lock-free SPSC ring of ~0.5 ms frames (`grDb`, `outDb`, `inDb`, `clipDb`) captured from per-sample GR/clip envelopes and sample values.

| Quantity | Atomic(s) | Tapped at step |
|---|---|---|
| Input peak / RMS / true-peak L/R | `inputPeakLDb_`, `inputRmsLDb_`, `inputTruePeakLDb_`, … | 2.3 |
| Gain reduction (wide / L / R / max) | `currentGrDb_`, `currentGrLDb_`, `currentGrRDb_`, `maxGrSinceResetDb_` | 2.14 |
| Clip reduction | `currentClipDb_`, `maxClipSinceResetDb_` | 2.7 |
| Output peak / RMS / true-peak L/R | `outputPeakLDb_`, `outputRmsLDb_`, `outputTruePeakLDb_`, … | 2.18 |
| Loudness / comp gain | `LoudnessAnalyzer` snapshots, `compGainDb` | 2.4 / 2.17 |

---

## 8. Known discrepancies & backlog (so the doc stays honest)
1. **Multiband true-peak leak:** at Color 100 + TruePeak, output TP can leak to ~−0.4 (over ceiling). Root cause = 4× OS headroom. Grouped with harmonic aliasing in the "OS-quality" slice (raise OS to 8×+ and/or proper ISP control).
2. **Color intermediate (0<x<100):** low end dips from phase cancellation between the full-band and allpass-split paths; endpoints (0/100) are clean. Fix = linear-phase complementary crossover (adds latency).
3. **DEV params** are shipping in the beta build — must be removed for 0.4.
4. **Metering history depth** — history graph storage is fixed at 65536 frames for the 30 s max view; extend only if a future UI window needs more.
