# MasterLimiter 0.3.0-beta

A mastering maximizer — early tuning beta. This build exposes extra **DEV controls** so testers can help voice the attack/release. See the included **Beta Tester Guide** for the full workflow.

## What's in this build
- **New lookahead-follower release** (smoother, program-dependent) — selectable in the DEV window vs the legacy adaptive release.
- **History Graph** window — see gain-reduction over time while you tune.
- **DEV tuning window** — Attack, Lookahead (Band/Wide), Release (Time/Poles), all grouped by section.
- **User presets** — save/load named voicings; **A/B** compare two settings instantly.
- Trimmed lookahead (max 6 ms) → lower latency (~14 ms).
- All four formats: **AU, VST3, AAX (Pro Tools), Standalone** — universal (Apple Silicon + Intel), signed & notarized.

## How to help
Tune a voicing you like (Engine = Lookahead, then sweep Attack / LA Band-Wide / LA Release / Poles), **save it as a preset**, and email the `.mlpreset` files back. Each preset captures the complete setting.

## Known beta notes
- DEV controls are temporary (they'll be baked into simpler controls for release).
- Character is intentionally inactive while attack is tuned via the DEV knob.
- Intermediate Color (between 0% and 100%) can shift the low end slightly — known, being reworked.

Thanks for testing — your ears are shaping this. 🎚️
