# MasterLimiter — Custom Filter Library Design (`mdsp_dsp/filters/`)

**Status:** 🏗️ DESIGN — supersedes the JUCE DSP filter dependencies. Implements the keystone of `FILTER_QUALITY_ROADMAP.md` (item C, the linear-phase FIR) and its fallout (D/E/F).
**Maintained by:** Claude (architect). **Built by:** Cursor, slice-gated. **Auditioned by:** avishali in Ableton + the offline rig.
**Measure everything** with `tools/analysis/sweep_render.py` / `sweep_isolate.py` and the planned Hammerstein analyzer (roadmap B) — before/after THD + phase numbers, not eyeballed spectrograms.

---

## 0. Why we're doing this (one paragraph)

We currently lean on three JUCE DSP filter families. Only **one** is a quality problem. `juce::dsp::LinkwitzRileyFilter` (the 120 Hz crossover) is **IIR / minimum-phase**, and its phase shift is the *direct cause* of the Color-knob low-end cancellation (roadmap D, backlog #2): at intermediate Color the phase-shifted split fights the full-band path and the low end dips. `juce::dsp::Oversampling` is already linear-phase equiripple FIR and sounds clean (−119 dB passthrough measured) — owning it buys us **control** (passband edge, attenuation, 4×→8×), not a fix. The K-weighting `juce::dsp::IIR::Filter` in `LoudnessAnalyzer` is **ITU-R BS.1770-mandated** — we replace the JUCE *class* but must keep the *coefficients spec-exact*; there is no "high-end" version of a standards filter. So this library exists primarily to **build a linear-phase complementary crossover**, with the oversampler and K-weight as secondary "own it" ports.

---

## 1. Inventory being replaced

| JUCE class | Site | Disposition |
|---|---|---|
| `dsp::LinkwitzRileyFilter` ×2 @120 Hz | `detectCrossover_` / `applyCrossover_` (PluginProcessor.h:165-166) | **Replace** → `LinearPhaseCrossover`. The keystone. |
| `dsp::Oversampling` ×1 (4× limiter) | `limiterOversampler_` (PluginProcessor.cpp:150-162) | Replace → `HalfbandPolyphaseOS` (later slice). |
| `dsp::Oversampling` ×4 (TruePeakDetector) | TruePeakDetector.cpp | Replace → shared OS engine. |
| `dsp::Oversampling` ×1 (FinalCeilingLimiter TruePeak) | FinalCeilingLimiter.cpp | Replace → shared OS engine. |
| `dsp::IIR::Filter` ×2/ch K-weighting | LoudnessAnalyzer.cpp:46-51 | **Port only** → `Biquad`, coefficients frozen to current values. No behavior change. |
| `dsp::IIR::Coefficients::makePeakFilter` | NotchBiquad.h (feedback SDK) | Out of scope (not in MasterLimiter path). |

Not touched (infrastructure, not signal-shaping): `ProcessSpec`, `AudioBlock`, `DelayLine`, `FFT`/`WindowingFunction` (analysis only).

All DSP lives in the **SDK submodule** `melechdsp-hq/shared/mdsp_dsp/`, not the plugin repo. New code goes there under `include/mdsp_dsp/filters/` + `src/filters/`. Remember the **two HQ copies** gotcha — the submodule is source of truth; `build*/_deps/mdsp_sdk/` are generated.

---

## 2. Module layout

```
mdsp_dsp/filters/
  Biquad.h                 TDF-II-T, double state. Foundation. Replaces IIR::Filter.
  BiquadCascade.h          N sections in series (higher-order IIR).
  FilterDesign.h           RBJ cookbook designers (LP/HP/shelf/peak) + frozen BS.1770.
  LinearPhaseFIR.h/.cpp    Symmetric Type-I kernel, partitioned FFT convolution. THE ENGINE.
  FirDesign.h/.cpp         Kaiser windowed-sinc (v1) + equiripple/Remez (v2) designers.
  HalfbandPolyphaseOS.h    Half-band polyphase oversampler. Replaces dsp::Oversampling.
  LinearPhaseCrossover.h   Complementary N-band split. Replaces LinkwitzRileyFilter. KEYSTONE.
```

Dependency layering (build bottom-up): `Biquad` → `LinearPhaseFIR` → {`HalfbandPolyphaseOS`, `LinearPhaseCrossover`}.

---

## 3. Component designs

### 3.1 `Biquad` — Transposed Direct Form II, double state
The correct high-end topology for floating point: TDF-II-T has the lowest coefficient-quantization noise and behaves well at low frequencies (critical for the 38 Hz K-weight HPF and any 120 Hz IIR work). Audio stays `float`; **state variables and coefficients are `double`**.

```
y = b0*x + s1
s1 = b1*x - a1*y + s2
s2 = b2*x - a2*y
```

Designers (`FilterDesign.h`) replicate the **RBJ Audio EQ Cookbook** formulas — the same family JUCE's `makeHighShelf`/`makeHighPass` use — so the K-weight port is numerically faithful, not just close.

### 3.2 K-weighting port (frozen)
Replace the two `juce::dsp::IIR::Filter<float>` per channel with two `Biquad`. Coefficients **frozen** to the current call sites:
- **Pre / high-shelf:** `makeHighShelf(fs, 1681.974, Q=1/√2, gain=+4 dB)`
- **RLB / high-pass:** `makeHighPass(fs, 38.135, Q=0.5003)`

Acceptance: integrated/short-term LUFS on a reference render must match the JUCE build **to ≤0.01 LU**. This is a regression port, not an improvement — do not retune.

### 3.3 `LinearPhaseFIR` — the engine
- **Symmetric (Type-I, odd length) kernel** → exact linear phase; exploit symmetry (fold-and-add) to halve MACs.
- **Group delay** `D = (N−1)/2` samples, integer by construction.
- **v1 design:** Kaiser windowed-sinc (β from stopband attenuation spec). **v2 design:** equiripple (Remez/Parks-McClellan) for sharper transition at equal length.
- **Runtime:** partitioned (uniform-block) FFT convolution for the long crossover kernels; direct fold-and-add for the short OS half-band kernels. RT-safe, no allocation in `process`.

### 3.4 `LinearPhaseCrossover` — keystone, fixes Color
The phase-coherence trick:

```
lowOut  = FIR_lowpass(x)              // group delay D
highOut = delay(x, D) − lowOut        // complement BY SUBTRACTION
```

Because the high band is *derived* from the delayed input minus the low band, **`low + high ≡ delay(x, D)`** — a pure delay, i.e. **perfectly flat magnitude *and* phase at every Color value**, by construction. The intermediate-Color low-end dip cannot occur because both the split path and the full-band path now share identical phase.

- **Movable cutoff:** just a redesign parameter → roadmap E "movable crossover frequency" falls out for free.
- **3-band:** nest a second split on `lowOut` (or `highOut`); each band keeps its own limiter envelope (roadmap E, stepping stone to the spectral limiter).
- Replaces **both** `detectCrossover_` and `applyCrossover_`. The detect/apply split (state-bleed avoidance) is an IIR concern; with FIR you can share one design but keep two instances for clean state, matching today's structure.

### 3.5 `HalfbandPolyphaseOS` — owns oversampling (later slice)
Half-band FIR (every other tap zero → free 2×), 2 stages cascaded for 4×, polyphase-decomposed. Baseline matches today's JUCE config (stage 1: 0.03 transition / −110 dB; stage 2: 0.10 / −100 dB). Then the roadmap-F wins: **move the anti-alias cut up** (flat top octave) and optionally **4×→8×** for the nonlinear stages. One shared instance type feeds the limiter, the 4 TruePeakDetectors, and FinalCeiling.

---

## 4. The hard tradeoff: crossover latency

A **linear-phase** split at **120 Hz** needs a long kernel — the transition band is a tiny fraction of fs. A clean ~80→160 Hz transition at 48 kHz is ≈1.5k–2k taps ≈ **20–40 ms** added latency.

**Decision (avishali, 2026-06-21): ship simple, optimize later.**
- **v1 ships** the linear-phase FIR at host rate and **accepts the latency**. We already run ~6 ms lookahead + OS latency; host PDC absorbs it; mastering isn't tracking. Goal: prove the Color fix end-to-end on the offline rig first.
- **Follow-up slice:** multirate low-band split (decimate to ~6 kHz, short linear-phase lowpass there, interpolate back) → ~4–6 ms instead of 30. Same phase-linear sum, fraction of the latency.
- Keep the `LinearPhaseCrossover` interface latency-agnostic (`getLatencySamples()`) so the multirate swap is internal.

Latency reporting must flow into the plugin's PDC; the wet/dry delay-align (`dryDelay_`) and lookahead padding already key off total latency — extend, don't special-case.

---

## 5. Build order (slice-gated, maps to roadmap B→C→D/E/F)

| Slice | Scope | Gate | Status |
|---|---|---|---|
| **B** | Hammerstein/Farina analyzer (`tools/analysis/hammerstein.py`): H1–H5/THD + linear mag/excess-phase, latency-removed. | Reports per-order harmonics + phase on rendered files. | ✅ **already existed** (predates this arc) |
| **F-0** | `Biquad` (TDF-II, double) + `FilterDesign` (JUCE-matched bilinear, *not* generic RBJ) + frozen BS.1770 K-weight port. Pure regression. | LUFS matches JUCE ≤0.01 LU. | ✅ **done** `bbd6d91` — diff 0.0007/0.00001/0.0006 LU; plugin rebuild clean |
| **F-1** | `LinearPhaseFIR` engine + `FirDesign` (Kaiser windowed-sinc). C++ only (B already covered the analyzer). | Symmetric kernel (linear phase); Kaiser magnitude spec; engine IR == taps; passband = pure M-sample delay. | ✅ **done** `8c4bd7e` — 5/5 checks; full suite 382 groups exit 0 |
| **F-2a** | `LinearPhaseCrossover` engine (subtraction trick) + unit test. SDK only. | Exact complementary sum; flat reconstruction at every Color blend. | ✅ **done** `735c6bc` — 5/5; + fold `e6e29c0` (symmetry halves OS MACs) |
| **F-2b** | Wire into `detectCrossover_`/`applyCrossover_`; DEV-tunable spec; phase-coherent reconstruction (`xDelayed`); detection alignment; fixed-latency Nmax zero-pad; PDC. | Offline Color sweep flat; band-LA restored; **CPU profiled**. | ⚠️ **correct but RT-UNVIABLE** `31ca234`/`882225f` — Color dip gone (±0.0000 dB), band-LA fixed, but **100% CPU in DAW**: ~14,264-tap kernel at 192k = ~5.5 GMAC/s direct conv. Offline rig couldn't catch (no RT deadline). Needs F-2c. |
| **F-2c** | Uniform-partitioned FFT convolution (`PartitionedConvolver`, `juce::dsp::FFT`) + OS-latency alignment. | Matches direct conv <1e-5; complementary sum exact; CPU RT-viable; HF phase tilt removed. | ✅ **done** SDK `b159ea5` / plugin `71d509e` — **CPU 100%→22%** @64 buf; **HF phase 178°→65°@20k** (residual 65° = oversampler, F-3); Color still ±0.0000 dB. Latency aligned to osFactor for exact PDC. |
| **F-3a** | `HalfbandPolyphaseOS` engine (linear-phase polyphase half-band, 2-stage 4×) + tests. SDK only. | Round-trip = pure delay (flat mag+phase); ≥−100 dB anti-alias; latency exact integer host samples. | ✅ **done** `51bf602` — round-trip ~6e-6; latency 136 exact. ⚠️ corrected in review: Cursor's first pass used a lossy `Linear` fractional delay (HF rolloff: 5k 0.053, 10k 0.21) + relaxed tests — replaced with a **pure integer 4×-rate delay** (half-band lengths are always N≡3 mod 4, so the 0.5-host residual is inherent). |
| **F-3b** | Swap the main limiter `limiterOversampler_` → `HalfbandPolyphaseOS`. Measured. | HF phase ~65°→≈0 @20k; THD/aliasing ≥ baseline; PDC correct. | ✅ **done** (plugin) — **HF phase 65°→0° @20k** (flat 5–20k); Color still ±0.0000 dB; THD −93.8 dB (same half-band taps as JUCE). CPU = user DAW check. |
| **F-3c/d** | Swap TruePeakDetector(×4) + FinalCeiling oversamplers; then move anti-alias cut up / optional 8× (roadmap F). | No regression; flat top octave. | ◻ Follow-ups (no output-phase impact). |
| **later** | Movable crossover freq (user) / 3-band / multirate XO-latency. | per-slice. | ◻ |

Each slice closes on the four-part gate and the "measure everything" rule. F-0 first because it's a no-risk port that stands up the module and validates the toolchain before the keystone.

---

## 6. Cross-references
- `FILTER_QUALITY_ROADMAP.md` — the C/D/E/F arc this implements.
- `SIGNAL_FLOW.md` §1 (oversampling), §4.1 (crossover), §4.6 (loudness) — the sites being replaced.
- `LIMITER_TYPES.md` — the 3-band split here is the foundation for the spectral limiter (#3).
