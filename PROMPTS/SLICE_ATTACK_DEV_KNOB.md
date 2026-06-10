# SLICE — Limiter attack DEV knob (+ Character switched off for tuning)

**Status:** ready for Cursor · **Architect:** Claude · **Audition/decide:** avishali
**Repos:** SDK `melechdsp-hq` (attack override) + plugin `MasterLimiter`. Commit + push BOTH (SDK first).
**Companion:** `docs/SIGNAL_FLOW.md` §4.3 (attack), §6 (DEV controls).

---

## Goal
Add a **live DEV knob for the limiter attack time** so it can be tuned directly, and **take the Character control out of the loop** while tuning. Today attack is derived from **Character** (Clean=3 ms / Tight=1 ms / Aggressive=0.3 ms via `LimiterEnvelope::recomputeAttackSamples`). The DEV attack knob **overrides** that for all envelopes; Character is greyed-out/inert until we bake final values.

DEV-only (orange strip), like the release/lookahead knobs. One knob, applied to all four envelopes (low/high/wide/wideR) — matching how Character drives them all today.

### Key constraint
Attack can never exceed the active lookahead window — `attackSamples_` is clamped to `[1, lookaheadSamples_]`. So the DEV attack is clamped to whatever **LA Band/LA Wide** are set to. (Per-envelope: low/high clamp to LA Band, wide clamps to LA Wide.) This coupling is correct — note it in the UI/tooltip.

---

## Allowed files to touch
```
# SDK
melechdsp-hq/shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h
melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp
# Plugin
Source/parameters/ParameterIDs.h
Source/parameters/Parameters.cpp
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/ui/MainView.h
Source/ui/MainView.cpp
docs/SIGNAL_FLOW.md
docs/PROGRESS.md
PROMPTS/PLAN.md
PROMPTS/SLICE_ATTACK_DEV_KNOB_CLOSE.md   (new, at close)
```

---

## 1. SDK — attack override in `LimiterEnvelope`
### Header
```cpp
void setAttackOverrideMs (float ms) noexcept;   // ms > 0 = override; ms <= 0 = use mode (default)
...
float attackOverrideMs_ = 0.0f;                 // 0 = disabled (mode-derived attack)
```
### Impl
```cpp
void LimiterEnvelope::setAttackOverrideMs (float ms) noexcept
{
    const float v = (ms > 0.0f ? ms : 0.0f);
    if (std::abs (attackOverrideMs_ - v) <= 1.0e-6f) return;
    attackOverrideMs_ = v;
    recomputeAttackSamples();
}
```
In `recomputeAttackSamples()`, intercept before the mode switch:
```cpp
void LimiterEnvelope::recomputeAttackSamples() noexcept
{
    if (attackOverrideMs_ > 0.0f)
    {
        attackSamples_ = std::max (1, (int) std::llround (attackOverrideMs_ * 1.0e-3 * sampleRate_));
    }
    else
    {
        switch (mode_) { /* ... existing Clean/Tight/Aggressive ... */ }
    }
    attackSamples_ = std::clamp (attackSamples_, 1, lookaheadSamples_);   // keep ≤ active lookahead
    refreshAttackTable();
}
```
- This composes correctly with the existing callers: `setMode()`, `setActiveLookaheadSamples()`, and `prepare()` all call `recomputeAttackSamples()`, so changing lookahead re-clamps the override-derived attack to the new window automatically.
- RT-safe (no allocation; `refreshAttackTable` writes into the max-sized `attackTable_`).
- Default `attackOverrideMs_ = 0` → byte-identical to today (mode-derived).

---

## 2. Plugin — param, wiring, Character switch-off
### 2a. Param (`ParameterIDs.h`, `Parameters.cpp`)
```cpp
inline constexpr std::string_view dev_attack_ms { "dev_attack_ms" };
```
| ID | Type | Range | Default |
|---|---|---|---|
| `dev_attack_ms` | Float (ms) | **0.05 – 10.0** (skew ~0.35 toward the fast end) | **3.0** (= current Clean) |

Cache `devAttackMs_` raw pointer + jassert.

### 2b. Apply in the configure block (`PluginProcessor.cpp`, alongside the lookahead/release setters ~L803–823)
```cpp
const float devAttackMs = devAttackMs_ ? devAttackMs_->load (std::memory_order_relaxed) : 0.0f;
```
Add to the `configureEnvelope` lambda (applies to all four envelopes):
```cpp
envelope.setAttackOverrideMs (devAttackMs);   // DEV: overrides Character-derived attack
```
**Character switched off (for now):** because the override is always active while this DEV knob ships, Character no longer drives attack. In Auto-release mode (the current focus) Character had *no other* audible effect, so no further change is needed to the engine. Just make it clear in the UI (§2c). Leave `setMode(envelopeMode)` as-is — harmless (its only remaining effect, the manual-release sustain split, is inactive in Auto).

### 2c. UI (`MainView.h/.cpp`)
- Add a DEV slider **Attack (ms)** → `dev_attack_ms`, in the DEV strip, APVTS-attached, matching existing dev-knob styling. Tooltip/label note: *"Overrides Character; capped by lookahead."*
- **Grey out / disable the Character control** (`setEnabled(false)` + a muted look) so it's visually clear Character is inactive during tuning. Do **not** remove or unbind it — we re-enable it when the DEV knob is removed. (A one-line comment marking this as temporary is enough.)

---

## 3. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -20
cp -R build/MasterLimiter_artefacts/Release/AU/MasterLimiter.component  ~/Library/Audio/Plug-Ins/Components/
cp -R build/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3     ~/Library/Audio/Plug-Ins/VST3/
auval -v aufx MaLm Melc 2>&1 | tail -5
```
Acceptance:
1. SDK + plugin build clean; AU validates.
2. **Attack knob audibly changes the limiter attack** — fast (sub-ms) = snappier/more aggressive, slow (toward lookahead) = softer onset; visible on the history-graph GR trace (the attack slope into each dip).
3. Attack is **clamped by lookahead** — raising LA Band/Wide lets the attack go slower; at very short lookahead the attack knob tops out (no error, just clamps).
4. **Character control is greyed out** and changing it does nothing while the DEV attack knob is present.
5. RT-safe; CPU still ~7–10%.

Close gate: update `docs/SIGNAL_FLOW.md` (§4.3 attack now DEV-overridable; §6 add the Attack DEV knob row; note Character is temporarily inert), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit + push **SDK first then plugin**; archive `PROMPTS/SLICE_ATTACK_DEV_KNOB_CLOSE.md`.

---

## Notes / when we bake
- Final step (later): pick attack + LA Band/Wide + LA Release ms + Poles by ear, then **bake** — either fold the attack into the Character modes (re-enable Character with the new values) or promote attack to a proper user param. Remove all DEV knobs for 0.4.
- One knob drives all envelopes (as Character does today). If you later want **per-band attack** (low vs high vs wide), it's a small extension of the same setter — flag it if you want it.
```
