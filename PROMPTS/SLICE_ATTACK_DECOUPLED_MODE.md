# SLICE — Attack mode: Ramp (current) vs Real (decoupled time-constant)

**Status:** ready for Cursor · **Architect:** Claude · **Audition/decide:** avishali
**Repos:** SDK `melechdsp-hq` (LimiterEnvelope) + plugin `MasterLimiter`. Commit + push BOTH (SDK first).
**Companion:** `docs/SIGNAL_FLOW.md` §4.3.

---

## Why
Today "attack" is the **shape of the cosine gain-ramp inside the lookahead window**, clamped to `≤ lookahead` — so the gain is always fully down by the peak (transparent), and a slow attack does nothing past the lookahead. That removes the most musical use of attack: **letting transients punch through**. We add a **mode** so the current behavior is preserved (default) and a new **decoupled "real" attack** is selectable.

- **Ramp** (current, default): attack = cosine pre-peak ramp, clamped to lookahead. Transparent smoothness control. Driven by `dev_attack_ms`.
- **Real** (new): attack = a genuine **time-constant** on the gain coming down, **decoupled from lookahead** (can be slower). When slower than the lookahead, the gain isn't fully down by the peak → the transient **punches through** → the FinalCeiling brickwall catches the tip (= punch, with a touch of true-peak-clip character on the very tips). Driven by `dev_real_attack_ms`.

---

## Allowed files to touch
```
# SDK
melechdsp-hq/shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h
melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp
# Plugin
Source/parameters/ParameterIDs.h
Source/parameters/Parameters.cpp
Source/PluginProcessor.h / PluginProcessor.cpp
Source/ui/DevControlsComponent.{h,cpp}
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_ATTACK_DECOUPLED_MODE_CLOSE.md  (new, at close)
```

---

## 1. SDK — `LimiterEnvelope` attack mode
### Header
```cpp
enum class AttackMode { Ramp, Real };
void setAttackMode (AttackMode mode) noexcept;        // default Ramp (current behavior)
void setRealAttackMs (float ms) noexcept;             // Real-mode attack time constant
...
AttackMode attackMode_ = AttackMode::Ramp;
float realAttackMs_ = 5.0f;
float realAttackAlpha_ = 0.0f;                        // exp(-1/(realAttackMs*sr/1000)); recompute on set/prepare
```
`recomputeRealAttack()`: `realAttackAlpha_ = std::exp(-1.0f / std::max(1.0f, realAttackMs_*1e-3f*(float)sampleRate_));` (call from prepare + the setters).

### `recomputeAttackSamples()`
- **Ramp mode:** unchanged (override/Character → cosine ramp, clamped to `lookaheadSamples_`).
- **Real mode:** set `attackSamples_ = 1` (no cosine pre-ramp; the attack is the time-constant in `process()`), then `refreshAttackTable()`. So `ext_` carries the **instant** lookahead-min target.

### `process()` — the attack branch (gain coming DOWN, `inp < state - eps`)
This branch exists in BOTH release paths (AdaptiveSigma and LookaheadFollower) and in the manual path. Modify each:
- **Ramp mode (current):** snap to target — `s1 = s2 = inp;` (and `laStages_.fill(inp)` in the follower). Unchanged.
- **Real mode:** apply the real attack time-constant toward `inp` instead of snapping:
  ```cpp
  // 2-pole attack smoother toward the (instant) target
  s1 = realAttackAlpha_ * s1 + (1.0f - realAttackAlpha_) * inp;
  s2 = realAttackAlpha_ * s2 + (1.0f - realAttackAlpha_) * s1;
  // (LookaheadFollower: run laStages_ cascade toward inp with realAttackAlpha_)
  ```
  So the gain ramps DOWN at the real attack rate. With lookahead advancing detection, a fast TC reaches target before the peak (clean); a slow TC doesn't → overshoot → FinalCeiling catches it.
- The **release** side (gain coming UP) is unchanged in both modes — the chosen release engine handles it.

> RT-safe: no allocation; just a branch on `attackMode_` and an extra alpha. Default Ramp = byte-identical to today.

---

## 2. Plugin — params + wiring
### Params (`ParameterIDs.h`, `Parameters.cpp`)
| ID | Type | Range | Default |
|---|---|---|---|
| `dev_attack_mode` | Choice `{"Ramp","Real"}` | — | index 0 (Ramp) |
| `dev_real_attack_ms` | Float (ms) | **0.05 – 100.0** (skew ~0.3 toward fast) | **5.0** |

Keep `dev_attack_ms` as-is (Ramp-mode control). Cache raw pointers + jasserts for the two new params.

### `configureEnvelope` (PluginProcessor.cpp)
```cpp
const int  attackModeIdx = devAttackMode_ ? (int) devAttackMode_->load() : 0;
const float realAttackMs = devRealAttackMs_ ? devRealAttackMs_->load() : 5.0f;
const auto attackMode = attackModeIdx == 1 ? LimiterEnvelope::AttackMode::Real
                                           : LimiterEnvelope::AttackMode::Ramp;
```
Add to the lambda (all four envelopes):
```cpp
envelope.setAttackMode (attackMode);
envelope.setRealAttackMs (realAttackMs);
// existing: envelope.setAttackOverrideMs (devAttackMs);  // Ramp-mode attack
```

## 3. UI (`DevControlsComponent`)
In the **ATTACK** section add:
- **Attack Mode** combo (`dev_attack_mode`: Ramp / Real).
- **Real Attack** slider (`dev_real_attack_ms`, ms) next to the existing **Attack** slider.
- Optional polish: grey out the inactive control for the current mode (Attack greyed in Real mode; Real Attack greyed in Ramp mode). Tooltip on Real Attack: *"Decoupled attack time-constant; slow = transients punch through to the ceiling (punch)."*

---

## 4. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
# (system install per current convention)
auval -v aufx MaLm Melc 2>&1 | tail -5
```
Acceptance:
1. SDK + plugin build clean; AU validates.
2. **Attack Mode = Ramp** → identical to current behavior (Ramp default; `dev_attack_ms` drives it).
3. **Attack Mode = Real** → **Real Attack** sweeps a genuine time-constant: fast = clean/controlled, **slow = transients punch through** (visible on the History Graph GR onset; audible as punch). A slow Real Attack > lookahead clearly lets the transient peak overshoot the limiter (caught by FinalCeiling) — confirm output true-peak still ≤ ceiling.
4. RT-safe; CPU ~7–10%.

Close gate: update `docs/SIGNAL_FLOW.md` §4.3 (attack modes: Ramp vs Real/decoupled), §6 (new DEV controls), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit + push SDK→plugin; archive CLOSE prompt.

---

## Notes
- This is the first half of the **Dual-limiter (#1)** idea: a slow main attack that lets transients through. The clean version (a fast dedicated catcher so the tips aren't brickwall-clipped) is the full Dual limiter — future.
- When we bake voicing for 0.4, the chosen attack mode + value get baked in with the rest; the DEV controls are removed.
- avishali audition: Real mode, push input gain, sweep Real Attack slow→fast on drums — should go from punchy/transient-through to tight/controlled.
