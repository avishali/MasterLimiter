# SLICE CLOSE — DEV release controls clarity

**Closed:** 2026-07-04 · **Repo:** MasterLimiter (DEV UI + one new DEV param + processor decouple)

## What shipped
- Clearer DEV release row labels and tooltips (Auto Engine, Release (ms), Smoothness, Adapt Onset/Hold, Low/Mid/High/Wide ×, Manual Sustain).
- Section headers renamed for Auto Lookahead / Adaptive legacy / per-band trim.
- `dev_wide_release_scale` (default 1.0) decoupled from `dev_high_band_release_scale` — wideband final stage vs high band only.
- Engine-aware greying: irrelevant engine controls disabled via `setEnabled(false)`; synced on combo change + 30 Hz DEV timer.

## Files touched
- `Source/parameters/ParameterIDs.h`, `Source/parameters/Parameters.cpp`
- `Source/PluginProcessor.h`, `Source/PluginProcessor.cpp`
- `Source/ui/DevControlsComponent.h`, `Source/ui/DevControlsComponent.cpp`
- `docs/SIGNAL_FLOW.md` §6, `docs/PROGRESS.md`, `PROMPTS/PLAN.md`

## Acceptance (pending avishali audition)
1. Build clean; auval PASS; installed fresh. Default sound unchanged (Wide 1× = prior High/Wide combined at 1×).
2. Labels/tooltips match slice table; section headers updated.
3. High × and Wide × independent in DSP.
4. Engine greying toggles with Auto Engine; holds on preset/automation.
5. Controls legible — no tuning dead knobs for selected engine.
