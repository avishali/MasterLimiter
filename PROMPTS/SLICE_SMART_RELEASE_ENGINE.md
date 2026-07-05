# SLICE — "Smart" release engine (program-dependent + lookahead + leakage) — PROTOTYPE

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (rig, 300ms range) · **Audition:** avishali → Asaf
**Repos:** SDK `melechdsp-hq` (LimiterEnvelope) + plugin `MasterLimiter`. New DEV release engine — **opt-in, ADDITIVE. LookaheadFollower + AdaptiveSigma bit-identical.**
**Design:** `docs/INTELLIGENT_RELEASE_DESIGN.md`. **Goal metric:** 300ms loudness range on avishali's mixes — JAZZ 2.1→~4.7, EDM 2.5→~5.1, at matched loudness, LF THD ≤ Real's.

> ⚠️ **Retrieval log first.** Read the current `LimiterEnvelope.{h,cpp}` release section (the `ReleaseEngine::LookaheadFollower` branch ~429-513, the `AdaptiveSigma` branch ~514-553, the `laMinOut_` deque ~431-458, coefficient setters) and the plugin's `configureEnvelope` + `dev_release_engine` mapping. Output actual lines before editing.

---

## Concept (see design doc)
`Smart` = LookaheadFollower's **peak-catching** (the `ext_` attack tent + `laMinOut_` window-min) + AdaptiveSigma's **program-dependent release rate** (fast after transients / slow during sustained) + **leakage** (recover past the rigid window-min; peaks that slip → TruePeak FinalCeiling). This is what lets the macro-level breathe.

---

## SDK — `melechdsp-hq` LimiterEnvelope (touch ONLY this file pair; leave quell/StftEngine WIP)

### 1. Enum (`LimiterEnvelope.h` ~34)
`enum class ReleaseEngine { AdaptiveSigma, LookaheadFollower, Smart };` (append — do not reorder).

### 2. New state + setters (`.h` + `.cpp`)
- State (per-instance, reset in `reset()`): `float smartSig_ = 0.0f;`
- Coefficients: `float smartFastAlpha_, smartSlowAlpha_, smartSigAtkAlpha_, smartSigRelAlpha_, smartLeak_ = 0.0f, smartDepthScale_ = 4.0f;`
- Setters (compute alphas from ms via the existing `releaseAlphaForMs`/`exp(-1/(ms*sr))` pattern; store on sampleRate change too):
  - `setSmartFastReleaseMs(float)` → `smartFastAlpha_`
  - `setSmartSlowReleaseMs(float)` → `smartSlowAlpha_`
  - `setSmartSustainMs(float)` → drives `smartSigAtkAlpha_`/`smartSigRelAlpha_` (attack fast, release = sustain ms; mirror AdaptiveSigma's `autoSigmaAttackAlpha_`/`autoSigmaAlpha_`)
  - `setSmartLeak(float 0..1)` → `smartLeak_`

### 3. Release branch — insert a new `else if` **before** the AdaptiveSigma fallback
Structure: `if (LookaheadFollower) {...unchanged...} else if (autoRelease_ && releaseEngine_ == Smart) { NEW } else if (autoRelease_) {...AdaptiveSigma unchanged...}`.

The `Smart` branch:
1. **Compute `laMinOut_` exactly as the LookaheadFollower branch does** ([:431-458](../../melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp:431)) — the sliding-window-min deque. (Duplicate the code, or factor a shared helper that LookaheadFollower also calls — but LookaheadFollower must stay bit-identical.)
2. Per-sample `j`, with `g` seeded from `laStages_[poles-1]` and `poles = clamp(lookaheadPoles_,2,4)`:
```cpp
const float instant = ext_[j];
const float winMin  = laMinOut_[j];
if (instant < g - 1e-12f) {
    // ATTACK — identical to LookaheadFollower (Ramp snap / else RC via realAttackAlpha_)
} else {
    // 1. sustain tracker (feedforward from windowed demand)
    const float depth = std::clamp((1.0f - winMin) * smartDepthScale_, 0.0f, 1.0f);
    if (depth > smartSig_) smartSig_ = smartSigAtkAlpha_*smartSig_ + (1-smartSigAtkAlpha_)*depth;
    else                   smartSig_ = smartSigRelAlpha_*smartSig_ + (1-smartSigRelAlpha_)*depth;
    // 2. program-dependent rate: fast when sig low (transient), slow when high (sustained)
    const float relAlpha = smartFastAlpha_ + smartSig_*(smartSlowAlpha_ - smartFastAlpha_);
    // 3. LEAKAGE: blend release target from the rigid window-min toward the instantaneous demand
    const float target = (1.0f - smartLeak_)*winMin + smartLeak_*instant;
    // 4. N-pole release toward target at relAlpha (reuse laStages_ cascade)
    float x = target;
    for (int p=0;p<poles;++p){ laStages_[p]=relAlpha*laStages_[p]+(1-relAlpha)*x; x=laStages_[p]; }
    g = x;
}
g = std::clamp(g,1e-12f,1.0f); gainOut[j]=g; /* maxGr as in LookaheadFollower */
```
3. After the loop, set `s1=s2=s1s=s2s=g` (as LookaheadFollower does at :509-512).

> Leakage mechanism: `smartLeak_` toward `instant` reduces the pre-dip for upcoming peaks → they arrive un-attenuated → leak to FinalCeiling → the gain recovers higher between hits = breathing. `smartLeak_=0` = window-min pinned (≈ today). Start conservative.

### Guard: LookaheadFollower & AdaptiveSigma **bit-identical** (their branches untouched; Smart is a new `else if`). Latency unchanged (Smart uses the same lookahead/pad).

---

## Plugin — `MasterLimiter`

### Params (`ParameterIDs.h`/`Parameters.cpp`) — 1 choice edit + 4 new DEV floats
- `dev_release_engine` Choice `{ "Adaptive", "Lookahead" }` → `{ "Adaptive", "Lookahead", "Smart" }`. **Keep default index 1 (Lookahead)** — Smart opt-in.
- New DEV (mirror `dev_la_release` pattern, cache pointers + jassert):
  - `dev_smart_fast_ms` — 1…200, default **20**
  - `dev_smart_slow_ms` — 50…1000, default **300**
  - `dev_smart_sustain_ms` — 10…500, default **120**
  - `dev_smart_leak` — 0…1 (step 0.01), default **0.3** (conservative)

### DSP wiring (`PluginProcessor.cpp`)
- Map `dev_release_engine` index 2 → `ReleaseEngine::Smart` (the `laEngine` assignment ~1350).
- In `configureEnvelope` (~1370): when applicable, call `envelope.setSmartFastReleaseMs(...)` etc. from the new params (unconditional set is fine — they only take effect when engine==Smart). Read the 4 params once/block.

### DEV UI (`DevControlsComponent`)
- Add "Smart" to the engine combo.
- New group **"RELEASE · Smart"** with 4 sliders: **Fast (ms) · Slow (ms) · Sustain (ms) · Leak** (0–1). Mirror the existing release sliders. Enable/grey them when engine==Smart (extend `updateReleaseEngineEnablement()`).

---

## Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build --config Release 2>&1 | tail -6
bash scripts/install_user.sh build      # ⚠️ pass 'build'
auval -v aufx MaLm Melc 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–4 + measures 5; avishali auditions 6):**
1. Build clean (SDK+plugin), AU validates, **no latency change**.
2. **LookaheadFollower & AdaptiveSigma bit-identical to HEAD** (offline null at their engine settings). Headline guard.
3. `dev_release_engine` shows Smart; 4 Smart sliders present + greyed correctly per engine. Verify installed via **VST3** (AU resolves by code).
4. Smart at `leak=0` ≈ LookaheadFollower (sanity — with leak off it should be close to the pinned behavior).
5. **(Claude runs the rig)** Smart on jazz + EDM: sweep `leak` (0→0.5) and fast/slow — does **300ms range rise toward 4.7/5.1** at matched loudness **without** LF THD climbing? Report the range/loudness/THD table per leak.
6. **Audition (avishali):** does it breathe/open up vs Lookahead without pumping or distortion?

**Close gate:** update `docs/SIGNAL_FLOW.md` + `docs/INTELLIGENT_RELEASE_DESIGN.md` (results) + `docs/PROGRESS.md` + `PROMPTS/PLAN.md`; commit SDK + plugin separately; **push** (hold resolved). Archive CLOSE.

## Output requirements
1. Retrieval log. 2. SDK diff (enum, state, setters, Smart branch). 3. Plugin diff (params, mapping, UI). 4. Build+auval. 5. Lookahead/Adaptive null evidence. 6. Latency before/after. 7. Both commit hashes. 8. Open questions.

## Notes for the architect (not for Cursor)
- The whole risk to the null is "did the LookaheadFollower/AdaptiveSigma branches change" — they must not; Smart is purely additive.
- Expect to iterate on the rig: leak + fast/slow are the voicing space. Paired FinalCeiling-release fix comes next if FC pumps under leakage (separate slice).
