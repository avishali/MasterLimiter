# MasterLimiter — v1 Spec (Clean Limiter Core)

**Status:** Draft, 2026-05-11
**Authoritative architecture:** [ADR-0004](../third_party/melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md)
**Slice plan:** [PROMPTS/PLAN.md](../PROMPTS/PLAN.md)

## 1. Product summary

A mastering-grade transparent limiter. v1 ships the clean-limiter core only.
Clipper, compressor, and character/color modes are planned successors and are
out of scope for v1.

**Reference targets** (transparency benchmarks): FabFilter Pro-L 2,
Waves L2 / L3, iZotope Ozone Maximizer (IRC modes).

## 2. Hard requirements

| # | Requirement |
|---|-------------|
| R1 | ≥ 5 dB sustained gain reduction with no audible artifacts on dense program material. |
| R2 | True-peak compliance per ITU-R BS.1770 when TP mode is selected. |
| R3 | Sample-peak mode available as a selectable alternative. |
| R4 | Stereo and M/S link options (link % continuous, 0–100%). |
| R5 | Plugin reports correct latency to host (PDC). |
| R6 | All audio-thread code is RT-safe per `docs/AUDIO_DSP_STANDARDS.md`. |
| R7 | Engine/UI boundary preserved: UI consumes snapshots only. |
| R8 | Builds clean (no new warnings) on macOS for VST3 + AU. Windows/Linux post-v1. |
| R9 | All new reusable DSP lives in `shared/mdsp_dsp/`. All new reusable UI lives in `shared/mdsp_ui/`. |

## 3. Parameter set (v1)

| Param ID            | Range / Default              | Notes |
|---------------------|------------------------------|-------|
| `input_gain_db`     | −12 .. +24 dB, default 0     | Pre-detector. |
| `ceiling_db`        | −12 .. 0 dB, default −1.0    | Output ceiling. |
| `release_ms`        | 1 .. 1000 ms, or "Auto"      | "Auto" = program-dependent (Slice 9). |
| `lookahead_ms`      | Fixed 5 ms in v1             | Exposed read-only; user-selectable post-v1. |
| `ceiling_mode`      | { Sample-Peak, True-Peak }   | Default True-Peak. |
| `stereo_link_pct`   | 0 .. 100 %, default 100      | 100 = fully linked. |
| `ms_link_pct`       | 0 .. 100 %, default 100      | 100 = full M/S link; 0 = independent M and S detection. |
| `character`         | { Clean }                    | Placeholder for future modes. |

Parameter changes are smoothed on the audio thread via `mdsp_dsp::Smoother`.

## 4. Snapshots (Engine → UI)

| Snapshot field        | Type   | Source |
|-----------------------|--------|--------|
| `gain_reduction_db`   | float  | Post-Stage-C envelope, instantaneous. |
| `gr_peak_hold_db`     | float  | UI-side hold logic. |
| `output_tp_dbtp`      | float  | Post-oversampler true-peak. |
| `output_sp_db`        | float  | Sample-peak. |
| `input_sp_db`         | float  | Pre-input-gain sample-peak. |

All published via lock-free single-writer/single-reader pattern (see
`mdsp_dsp` snapshot conventions).

## 5. Transparency validation — pass criteria

Bench harness (Slice 2) renders each test signal through the limiter at
target GR depths and compares against the dry signal (gain-matched).

**Bar:** stricter than FabFilter Pro-L 2 / Waves L2-L3 / Ozone Maximizer (IRC).
Where public measurement data is unavailable, we will A/B against those plugins
on the same corpus in Slice 5 prep and require our null residual to beat the
best reference by **≥ 3 dB** on every signal, on top of the absolute thresholds
below.

### Test corpus (v1)

- `pink_noise_60s.wav`
- `sine_sweep_20_20k.wav`
- `dual_tone_imd_smpte.wav` (60 Hz + 7 kHz, 4:1)
- `drum_loop_dense.wav` (user-supplied)
- `full_mix_dense.wav` (user-supplied, modern loud master)
- `full_mix_dynamic.wav` (user-supplied, dynamic / acoustic)
- `vocal_solo.wav` (user-supplied — transparency on isolated voice is the
  hardest test; artifacts surface immediately)

### Absolute pass criteria

Each row is a hard threshold. Failure of any row at the target GR depth blocks
the gate of the slice that introduced it.

| Metric                                            | 3 dB GR     | 5 dB GR (PRIMARY) | 7 dB GR (stretch) |
|---------------------------------------------------|-------------|-------------------|-------------------|
| Null residual, K-weighted, dense mix              | ≤ −60 dBFS  | ≤ −55 dBFS        | ≤ −48 dBFS        |
| Null residual, K-weighted, drum loop              | ≤ −52 dBFS  | ≤ −45 dBFS        | ≤ −38 dBFS        |
| Null residual, K-weighted, vocal solo             | ≤ −58 dBFS  | ≤ −50 dBFS        | ≤ −42 dBFS        |
| THD+N delta vs dry (pink noise, A-weighted)       | ≤ +0.10 %   | ≤ +0.20 %         | ≤ +0.50 %         |
| IMD (SMPTE 60 Hz/7 kHz 4:1) delta vs dry          | ≤ +0.08 %   | ≤ +0.15 %         | ≤ +0.35 %         |
| Transient crest-factor delta (drum loop)          | ≥ −0.6 dB   | ≥ −1.0 dB         | ≥ −1.8 dB         |
| Inter-modulation pumping, 50 Hz LF tone + speech  | ≤ −3 LU short-term variance increase | ≤ −2 LU | ≤ −1 LU |
| Aliasing residual above 20 kHz at 96 kHz render   | ≤ −90 dBFS  | ≤ −90 dBFS        | ≤ −85 dBFS        |
| True-peak overs (TP mode, ceiling −1 dBTP)        | 0 over 60 s | 0 over 60 s       | 0 over 60 s       |
| Reported latency vs measured latency              | ±1 sample   | ±1 sample         | ±1 sample         |

### Relative pass criteria (vs reference plugins)

On the **same corpus, same GR depth, same ceiling**, MasterLimiter's K-weighted
null residual must be ≥ 3 dB lower than the lowest of:

- FabFilter Pro-L 2 (Modern / Transparent style)
- Waves L3-LL Multimaximizer (Punchy preset)
- iZotope Ozone Maximizer (IRC IV)

The 3 dB margin is the headline differentiator. If we tie or lose on any
signal, the slice fails its gate.

Bench artifacts (WAV nulls, CSV measurements, A/B plots) are committed under
`docs/products/MasterLimiter/bench/SLICE_NN/` so results are reviewable.

## 6. Out of scope for v1

- Clipper, compressor, multiband modes
- Character / saturation flavors beyond the sub-audible safety net
- Adaptive / auto-release intelligence beyond a documented Slice-9 algorithm
- Windows / Linux builds
- Loudness target (LUFS) controls
- Dithering (separate concern, may live elsewhere)

## 7. Repository layout

- **Product repo (separate):** `MasterLimiter` at
  `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/`, sibling to
  `AnalyzerPro`. Plugin shell, APVTS, product UI assembly, presets only.
  Consumes HQ via git submodule at `shared/melechdsp-hq/`.
- **HQ (this repo):** all reusable DSP and UI lives here.
- **Marketing name:** TBD. `MasterLimiter` is a placeholder; rename touches
  only display strings and bundle IDs, never parameter IDs.

## 8. Open questions (resolved as ADRs)

- ADR-0005 (pending): Transient/sustain split method — envelope-difference vs
  2-band crossover. Decided in Slice 4 prep using bench A/B.
- ADR-0006 (pending): Auto-release algorithm. Decided in Slice 9 prep.
