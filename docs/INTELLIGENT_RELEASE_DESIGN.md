# Intelligent (Program-Dependent) Release Engine — Design

**Status:** 🎯 DESIGN, approved direction (avishali 2026-07-05: Model A · Ozone-like leakage · new engine) · **Architect:** Claude
**Goal:** close the measured **~2.6 dB macro-dynamic (300 ms) flattening** vs Ozone IRC 1 — the single confirmed cause of "Ozone sounds more open/punchy" (`docs/LIMITER_TYPES.md` 2026-07-05). Metric = 300 ms loudness range on avishali's two mixes: JAZZ **2.1 → ~4.7**, EDM **2.5 → ~5.1**, at matched loudness, **without** raising LF THD.

---

## Why today's release flattens (measured + code)

The **LookaheadFollower** release recovers the gain toward **`laMinOut_[j]` — the minimum required gain over the lookahead window** ([`LimiterEnvelope.cpp:490`](../../melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp:490)), *not* toward unity, at a **fixed** rate (`laReleaseAlpha_`). On a dense master there's always a peak in the window → the gain is **pinned to the window-min → never recovers → flat.** Correct for a brickwall, fatal for breathing.

The legacy **AdaptiveSigma** ([:514-546](../../melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp:514)) has the *right* idea — a `sigma` sustain-tracker driving a **program-dependent release rate** (`relAlpha = fast + sigma·(slow−fast)`) — but it releases toward the *instantaneous* `ext_[j]` with **no lookahead**, so it distorts and under-catches (lost the release sweep).

**Neither breathes: one has lookahead but a fixed, window-min-pinned release; the other has program-dependent release but no lookahead.**

---

## The design: `ReleaseEngine::AdaptiveLookahead` (working name)

**Graft the two:** LookaheadFollower's peak-catching (the `ext_` attack tent + the `laMinOut_` window-min) **+** AdaptiveSigma's program-dependent release *rate* **+** *leakage* (recover past the hard window-min, let FinalCeiling clean up).

### Per-sample release branch (new engine)
```
// 1. SUSTAIN TRACKER (from AdaptiveSigma) — is the current reduction transient or sustained?
depth = clamp((1 - g) * kDepthScale, 0, 1);
if (depth > sig) sig = sigAtkAlpha*sig + (1-sigAtkAlpha)*depth;   // rises fast (deepening)
else             sig = sigRelAlpha*sig + (1-sigRelAlpha)*depth;   // falls slower (recovering)

// 2. PROGRAM-DEPENDENT RATE — fast recover after transients, slow during sustained
relAlpha = fastAlpha + sig * (slowAlpha - fastAlpha);

// 3. LEAKAGE TARGET — relax the hard window-min so the gain can breathe up between hits
//    leak ∈ [0,1]: 0 = today's hard window-min (no leak); 1 = ignore the far window, recover toward unity.
target = mix(laMinOut_[j], laMinShort_[j], leak);     // laMinShort_ = min over a SHORTER window (or 1.0)

// 4. N-pole release toward target at the program-dependent rate (reuse laStages_ cascade)
for p in poles: laStages_[p] = relAlpha*laStages_[p] + (1-relAlpha)*target; g = laStages_[last];
```
Attack branch: **unchanged from LookaheadFollower** (catch the peak via `ext_` tent + snap/RC).

### Why it breathes (the mechanism)
- **During a sustained loud section:** `sig` high → slow release + window-min holds → gain stays down → the section is controlled, no pumping, no LF distortion.
- **When the section eases:** demand drops → `sig` falls **fast** → `relAlpha` becomes fast → gain recovers quickly toward the **leak-relaxed** target → **the macro level springs back up → breathes.**
- **Leakage** lets the recovery climb above the rigid window-min → more breathing + more loudness; the peaks that slip are caught by the **TruePeak FinalCeiling** (which we make fast so *it* doesn't re-pump — see paired change).

This is exactly IRC 1's "fast on transients (no pump), slow on bass (no distortion)" — with our lookahead keeping it clean.

---

## Tunable knobs (DEV, for voicing the prototype)
- **Fast release ms** (`fastAlpha`) — the post-transient recovery speed (the breathing).
- **Slow release ms** (`slowAlpha`) — the sustained-content release (LF cleanliness).
- **Sustain sensitivity** (`kDepthScale` / `sigAtk`,`sigRel`) — how quickly it decides "sustained" vs "transient."
- **Leak** (0–1) — how far past the window-min to recover (the loudness/breathing ↔ FinalCeiling-workload trade).

---

## Paired change: FinalCeiling must stop pumping under leakage
With leakage, FinalCeiling catches more true-peak transients. Measured today: FinalCeiling **flattens 300 ms range 8.2→6.7 and costs 1.7 dB** because its release pumps. So the engine change must be paired with a **faster / program-dependent `FinalCeilingLimiter` release** (or it re-flattens what we just freed). Prototype the engine first (measure), then address FinalCeiling if it pumps.

---

## Rollout (safe, A/B-able)
- **New `ReleaseEngine` enum value — ADDITIVE.** LookaheadFollower + AdaptiveSigma paths **bit-identical**. `dev_release_engine` gains a 3rd option ("Adaptive LA" / TBD name). Ships opt-in; nothing changes until it wins the A/B.
- **Metric-driven:** Claude measures 300 ms range on the two mixes each iteration; target the Ozone numbers at matched loudness with LF THD ≤ Real's.
- **Voicing:** avishali → Asaf once it measures right.

## Open questions for avishali
- Engine name.
- Start leak conservative (~0.3) and open up, or start aggressive?
- Reuse `dev_la_release`/`poles` for fast/slow, or add dedicated fast/slow knobs? (Prototype adds dedicated so we can tune independently.)
