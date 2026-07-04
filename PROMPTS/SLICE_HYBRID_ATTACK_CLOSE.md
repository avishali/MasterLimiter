# SLICE CLOSE — Hybrid attack mode experiment

**Closed:** 2026-07-04 · **Repos:** melechdsp-hq (LimiterEnvelope enum) + MasterLimiter (plugin)

## What shipped
- SDK: `AttackMode::Hybrid` appended (Ramp=0, Real=1 unchanged). Hybrid gets Ramp-style `recomputeAttackSamples()` + RC-smoothed follower (`!= Ramp` branches).
- Plugin: `dev_attack_mode` {Ramp, Real, Hybrid}, default Real; DEV UI + dual-knob enablement for Hybrid.

## Files touched
- HQ: `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h` only
- Plugin: `Parameters.cpp`, `PluginProcessor.cpp`, `DevControlsComponent.cpp`, docs

## Acceptance (pending rig + audition)
1. Build + auval PASS; latency unchanged.
2. Ramp/Real paths untouched (additive enum + index-2 mapping only).
3. Hybrid enables both Attack + Real Atk knobs.
4. All `attackMode_ == Ramp` sites verified; only `== Real` is attackSamples special-case.
5. Offline rig table: Hybrid crest/THD/ceiling vs Ramp/Real.
6. Program-material A/B audition.
