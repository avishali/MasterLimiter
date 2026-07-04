# SLICE — Hybrid attack mode (lookahead pre-ramp + smoothed follower) — EXPERIMENT

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (measures on real mix) · **Audition/decide:** avishali/Asaf
**Repos:** SDK `melechdsp-hq` (LimiterEnvelope only) + plugin `MasterLimiter`. New DEV attack mode — **opt-in, additive, Ramp/Real behavior unchanged.**
**Goal:** test the hypothesis that a **lookahead pre-ramp fed into the RC-smoothed follower** catches transients (like Ramp) with low distortion (like Real) — potentially deferring the full two-stage limiter rebuild. Measured target (from the real-mix analysis): reduce crest ~3–4 dB on peaks, THD near Real's −58 dB (not Ramp's −41 dB), holds the ceiling.

> ⚠️ **Retrieval log first.** Re-confirm every cited line in `melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` + `.../include/mdsp_dsp/dynamics/LimiterEnvelope.h` and the plugin files. Grep **all** `attackMode_ ==` / `AttackMode::` uses and handle the new value at each. Output the log.

---

## Why (measured)

The investigation proved: **Ramp** snaps to a lookahead pre-ramp → catches transients but distorts sustained (THD −41 dB). **Real** forces `attackSamples_ = 1` ([LimiterEnvelope.cpp:279-284](../../melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp:279)) → no pre-ramp, pure RC follower → transparent but **cannot catch transients at any lookahead** (real mix: +4 to +8.6 dB overs). The two knobs the code already has — a **pre-ramp** (tent over the lookahead) and an **RC smoother** (`realAttackAlpha_`) — are never combined. This slice combines them as a third mode.

**The combination is nearly free structurally:** the tent is built for any `attackSamples_ > 1`, and the follower's attack branch already routes **non-Ramp → RC-smoothed** ([:463-499](../../melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp:463)). So a mode that is *not* Ramp (→ gets the smoothed follower) and *not* special-cased to `attackSamples_=1` (→ gets a real pre-ramp) IS the hybrid.

---

## SDK change — `melechdsp-hq` LimiterEnvelope ONLY

⚠️ The shared checkout has unrelated `StftEngine`/quell WIP — **touch only `LimiterEnvelope.{h,cpp}`**. Commit in `melechdsp-hq`, **do NOT push** (Quell hold).

1. **Header enum** (`LimiterEnvelope.h` ~line 47): `enum class AttackMode { Ramp, Real, Hybrid };` (append `Hybrid` — do not reorder; Ramp=0, Real=1 must stay).
2. **`recomputeAttackSamples()`** (`LimiterEnvelope.cpp:279-284`): keep the `if (attackMode_ == Real) { attackSamples_ = 1; ... return; }` special-case **exactly as is**. `Hybrid` is *not* special-cased → it falls through to the existing Ramp-style pre-ramp derivation (attackOverrideMs_ / mode-based, clamped to `lookaheadSamples_`). ✅ Hybrid gets a lookahead pre-ramp.
3. **Follower attack branches**: every `if (attackMode_ == Ramp) { snap } else { RC-smooth }` ([:468](../../melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp:468), and the AdaptiveSigma branches ~531/568/587). `Hybrid != Ramp` → automatically takes the RC-smooth `else`. ✅ **Verify no branch uses `!= Real` or `== Real` in a way that would wrongly route Hybrid.** (Grep confirms; the only `== Real` is the attackSamples special-case.)
4. **No other logic.** Hybrid reuses `realAttackAlpha_` (from `realAttackMs_`) for smoothness and the Ramp attack derivation for pre-ramp length. Latency = same as Ramp (pre-ramp within lookahead) — unchanged.

> Net SDK diff: +1 enum value. The behavior emerges from the existing branch structure. Ramp and Real are **bit-identical** (their code paths are untouched).

---

## Plugin change — `MasterLimiter`

1. **`Parameters.cpp` ~242**: `dev_attack_mode` Choice `{ "Ramp", "Real" }` → `{ "Ramp", "Real", "Hybrid" }`. **Keep default index 1 (Real)** — Hybrid is opt-in for A/B, don't change the shipping default in this experiment.
2. **`PluginProcessor.cpp` ~1336**: the map `attackMode = (attackModeIdx == 1) ? Real : Ramp` → add index 2 → `Hybrid`. (e.g. `attackModeIdx==1 ? Real : attackModeIdx==2 ? Hybrid : Ramp`.)
3. **`DevControlsComponent.cpp`**: combo (`~71-73`) add `cmbAttackMode_.addItem ("Hybrid", 3);` + tooltip: *"Hybrid = lookahead pre-ramp (catches transients like Ramp) fed to the smoothed follower (low distortion like Real). Uses both Attack (pre-ramp length) and Real Atk (smoothness)."*
4. **`updateAttackModeControls(int)` (`~453`)**: Hybrid uses **both** knobs. Currently `ramp = idx==0` greys one or the other. Change so: **Attack** (pre-ramp) enabled when `idx != 1` (Ramp+Hybrid); **Real Atk** enabled when `idx != 0` (Real+Hybrid). Verify against the actual combo-index convention in the retrieval log.

---

## Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
```
**Acceptance (Claude verifies 1–4 + measures 5 on the real mix; avishali auditions 6):**
1. Build clean (SDK + plugin), AU validates, **no latency change**.
2. **Ramp and Real are bit-identical to HEAD** at every setting (their paths untouched; the enum addition is additive). State how confirmed (offline null or bench).
3. Hybrid selectable in DEV; both Attack + Real Atk knobs enabled in Hybrid; greying correct for Ramp/Real.
4. No new warnings; grep shows every `AttackMode`/`attackMode_ ==` site handles the 3rd value.
5. **(Claude runs the rig)** Hybrid vs Ramp vs Real on the real mix + 100 Hz bass: does Hybrid **hold the ceiling** (like Ramp), **reduce crest ~3–4 dB** (like Ramp), and have **THD near Real** (≪ −41 dB)? This is the hypothesis test — report the table.
6. **Audition:** Asaf A/Bs Ramp / Real / Hybrid on program material — transient control without the Ramp "crunch."

**Close gate:** if the hypothesis holds, update `docs/SIGNAL_FLOW.md` (attack modes) + `docs/LIMITER_TYPES.md` (Hybrid may reduce/reshape the two-stage need); `docs/PROGRESS.md`; `PROMPTS/PLAN.md`. Commit SDK (melechdsp-hq) + plugin separately, **neither pushed**. Archive `SLICE_HYBRID_ATTACK_CLOSE.md`.

## Output requirements
1. Retrieval log (all AttackMode sites). 2. SDK diff (enum + confirmation the branches route Hybrid correctly). 3. Plugin diff. 4. Build + auval. 5. Ramp/Real null evidence. 6. Latency before/after. 7. Both commit hashes (SDK + plugin, no push). 8. Open questions.

## Notes for the architect (not for Cursor)
- If Hybrid wins, it may make the full Stage-1/Stage-2 rebuild unnecessary for 0.4 — the single multiband stage in Hybrid mode could be transparent AND transient-controlling. That's the whole point of testing it cheap first.
- Pre-ramp length knob = `dev_attack_ms` (Attack); smoothness = `dev_real_attack` (Real Atk). Asaf's two dials to find the sweet spot.
- Risk is entirely "did Ramp/Real change" (guard #2) and "all AttackMode sites handled" (guard #4). Small diff, but SDK-shared → verify with care.
