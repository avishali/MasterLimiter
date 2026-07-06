# Spectral Dynamic Threshold Lookahead Engine — Design

**Status:** 🎯 DESIGN (2026-07-06) — the leapfrog *above* parity. Architect: Claude, from avishali's direction.
**Companions:** `docs/SPECTRAL_ENGINE_DESIGN.md` (the §4a proof + the 2-band parity floor), `docs/LIMITER_TYPES.md` (roadmap #2 content-aware / #3 spectral), ADR-0013 (the module foundation this builds on).
**Frame:** `manifest-mission` — 2-band parity matches Ozone but is a *me-too*. This engine is the bid to *leapfrog*: a limiter whose per-band thresholds **adapt to the program, with lookahead**, pushing loudness to the threshold of *audible* distortion rather than a fixed level.

---

## 1. The thesis

Today's limiter (and the 2-band parity engine) uses **fixed per-band thresholds** — content-blind. The mission bet is that **program-adaptive per-band thresholds, set from a lookahead spectral analysis, beat any fixed configuration**:
- **Anticipate** (lookahead): see a loud section *before* it arrives → adjust smoothly → no pump.
- **Allocate by audibility** (masking): limit hardest where a band is perceptually masked (inaudible), least where it's exposed → more loudness at equal *audible* distortion (IRC 3's trick, per-band).
- **Preserve what defines the track** (dominance): don't over-duck a dominant bass groove (our measured EDM failure) — *raise* its threshold when it's carrying the track.
- **Breathe with the music**: thresholds track the macro-envelope → openness without pumping.

Ozone IRC is program-dependent *release* on a fixed threshold; this is program-dependent *threshold*, per band, with spectral lookahead — a different and, we believe, stronger lever.

## 2. What we've already proven (this engine stands on it)

- **2-band peak-controlled limiting = Ozone-parity breathing** (`SPECTRAL_ENGINE_DESIGN.md` S-C). The floor exists; this engine is the ceiling on top of it.
- **Band-separation is the breathing lever** (not per-band release profile). So the adaptive knob to drive is the per-band **threshold**, on a multiband time-domain limiter.
- **The `MultibandLimiter` foundation is built, committed, null-tested** (ADR-0013). This engine adds a *brain* that drives its per-band thresholds dynamically.
- **The measurement harness works** (`mbl_bench` + `mbl_measure.py`): we can prototype the adaptive control the same proven way — Cursor extends the bench, Claude drives it from Python-computed control signals and measures. No Python DSP re-implementation.

## 3. Architecture — eyes → brain → hands

The correct structure (learned the hard way in `SPECTRAL_ENGINE_DESIGN.md` Q4): **STFT is the *analyzer*, not the limiter.** Peak control stays time-domain.

```
input ─┬─────────────────────────────────────────────► [ MultibandLimiter ]  (the HANDS)
       │                                                   per-band time-domain limiters,
       │                                                   per-band THRESHOLD driven live
       └─► [ Detection STFT ]  (the EYES, analysis-only, LOOKAHEAD)
                │  per-critical-band features per frame:
                │   energy · transient-ness · spectral balance · masking estimate
                ▼
           [ Threshold Controller ]  (the BRAIN)
                per-band threshold offset  Δθ_b[t] = f(features, lookahead)
                (+ optional per-band release trim)
                → smoothed → setBandThreshold(b, base_b − Δθ_b) each block
```

- **Eyes** = `StftEngine` in **analysis-only** mode (`setAnalysisOnly(true)`) on a copy of the input. It already exists (SDK `spectral/StftEngine.h`, tracked). Its inherent latency is the **lookahead**: the controller sees each frame's spectrum before that audio reaches the limiter (align the limiter's audio path to the STFT latency).
- **Brain** = a new small module `SpectralThresholdController` (mdsp_dsp): consumes per-frame per-bin magnitudes, groups to K critical bands, computes each band's threshold offset, smooths it, outputs per-band thresholds at block rate.
- **Hands** = the committed `MultibandLimiter`, extended with a **per-block per-band threshold setter** (small additive change) so the brain can drive it live.

## 4. The dynamic-threshold law (the novel core)

Per band `b`, per control step: `threshold_b = base_b − Δθ_b`, where `Δθ_b` (dB of extra limiting allowed) is a bounded sum of content terms. Start heuristic (rules), evolve toward the online distortion-aware controller (roadmap #2). Terms:

1. **Masking allowance** `+m_b` — if band `b` sits below the spread-masking curve of its louder neighbours (Bark spreading function; seed from SDK `SpectralReferenceCurve`), allow more limiting (inaudible). This is the loudness-at-equal-audible-distortion win.
2. **Dominance preserve** `−d_b` — if band `b` is dominant and sustained (carries the track — e.g. the bass groove), *reduce* the allowed limiting (raise threshold) so we don't over-duck it. Directly targets the measured EDM over-duck.
3. **Lookahead anticipation** `a_b` — the STFT sees the section ahead; ramp `Δθ_b` *smoothly toward* the coming demand so the transition into a loud section doesn't pump.
4. **Transient guard** — during a per-band transient, hand the peak to the time-domain limiter's attack (don't chase transients with the threshold; the threshold moves at macro rate).

Bounds + smoothing: `Δθ_b` is clamped and slew-limited (control-rate, e.g. STFT hop) to avoid zipper/pumping — the control signal must move at *macro* rate, faster than a mix section but slower than a beat.

**Escape hatch to the real leapfrog:** replace the heuristic `f(...)` with an **online per-band distortion monitor** (we already compute per-band IMD/THD offline in `tools/analysis/analyze.py`) that pushes each band's threshold down until its *predicted audible* distortion hits a target — the true "distortion-aware brain" (roadmap #2). Heuristic first (measure the win), then the monitor.

## 5. Reuse (compounding SDK assets)

| Need | SDK asset | Status |
|---|---|---|
| Lookahead spectral analysis | `StftEngine` (analysis-only mode) | tracked ✓ |
| Per-band reference / masking seed | `SpectralReferenceCurve` | tracked ✓ |
| Per-bin gain mask (if we ever shape spectrally) | `SpectralGainMask` | tracked ✓ |
| The time-domain limiter (hands) | `MultibandLimiter` + `SingleBandLimiter` | committed (ADR-0013) ✓ |
| New: the brain | `SpectralThresholdController` | to build |
| New: dynamic per-band threshold input | small setter on `MultibandLimiter` | to add (additive) |

## 6. Latency & alignment
- Total latency = `StftEngine.getLatencySamples()` (the lookahead) + `MultibandLimiter` lookahead. The limiter's audio path is delayed to align with the control signal derived from the *same* frame — so the threshold change lands exactly as that audio arrives.
- Report exact integer latency (the recurring HF-tilt lesson, `CUSTOM_FILTERS.md`).
- Acceptable for mastering (avishali confirmed ~43–85 ms STFT latency is fine).

## 7. Hard problems (and the plan for each)
| Risk | Plan |
|---|---|
| **Zipper / pump from threshold moves** | slew-limit + clamp `Δθ_b` at control rate; move at macro (section) rate, not beat rate. |
| **Masking model correctness** | start with a simple Bark spreading function seeded by `SpectralReferenceCurve`; validate on the rig against audibility. |
| **Lookahead alignment off-by-N** | align limiter audio delay to `StftEngine.getLatencySamples()` exactly; verify with an impulse test. |
| **Over-cleverness hurting simple material** | the controller must degrade gracefully to the fixed-threshold 2-band engine when features are flat (bounded `Δθ_b`, null-safe). |
| **CPU** | analysis-only STFT at 4× overlap + K≈24 band grouping (not 2049 bins); control at hop rate. |

## 8. Validation-first (the proven path — measure before C++)
Prototype the *control law* before building the C++ brain, using the harness that just worked:
- **P-A — offline control-curve prototype:** Claude computes per-band threshold curves in Python (numpy STFT features → the §4 law) and feeds them to an extended `mbl_bench` mode (`--band-threshold-curve <file>`) that drives `MultibandLimiter`'s per-band thresholds from a precomputed per-frame table. Render jazz/EDM, measure: does adaptive-threshold beat the best fixed 2-band at matched loudness/TP (300 ms range **and** lower per-band THD)? **Gate: a real, repeatable win over fixed thresholds.**
- **P-B — masking term:** add the spreading-function masking allowance; measure loudness gained at equal *audible* distortion.
- Only then build `SpectralThresholdController` in C++.

The bench extension (Cursor) + control computation (Claude) mirrors the S-C split that produced the §4a confirmation — no Python DSP re-implementation, real production limiter.

## 9. Phasing / slices
1. **S-E — `MultibandLimiter` dynamic per-band threshold setter** + `mbl_bench --band-threshold-curve` (feed a precomputed per-frame per-band threshold table). Additive, null-tested (static curve == fixed threshold).
2. **P-A — offline control-law prototype** (Claude: numpy features → threshold curves → measure). Gate: beats best fixed 2-band.
3. **P-B — masking allowance** (the audibility win).
4. **S-F — `SpectralThresholdController` C++ module** (StftEngine analysis-only → per-band thresholds), wired to `MultibandLimiter`. Null-safe, degrades to fixed.
5. **S-G — online distortion-aware term** (roadmap #2 brain) — the real moat.
6. Latency reporting + Style-seam integration + UI.

## 10. Open questions for avishali
1. **Ship the 2-band parity engine now** (near-term release, the floor) *while* this R&D cooks, or hold release for the adaptive engine (the leapfrog)?
2. Control rate / smoothing character — how fast should thresholds be allowed to move (section-rate vs faster)? A voicing call once P-A renders exist.
3. Heuristic brain first (ship-able sooner) vs hold for the online distortion-monitor brain (bigger leap)?
4. Band count for the adaptive engine — 2-band (proven parity) with adaptive thresholds, or more bands now that the brain can manage the sum-peak trade-off?

---

*The 2-band engine proves we can match Ozone cheaply. This engine is the bid to pass it: a limiter that looks ahead, understands the program spectrally, and spends its limiting budget where the ear can't hear it — parity is the floor, this is the reach.*

---

## ⚠️ P-A RESULT — 2026-07-06 — NEGATIVE for the LF-distortion premise; parity confirmed on LF too
`tools/analysis/mbl_pa.py` — tested the leapfrog's first hypothesis: does a lookahead-adaptive LOW-BAND threshold (duck the low band before bass transients) reduce the clipper's LF distortion while keeping breathing? (baseline validated = MB-2 engine.)
- **NEGATIVE.** Adaptive ducking reduces clipper WORKLOAD (clip GR −1.7 dB) but **LF-THD does not improve** (jazz unchanged, EDM WORSE −10.9→−5.1 dB) and **breathing drops** (EDM 6.02→4.02). Ducking the low band = *limiting* it harder, and the low-band limiter's fast attack distorts the bass just like the clipper — distortion moved, not removed; and the groove flattened. Same fundamental wall: **a bass transient can't be reduced fast (clipper OR limiter) without distorting.**
- **⭐ DECISIVE CONTEXT — our LF distortion = OZONE's, exactly** (LF-THD 45–130 Hz, matched loudness): OZONE jazz −1.9 / edm −11.0; OURS (2-band+clipper) jazz −1.9 / edm −10.9. **We are NOT worse than Ozone on LF — identical.** The LF distortion at −9 LUFS is inherent to loud limiting, not a defect.
- **→ Implication:** the leapfrog's headline justification (fix the LF) is DISPROVEN — you can't beat Ozone's LF distortion without going quieter. **Parity with Ozone is achieved on BOTH breathing AND LF distortion** by the 2-band+clipper engine. avishali's LF complaint is a **CHARACTER** difference (our hard clipper's harsh harmonics vs Ozone's smoother limiting at the same THD magnitude), addressed by **voicing** (Soft clip, lower clipper_db, `dev_mb_attack_ms`, less push), NOT a new engine.
- **This engine (the leapfrog) is now OPTIONAL R&D** (push PAST Ozone: mids/highs openness, loudness at equal *audible* distortion via masking), **not required for parity.** Revisit only if voicing the 2-band+clipper engine leaves a gap avishali cares about.
