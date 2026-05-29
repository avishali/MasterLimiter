# MasterLimiter — Slice Plan

**Owner:** avishali (architect: Claude / coder: Cursor)
**Architecture:** [ADR-0004 (in HQ)](../third_party/melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md)
**Spec:** [SPEC.md](../docs/SPEC.md)
**Progress log:** [PROGRESS.md](../docs/PROGRESS.md)

## Slice gate (applies to every slice)

A slice is **done** only when **all four** are true:

1. Builds clean on macOS, no new warnings.
2. Slice-specific bench criteria pass (see each slice's "Gate").
3. avishali auditions the result and approves.
4. Claude (architect) signs off on diff scope and RT-safety.

If a slice fails its gate, we do not proceed. We either patch within the same
slice or split a follow-up slice.

## Workflow per slice

1. **Interview** — Claude asks scope-specific questions for this slice.
2. **Slice prompt** — Claude writes `PROMPTS/SLICE_NN_*.md`
   following the SDK Cursor-task format (Trinity Protocol preamble + scoped
   file list + non-goals + RT-safety reminders + test plan).
3. **Cursor implements** — only the files listed.
4. **Validation** — bench run + avishali audition.
5. **Gate** — pass or iterate. On pass, PROGRESS.md updated.

## Slice list

| #  | Title                                                  | Touches (HQ)                                | Touches (product repo) | Gate (headline)                                                |
|----|--------------------------------------------------------|---------------------------------------------|------------------------|----------------------------------------------------------------|
| 0  | Architecture + plan (this doc)                         | docs only                                   | —                      | Avishali approves ADR-0004, SPEC, PLAN.                        |
| 1  | Product repo skeleton @ `../MasterLimiter/`            | —                                           | new sibling repo       | Plugin loads in Live, audio passes through, params declared.   |
| 2  | Null-test bench harness                                | new `shared/limiter_bench/`                 | —                      | Bench can null any pass-through with residual ≤ −120 dBFS.     |
| 3  | Lookahead + sample-peak single-band limiter            | `mdsp_dsp/LookaheadDelay`, `LimiterEnvelope`| wire-in                | Clean 3 dB GR on pink noise + drum loop per §5 criteria.       |
| 4  | True-peak detector + 4× oversampler on ceiling         | `mdsp_dsp/TruePeakDetector`, `Oversampler`  | TP mode toggle         | Zero TP overs at ceiling −1 dBTP across corpus.                |
| 5  | Transient/sustain split (ADR-0005 decision)            | `mdsp_dsp/TransientSustainSplit`            | —                      | Clean 5 dB GR on drum loop + dense mix per §5 criteria.        |
| 6  | Soft-knee sub-audible saturator                        | `mdsp_dsp/SoftKneeSaturator`                | —                      | THD+N delta within bounds at 7 dB GR push.                     |
| 7  | ✅ **Shipped — Stereo Link % + 2ch GR meter + envelope SIMD** | ADR-0008; `LimiterEnvelope` SIMD ramp loop | `stereo_link_pct`, per-channel envelopes, 2ch GR meter, latched max readouts | Shipped 2026-05-29. Per-channel parallel envelopes with continuous Link % blend (frozen ID `stereo_link_pct`, default 100% = bit-exact fast-path). 2ch GR meter + latched-max readouts. NEON SIMD inner ramp loop (~4× speedup, resolves heavy-GR underrun). Constant latency 301/301. Bench Slice 3/4/5 PASS 13/13, 14/14, 25/25 unchanged. ADR-0008. M/S deferred. Multiband detection promoted as next architectural lever for "open" gap and CPU↔GR correlation parity with Ozone. |
| 8  | Meters (GR + TP) to mdsp_ui                            | `mdsp_ui/GainReductionMeter`, `TruePeakMeter`| UI assembly           | Meters track snapshots accurately, no UI-thread DSP access.    |
| 9  | ✅ **Shipped — Limiter character + Clipper Drive + on/off** | `LimiterEnvelope::Mode`; `dsp_bench` recal + Ozone reference driver | `limiter_active`, `clipper_drive_db`, expanded `character`, 4× OS limiter chain, TP envelope headroom | Shipped 2026-05-29. 3 Character modes, Clipper Drive stage, limiter on/off, 4× OS limiter chain, ispTrim removed, TP headroom in envelope. Bench Slice 3/4/5 PASS via treatment-B recalibration; Ozone IRC IV reference run documents the in-family character. ADR-0006 + ADR-0007. |
| 10 | Maximizer UI shell (Ozone-inspired two-panel layout)   | —                                           | `ui/MainView`          | Look-lock: two-panel layout, drive Gain at 0 dB hard-left, placeholders for new controls. Audition the look. |
| 11 | ✅ **Shipped — I/O gains + dual Gain-Match (Auto/Track + Learn + Bypass-with-match)** | ADR-0007 + Addendum 2026-05-29 | 8 new params + 1 hidden ValueTree state + DSP + UI | Family shipped 2026-05-29 → 2026-05-30. **11a** (✅): I/O Input/Output trims (independent L/R + link bools), positive-only Drive (0..+24 dB), Ceiling (0..−24 dB). **11b1** (✅): Gain⇄Ceiling structural Link (control coupling, no DSP). **11b2** (✅): `gain_match_auto` + hidden `learned_ref_lufs`; two new `LoudnessAnalyzer` instances at post-Input-Gain and post-Limiter; Learn arm-and-capture (3 s short-term snapshot); Track loop (clamp(ref−live, ±12 dB) at τ ≈ 1 s); compensation gain between limiter and Output. **11b2.1** (✅): `plugin_bypass` as JUCE host bypass param via `getBypassParameter()`; matched-bypass hardened (IO trims always survive); in-UI Bypass label+colour feedback; Learn relocated next to Auto/Track. **11b2.2** (✅): bypass-transition click fix — always-running limiter chain + `juce::dsp::DelayLine` sized to `baseLatencySamples_` + `LinearSmoothedValue` 5 ms crossfade between live and dry-aligned signals. Bench Slice 3/4/5 PASS unchanged across the entire family. Audition note in PROGRESS.md: Ableton's green Device Activator LED is a host-side track enable, not bound to VST3 bypass params — in-UI Bypass button is canonical. |
| 12 | ✅ **Shipped — Clipper gain-staging + Hard/Soft modes + threshold semantics + drive-attributable meter (ADR-0010)** | ADR-0010 (incl. §3 revision + §1b addition) | new `clipper_mode` param; range flip on `clipper_drive_db`; chain refactor; UI relocation | Shipped 2026-05-30. Drive (`input_gain_db`) moves AFTER the clipper inside the OS region — clipper input is now post-IO-Input only, independent of Drive and Ceiling. Removed the `if (clipper_drive_db > 0)` gate. New `clipper_mode` Choice (`Hard`/`Soft`, default `Hard`). Soft curve uses `tanh`-knee at `k = 0.891` (≈ −1 dBFS) asymptoting to ±1.0; Hard branch is the Slice 9 `clamp(±1.0)` exactly. Threshold-style semantic added mid-slice (12.2): `clipper_drive_db` range flipped to `[−12, 0] dB` with self-compensating math (`g = decibelsToGain(−drive)`, `out = curve(x·g) / g`) so peaks pull down to threshold while body level is preserved. Drive-attributable meter formula reports per-block max curve attenuation depth, unified for both modes. UI relocated to sit between Ceiling and GR meter at intermediate size. Bench Slice 3/4/5 PASS 13/13, 14/14, 25/25 unchanged at defaults (Hard + drive 0 + signals below ±1.0 → identity). Slice 11b1 Gain⇄Ceiling Link re-auditioned: math unchanged, audible feel approved post-chain-change. |
| 13 | **NEXT — LUFS calibration (~1 LU offset vs WLM / Insight 2)** | HQ `LoudnessAnalyzer` | meter readouts | Investigation slice: `mdsp_dsp::LoudnessAnalyzer` reads ~1 LU higher than reference meters (WLM, Insight 2) on the same source. Most likely root cause: K-weighting filter coefficient error (Stage 1 high-shelf or Stage 2 RLB), BS.1770-2 reference constant (`−0.691 dB`) off-by-one, or per-channel weight scaling. No ADR — calibration bugfix. Validation against a known LUFS test sample. |

Note: Slice 12 (Clipper gain-staging + Hard/Soft + threshold +
meter, ADR-0010) closed 2026-05-30. Slice 13 (LUFS calibration)
is the active slice — HQ-side bugfix on `mdsp_dsp::LoudnessAnalyzer`
producing a ~1 LU offset vs. WLM/Insight 2 reference meters.
ADR-0009 (multiband detection) remains the next architectural
slice in the backlog. Slice 6 (saturator) still backlogged.

## Backlog

| Status | Title | Touches (HQ) | Touches (product repo) | Rationale |
|--------|-------|--------------|------------------------|-----------|
| Backlog | Multiband detection (detection-bus only) | new ADR-0009 | new product params | Promoted to next architectural slice after Slice 7 close. Closes the remaining ~7 dB null-residual / "open" gap vs Ozone IRC IV (Slice 9.6c reference) AND addresses the visible CPU↔GR correlation surfaced during 7.1.5 (single-band 4× OS limiting puts all peak work in one envelope; multiband distributes across band-specific envelopes, each with lower per-band peak density). Detection-bus only to avoid LR4 audio-path phase concerns per ADR-0005's reasoning. |
| Backlog | Slice 7b: M/S detection | ADR-0008 §Alternatives | `ms_link_pct` already declared in `ParameterIDs.h` | Placeholder ID retained. No active demand. |
| Backlog | Envelope smoothing-stage SIMD | `mdsp_dsp::LimiterEnvelope` | none | The inner ramp loop is NEON-vectorized as of Slice 7.1.4.1. The per-sample smoothing cascade (s1/s2 + T/S s1s/s2s for Clean) remains scalar. If a future regression surfaces the smoothing as a hot path, vectorize. Low priority. |
| Backlog | Envelope snap-event smoother (was proposed Slice 9.6b) | `mdsp_dsp::LimiterEnvelope` | none | Demoted from Slice 9 close per Ozone evidence: only ~2 percentage-point IMD lever vs the 4–7 percent gap to in-family numbers. Revisit if a different future motivation surfaces. |
| Backlog | Optional SP-mode envelope headroom (parity with TP) | — | product `processBlock` | Slice 9 SP mode accepts small 1× SP overs from OS downsample ringing; TP mode bounds via 0.3 dB headroom. If users want bulletproof SP output, add a tiny headroom in SP too. Low priority. |

## Repository locations

- **Product repo (this repo):** `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/`
  — sibling to `AnalyzerPro/`.
- **HQ (consumed via submodule):** `third_party/melechdsp-hq/`
  — also lives locally at `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/`
  (presets prefer the sibling path for fast iteration).

## Documentation locations

- Architecture (ADRs): HQ at `docs/DECISIONS/` — ADR-0004 already, ADR-0005/0006 to come.
- Product spec: this repo at `docs/SPEC.md`.
- Progress log: this repo at `docs/PROGRESS.md`.
- Cursor slice prompts: this repo at `PROMPTS/SLICE_NN_*.md`.
- Bench artifacts (from Slice 2 onward): this repo at `docs/bench/SLICE_NN/`.

## Conventions

- Every Cursor prompt is prefixed with the mandatory
  `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt` preamble and the
  Trinity Protocol retrieval gate from `TEMPLATE_task.txt`.
- Each slice prompt names its files exhaustively. Cursor must STOP if scope
  expands.
- Parameter IDs, once introduced, are frozen. Any change requires a migration
  plan + ADR per system rules.
