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
| 7  | Stereo / M-S link options                              | `mdsp_dsp/StereoMSLink`                     | params + UI            | Bench A/B confirms link % continuum behaves monotonically.     |
| 8  | Meters (GR + TP) to mdsp_ui                            | `mdsp_ui/GainReductionMeter`, `TruePeakMeter`| UI assembly           | Meters track snapshots accurately, no UI-thread DSP access.    |
| 9  | Auto-release algorithm + preset pass + ship gate       | `LimiterEnvelope` (auto mode)               | presets                | Full §5 corpus passes at 5 dB GR. ADR-0006 written.            |

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
