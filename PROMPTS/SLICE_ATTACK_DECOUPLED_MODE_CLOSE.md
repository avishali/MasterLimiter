# SLICE CLOSE — Attack mode: Ramp vs Real (decoupled)

**Closed:** 2026-06-22 · **Repos:** melechdsp-hq (SDK) + MasterLimiter (plugin)

## What shipped
- `LimiterEnvelope::AttackMode` — **Ramp** (default, byte-identical) vs **Real** (2-pole decoupled attack TC).
- Plugin params: `dev_attack_mode`, `dev_real_attack_ms`; wired to all four envelopes.
- DEV UI: Attack Mode combo + Real Attack slider; inactive control greyed per mode.

## Files touched
- SDK: `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.{h,cpp}`
- Plugin: `ParameterIDs.h`, `Parameters.cpp`, `PluginProcessor.{h,cpp}`, `DevControlsComponent.{h,cpp}`
- Docs: `SIGNAL_FLOW.md` §4.3 + §6, `PROGRESS.md`, `PROMPTS/PLAN.md`

## Acceptance (pending avishali audition)
1. SDK + plugin build clean; AU validates.
2. Ramp mode = current behavior (`dev_attack_ms`).
3. Real mode: slow Real Attack lets transients punch through; fast = controlled.
4. RT-safe; no allocation on audio thread.
