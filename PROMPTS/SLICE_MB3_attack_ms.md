# SLICE MB-3 — expose MB-engine attack time (`dev_mb_attack_ms`)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (param present + sane range) · **Audition:** avishali (LF-distortion voicing)
**Repo/scope:** plugin `MasterLimiter`, MB-engine path only. Additive/DEV-gated. No SDK edits.
**Why:** avishali audition — LF distortion at high push; wants to fine-tune attack. Today the MB path exposes attack MODE (`dev_mb_attack_mode` Ramp/Hybrid/Real) but not attack TIME. Expose it so the transient handling into the clipper is tunable (slower attack → passes more to the clipper; faster → catches more but risks LF grit).

> ⚠️ **Retrieval log first.** Read the MB per-band configure (`configureMbBandLimiter` / where `band(i).setAttackMode` etc. are called) + `SingleBandLimiter.h` attack setters (`setRealAttackMs`, `setAttackOverrideMs`, `setAttackMode`). Output them.

## Change
- **New DEV param `dev_mb_attack_ms`** — Float, range **0.05–50 ms**, default **5.0** ("DEV MB Attack (ms)"). Cache pointer + jassert (mirror `dev_mb_release_ms`).
- **Wire it** in the MB per-band config: `band(i).setRealAttackMs(dev_mb_attack_ms)` for BOTH bands (and the safety limiter if enabled). This sets the attack RC — meaningful for **Hybrid/Real** modes; for **Ramp**, the pre-ramp is set by `dev_mb_lookahead_ms` (already exposed), so document that in the UI tooltip / a code comment.
- **DEV UI:** add the slider to the "MB Engine" group next to attack-mode/release/lookahead.
- Only-apply-on-change (rule §6); no reallocation in process (RT §3).

## Non-goals
- No SDK edits, no toggle-OFF path changes, no new attack behaviour — just expose the existing `SingleBandLimiter` attack-RC setter to the MB path.

## Build/verify/audition
- Build clean, AU validates, toggle-OFF unchanged.
- (Claude) confirm `dev_mb_attack_ms` present in the VST3, range 0.05–50, default 5.
- (avishali) with MB engine ON, sweep attack-ms (+ try Hybrid/Real mode, Soft clip, lower `clipper_db`) and judge the LF-distortion↔breathing trade.

## Output requirements
1. Retrieval log. 2. Diff (param + wiring + UI). 3. Build+auval. 4. Confirm toggle-OFF unchanged. 5. Open questions.

## Notes for the architect
- Small voicing knob; the REAL LF-distortion fix is the leapfrog (`SLICE_SE_band_threshold.md` → P-A). This just buys avishali immediate headroom.
