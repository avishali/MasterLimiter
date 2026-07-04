# SLICE CLOSE — Hybrid attack mode experiment

**Closed:** 2026-07-04 · **Repos:** melechdsp-hq (LimiterEnvelope enum) + MasterLimiter (plugin)

## What shipped
- SDK: `AttackMode::Hybrid` appended (Ramp=0, Real=1 unchanged). Hybrid gets Ramp-style `recomputeAttackSamples()` + RC-smoothed follower (`!= Ramp` branches).
- Plugin: `dev_attack_mode` {Ramp, Real, Hybrid}, default Real; DEV UI + dual-knob enablement for Hybrid.

## Files touched
- HQ: `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h` only
- Plugin: `Parameters.cpp`, `PluginProcessor.cpp`, `DevControlsComponent.cpp`, docs

## Acceptance
1. Build + auval PASS; latency unchanged (same `baseLatencySamples_` formula; Hybrid uses existing pre-ramp within lookahead budget).
2. Ramp/Real paths untouched (additive enum + index-2 mapping only).
3. Hybrid enables both Attack + Real Atk knobs.
4. All `attackMode_ == Ramp` sites verified; only `== Real` is attackSamples special-case.
5. **Rig (synthetic, LA band=2 / wide=5 ms, +18 dB, ceiling −1, FC off):**
   - **100 Hz bass:** all three hold ceiling; Hybrid THD **−65.3 dB** (best) vs Real −62.7 vs Ramp −46.3.
   - **Transient mix:** only Ramp holds ceiling (−1.0 SP); Real/Hybrid overs ~+11 dB — hypothesis **not met** on transients in this rig.
   - **Defaults (LA=0):** Hybrid ≡ Real (no ceiling hold on bass without lookahead).
6. Program-material A/B audition — **pending** (avishali/Asaf).

## Commits (not pushed)
- HQ: `1df0fcd` — `feat(dynamics): add AttackMode::Hybrid to LimiterEnvelope enum`
- Plugin: `31b6abc` — `feat(dev): add Hybrid attack mode (pre-ramp + RC follower)`
