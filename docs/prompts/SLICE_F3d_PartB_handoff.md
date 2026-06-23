# Cursor handoff — F-3d Part B: wrap the clipper at 8× (plugin)

**Repo:** `MasterLimiter` (the plugin). **Prereq:** Part A is green and **merged into the
SDK** — i.e. `HalfbandPolyphaseOS` has the N-stage `prepare(... const std::vector<StageSpec>&)`
form, `getOversamplingFactor()` returns `1<<N`, and 1×/2×/3× round-trip rel-err < 1e-5.
Do NOT start this until Part A is verified. Full slice context:
`docs/prompts/SLICE_F3d_oversampler_8x.md`; Part A: `docs/prompts/SLICE_F3d_PartA_handoff.md`.

**Scope:** plugin only (`Source/PluginProcessor.{h,cpp}` + offline-rig verification).
No SDK changes.

---

## Goal

The clipper currently runs at the **4× limiter OS rate**, which is marginal — its
hard-clip harmonics fold back as **−34 dB aliased products** (measured, 11k+13k two-tone,
hard, ~1.9 dB into clip). Push the *clipper only* to **8×** so the fold-point moves up an
octave and that aliasing drops ≥10 dB. The **−28 dB real cubic IMD is inherent to
clipping and will NOT change** — that's expected, don't chase it.

**Targeted, not blanket.** Oversample only the clipper (a memoryless nonlinearity) with a
**1-stage (2×)** `HalfbandPolyphaseOS` wrapped around the existing 4× clip loop
(4×→8×→clip→4×). Keep the crossover + limiter at 4×. Do NOT raise the whole limiter to
8× — the linear partitioned-FFT crossover gains nothing from more OS and would ~double in
cost.

---

## Where the clipper is

`Source/PluginProcessor.cpp`, inside `if (limiterActive_->get())`:

- The 4× OS block is produced at **line 967**: `auto osBlock = limiterOversampler_.processSamplesUp(block);` with `osN = osBlock.getNumSamples()` (= host `n` × 4).
- The **clipper loop is lines 977–1018** (`for (int i = 0; i < osN; ++i)`): per-sample
  drive → clip (hard `clipperModeIdx==0`, else soft tanh knee `kSoftKnee=0.891f`) →
  un-drive (`y /= clipperDriveGain`), writing back into `osBlock` in place. Clip metering
  fills `clipEnvBuf_` via `hostIdx = i * n / osN` (line 980/1010).
- After the clipper, the input-gain loop (line 1031+) and the rest of the limiter run on
  the same 4× `osBlock`, then `limiterOversampler_.processSamplesDown(block)` at line 1344.

## What to build

Wrap **only the clip math** (the per-sample drive/clip/un-drive) in a 2× OS:

```cpp
// once, in processCore before the clipper loop, only when clipperActive:
auto clipBlock = clipperOversampler_.processSamplesUp(osBlock);   // 4× -> 8×
const int clipN = (int) clipBlock.getNumSamples();                // = 2 * osN
for (int i = 0; i < clipN; ++i) {
    // identical drive -> clip(hard/soft) -> un-drive math, on clipBlock.getChannelPointer(ch)[i]
    // metering: hostIdx = jmin(n - 1, i * n / clipN);   // (= i*n/(2*osN))
}
clipperOversampler_.processSamplesDown(osBlock);                  // 8× -> 4×
```

### New member (`PluginProcessor.h`)
```cpp
mdsp_dsp::HalfbandPolyphaseOS clipperOversampler_;   // 1-stage (2×), runs inside the 4× domain
int clipperOsLatencySamples4x_ = 0;                  // its latency in 4×-rate samples
```

### prepareToPlay (near line 165, after `limiterOversampler_.prepare`)
```cpp
// Clipper-only 2× OS, prepared at the OS (4×) rate. Gentle spec — it only needs to
// suppress images in the top octave of the 4× band.
clipperOversampler_.prepare (2, samplesPerBlock * 4, osSampleRate_, {{0.10, 90.0}});  // vector form, 1 stage
clipperOversampler_.reset();
clipperOsLatencySamples4x_ = clipperOversampler_.getLatencyInSamples();  // in 4×-rate samples
```
- `osSampleRate_` is the 4× rate (member, line 260). Max block at the 4× rate is
  `samplesPerBlock * 4` (= max `osN`); the 2× up path produces `*2` that internally,
  which `HalfbandPolyphaseOS::prepare` sizes for.

## ⚠️ Latency alignment — the F-2c / F-3a lesson (most important part)

The clipper OS adds latency **inside the 4× domain**. `getLatencyInSamples()` returns it
in **4×-rate samples**. For the host PDC to stay an **integer**, that latency MUST be a
**multiple of 4** (the limiter osFactor). A 1-stage `HalfbandPolyphaseOS` only pads to a
multiple of **2**, so you may be 2 short of a multiple of 4 → a sub-sample residual at the
host → **HF phase tilt returns**.

- Compute `clipperOsLatencySamples4x_` and, if it is not a multiple of 4, add a **small
  integer 4×-rate delay** on the post-clipper `osBlock` to round it up to the next
  multiple of 4. (A `juce::dsp::DelayLine<float, None>` at `osSampleRate_`, only engaged
  when clipper-active — see the SDK's `osCompensationDelay_` for the exact pattern.)
- Add the resulting **host-rate** latency `clipperOsLatencySamples4x_ / 4` to
  `baseLatencySamples_` (line 313). Keep `dryDelay_` (line 359) and the `lookaheadPad_`
  math consistent — they key off `baseLatencySamples_`.
- **Subtlety:** the clipper is *conditional* (`clipperActive_`). Latency must NOT flip
  when the user toggles the clipper — that would force a host PDC re-negotiation mid-
  stream (clicks / re-prepare). **Always reserve the clipper-OS latency in
  `baseLatencySamples_` regardless of clipper state**, and when the clipper is bypassed,
  run the signal through the same compensation delay (or an equivalent fixed integer
  delay) so the path length is constant. Confirm this in your report.

## ⚠️ Drive-smoother cadence

`clipperDriveSmoothed_.getNextValue()` is currently called once per 4×-sample (`osN`
times). If you call it once per 8×-sample (`clipN = 2*osN` times) the drive ramp completes
in **half the wall-clock time** → audibly different automation feel. Keep the ramp timing
identical: advance the smoother **once per 4×-sample** and hold its value across both 8×
sub-samples (e.g. recompute `clipperDriveGain` only when `i` crosses into a new 4× sample,
`i/2`). State this choice in your report.

---

## Part B gate (offline rig — CORRECT param names: `input_gain`, `color`, `ceiling`, `clipper`, `clipper_mode`)

Use the offline rig (`tools/analysis/sweep_render.py` + `sweep_isolate.py`, `.venv` has
pedalboard). Renders THROUGH the VST3 offline, latency-independent.

- [ ] **Clipper two-tone IMD** (11k+13k, hard, ~1.9 dB into clip): the **−34 dB aliased
      products drop ≥ ~10 dB**. The −28 dB real cubic IMD (2f1−f2 @9k, 2f2−f1 @15k) is
      **unchanged** — that's correct, it's inherent to clipping.
- [ ] **No regression:** Color reconstruction still **±0.0000 dB** at Color 0/25/50/75/100;
      HF phase still flat (proves the clipper-OS latency aligned to a multiple of 4);
      true-peak ceiling exact.
- [ ] Latency does **not** change when toggling the clipper on/off (report measured host
      latency in both states — they must match).
- [ ] Full `mdsp_dsp_tests` exit 0; MasterLimiter **Release build clean** (Standalone/AU/VST3).
- [ ] **CPU at clipper-engaged, measured in a DAW** — confirm targeted-8× didn't blow the
      RT budget (should be small; only the clipper runs at 8×). Report the number.

## Report back

- Each gate line with the **measured** number (aliased-product Δ in dB, Color recon dB,
  host latency clipper-on vs clipper-off, CPU%).
- Confirm the drive-smoother cadence choice and the latency-reservation approach.
- Commit message: `F-3d Part B — clipper 8× OS (kill −34 dB clipper IMD aliasing)`.
- Do NOT push until reviewed; SDK + plugin push together at slice close. Then I'll update
  `docs/PROGRESS.md`, `docs/CUSTOM_FILTERS.md` (F-3d row), and the memory.
