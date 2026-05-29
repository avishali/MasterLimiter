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
| 7  | **NEXT — Stereo / M-S link options**                   | `mdsp_dsp/StereoMSLink`                     | params + UI            | Bench A/B confirms link % continuum behaves monotonically.     |
| 8  | Meters (GR + TP) to mdsp_ui                            | `mdsp_ui/GainReductionMeter`, `TruePeakMeter`| UI assembly           | Meters track snapshots accurately, no UI-thread DSP access.    |
| 9  | ✅ **Shipped — Limiter character + Clipper Drive + on/off** | `LimiterEnvelope::Mode`; `dsp_bench` recal + Ozone reference driver | `limiter_active`, `clipper_drive_db`, expanded `character`, 4× OS limiter chain, TP envelope headroom | Shipped 2026-05-29. 3 Character modes, Clipper Drive stage, limiter on/off, 4× OS limiter chain, ispTrim removed, TP headroom in envelope. Bench Slice 3/4/5 PASS via treatment-B recalibration; Ozone IRC IV reference run documents the in-family character. ADR-0006 + ADR-0007. |
| 10 | Maximizer UI shell (Ozone-inspired two-panel layout)   | —                                           | `ui/MainView`          | Look-lock: two-panel layout, drive Gain at 0 dB hard-left, placeholders for new controls. Audition the look. |
| 11 | I/O gains + dual Gain-Match (auto-track + Gain⇄Ceiling)| (loudness match — likely product-side)      | new params + DSP + UI  | I/O Input/Output trims (independent) + Gain Match (Auto/Track + Gain⇄Ceiling link), positive-only drive. ADR-0007 written. **Split: 11a = ADR + params + DSP wiring (✅ shipped); 11b1 = Gain⇄Ceiling Link (control coupling only); 11b2 = Auto/Track + Learn (one-shot short-term snapshot, momentary tracking) + Bypass-with-match.** |

Note: Slice 7 (stereo/M-S link) is next after Slice 9 close. Slice 6
(saturator) remains in the backlog; ordering was adjusted to do meters
(8), brickwall voicing (9), and the Maximizer UI/I-O model (10–11)
ahead of it.

## Backlog

| Status | Title | Touches (HQ) | Touches (product repo) | Rationale |
|--------|-------|--------------|------------------------|-----------|
| Backlog | Multiband detection (detection-bus only, no audio-path crossover phase issues) | new ADR-0008 | new product params | Closes the ~7 dB null-residual gap and "open" perception gap vs Ozone IRC IV documented in Slice 9.6c reference run. Avoids ADR-0005's LR4 phase concerns by keeping the split detection-only. |
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
