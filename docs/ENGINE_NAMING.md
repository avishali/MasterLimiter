# Engine Naming — Source of Truth

**Purpose:** ONE authoritative map of product engine names ↔ DSP mechanism ↔ parameters, so docs/UI/code stay consistent as engines are added. Established 2026-07-06.

## Two naming layers (do not collapse them)
- **Product layer (user/tester-facing):** the "engine" the user selects — **Transparent**, **Open**, later **Smart**, **Spectral**. Used in the UI engine selector, DEV panel labels, and product docs (`SPECTRAL_ENGINE_DESIGN.md`, `SIGNAL_FLOW.md`, `MANUAL.md`, etc.).
- **SDK/DSP layer (technical, shared):** `MultibandLimiter`, `SingleBandLimiter`, `LimiterEnvelope::ReleaseEngine::{Lookahead,Smart,AdaptiveSigma}`, `FinalCeilingLimiter`, crossovers. **NEVER rename these to product names** — they are shared primitives used by other products; the product name is a *composition* of them.

## The engines (product name = the source of truth for UI + docs)

| Product engine | What it brings | DSP mechanism | Params / switch | Tester status |
|---|---|---|---|---|
| **Transparent** | Clean, controlled baseline limiting | inline wideband/3-band limiter, `LimiterEnvelope` **LookaheadFollower** release + `LinearPhaseCrossover` tree | `dev_mb_engine = OFF`, `dev_release_engine = Lookahead` | ✅ in alpha selector |
| **Open** | Loud + open, Ozone-parity macro-breathing | SDK **`MultibandLimiter`** (2-band `LinkwitzRileyBandSplitter` + `SingleBandLimiter`) → **Ceiling** tip-catch | `dev_mb_engine = ON` (+ `dev_mb_*`) | ✅ in alpha selector |
| **Smart** | *(intended)* program-dependent breathing via adaptive release | inline limiter, `ReleaseEngine::Smart` (`dev_smart_*`) | `dev_release_engine = Smart` | ⏸ **PARKED** — next dev target after alpha voicing begins; NOT in the alpha tester selector |
| ~~Adaptive~~ | — | `ReleaseEngine::AdaptiveSigma` (legacy) | `dev_release_engine = Adaptive` | ❌ legacy — remove from selector |

## Stages (shared across engines) — CLIP-1
```
[ optional Drive (tone) ] → Engine (Transparent / Open) → [ Ceiling (peak safety) ] → out
```
- **Drive** = optional PRE-engine tone/saturation (the old "Clipper", pre-only). Character, not peak safety. Params keep `clipper_*` IDs to avoid preset breakage; UI/docs say "Drive". Hard/Soft via `clipper_mode`.
- **Ceiling** = the single peak-safety stage (main window). On/Off = `dev_final_ceiling`; Release = `dev_final_ceiling_release_ms` (0…100 ms; **0 = Clip** detent = oversampled hard-clip at `ceiling_db`; >0 = `FinalCeilingLimiter` at that release). Output Level remains `ceiling_db`.
- The **Open** engine uses Ceiling at the Clip end as its tip-catcher — no forced user clipper.
- One Drive (tone) + one Ceiling (peak) → you cannot stack two peak stages.

## Rules
- **Alpha tester selector = 2 engines: Transparent · Open.** Smart/Adaptive kept as params (for our R&D) but NOT presented to the tester.
- When adding/renaming an engine, **update this table first**, then propagate the product name to: UI labels, `SPECTRAL_ENGINE_DESIGN.md`, `SIGNAL_FLOW.md`, `MANUAL.md`, and any DEV prompt. SDK primitive names stay as-is.
- In docs, refer to engines by product name with the DSP in parentheses on first mention, e.g. *"the **Open** engine (2-band `MultibandLimiter` + Ceiling clip)."*
- **"Smart" is the next engine we build** (after alpha voicing starts) — keep its params and this row; do not delete.
