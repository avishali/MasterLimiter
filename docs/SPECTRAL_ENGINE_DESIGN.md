# Spectral / Psychoacoustic Limiter — Engine Design

**Status:** 🎯 DESIGN (2026-07-06) — the mission engine. Architect: Claude, from avishali's direction and the measured Ozone investigation.
**Companions:** `docs/LIMITER_TYPES.md` (roadmap #3/#4 + all measured findings), `docs/INTELLIGENT_RELEASE_DESIGN.md` (the release-breathing mechanism, generalized here per-band), `docs/SIGNAL_FLOW.md` (current wideband/multiband engine).
**Frame:** `manifest-mission` — this is a leapfrog bid, **not** a me-too spectral EQ. The novelty is a **per-band, masking-aware, *breathing* limiter**, not "another Soothe."

---

## 1. The mandate (measured, not assumed)

Every prior experiment converged on one cause and one wall:

- **The gap to Ozone = MACRO-DYNAMIC BREATHING** (300 ms loudness range), ~2.6 dB flatter than Ozone IRC 1, **consistent across genres and source-dynamics.** NOT tone (we're ≈/brighter), NOT transients (ours sharper), NOT width (we're wider). (`LIMITER_TYPES.md` 2026-07-05.)
- **The wall = the wideband peak-catcher.** The 2026-07-06 combo test quantified it: macro-breathing tops out at **4.55 only with FinalCeiling OFF = +7.5 dB true-peak overs (unshippable); the TP-safe ceiling is ~4.07.** Every wideband peak-catcher (FinalCeiling, clipper) *reduces* breathing, because **one broadband gain reduction ducks the whole mix to catch one band's peak.** Tuning cannot pass this.
- **The escape is spectral surgery** — Ozone IRC 4/5's openness = "limit only the bands contributing to the peak; when no limiting is necessary the spectrum is unaltered." Duck the offending band, leave the broadband level un-ducked → **breathing and true-peak safety at the same time.** This is the one thing the wideband architecture structurally cannot do, and the whole reason for this engine.

Two secondary, EDM-specific findings fold in cleanly (both natural in the STFT domain):
- Our multiband **over-ducks dominant bass** (Ozone keeps 60–160 Hz +0.7 dB; we're 1–1.75 dB thinner above).
- Our **sub-bass is ~1.6 dB too wide** (Ozone keeps <~120 Hz mono/centered; we spread it → worse mono-compat/punch). → mono the sub.

**Success metric (unchanged, rig-measured on avishali's jazz + EDM at matched loudness, LF THD ≤ Real's):** lift 300 ms range JAZZ 2.1 → ~4.7, EDM 2.5 → ~5.1, while holding true-peak ≤ ceiling and *not* raising low-frequency THD.

---

## 2. The compounding head start — the SDK spectral stack already exists

**All of this is TRACKED on `melechdsp-hq` `master` (94097e3)** — built for the sibling *Quell* (`DynamicResonanceSuppressor`), but the STFT substrate underneath is a **shared SDK asset** (`manifest-mission`: "every SDK win compounds"). This is the difference between a from-scratch FFT arc and wiring existing, RT-proven primitives.

| Primitive | Header | What it gives us |
|---|---|---|
| **`StftEngine`** | `spectral/StftEngine.h` | COLA overlap-add framing (75%/87.5% overlap), selectable analysis window (√Hann/Kaiser/Planck…), √Hann synthesis (bit-transparent to −120 dB on null callback), **exact integer latency**, allocation-free `process()`. **The frame callback is `(re, im, gainLinear, numBins, channel)` — we fill per-bin linear gain; the engine applies + resynthesises.** This is the exact substrate a spectral limiter needs. |
| **`SpectralReferenceCurve`** | `spectral/SpectralReferenceCurve.h` | per-bin neighbourhood reference (`refDb`) + `excessDb` over it, width in octaves — a ready **local-threshold / masking-proxy** estimator. |
| **`SpectralGainMask`** | `spectral/SpectralGainMask.h` | per-bin soft-knee excess→gain with attack/release + max-depth — a ready **per-bin gain computer** to adapt for limiting. |
| **`SpectralTonalityGate`** | `spectral/SpectralTonalityGate.h` | tonal-vs-noise gate per bin — **musical-noise mitigation.** |
| **`SpectralNeighbourCoherence`** | `spectral/SpectralNeighbourCoherence.h` | cross-bin coherence — **smearing/whitening mitigation.** |
| **`LimiterEnvelope`** | `dynamics/LimiterEnvelope.h` | the per-band attack/release engine (ReleaseEngine: AdaptiveSigma/LookaheadFollower/Smart; AttackMode: Ramp/Real/Hybrid). Reusable per critical band. |
| **`FinalCeilingLimiter`** | `dynamics/FinalCeilingLimiter.h` | TP-safe brickwall backstop (fast release per combo test) — catches the residual after spectral limiting. |
| **`LinkwitzRileyBandSplitter`** | `filters/LinkwitzRileyBandSplitter.h` | **0-latency** IIR split, up to 8 bands — the low-latency alternative front-end (see §6). |

> ⚠️ **Coordination:** the shared SDK checkout is also live for Quell/DeNoiser (`project-parallel-products`). Scope SDK edits tight; if the spectral limiter needs changes to `StftEngine`/mask primitives, make them **additive** and coordinate — do not refactor Quell's paths. Prefer a new `dynamics/SpectralLimiter*` gain-computer that *consumes* the existing framing over editing the framing.

---

## 3. Architecture — Spectral as a decomposition front-end

Per the "limiter = decomposition" model (`LIMITER_TYPES.md`), Spectral is a **new front-end on the shared back-end**. Signal path for the Spectral style:

```
input → inputGain
      → [StftEngine] analysis (per channel)
             └─ frame callback: SpectralLimiterCore
                   1. bins → K critical bands (Bark/ERB)
                   2. per-band detector (peak/energy over the band)
                   3. per-band gain computer  ← the psychoacoustic core (§4)
                   4. masking model: allow-more-limiting where masked (§4)
                   5. spectral gain smoothing (freq neighbours) + tonality/coherence guard
                   6. write gainLinear[bin] = band gain (interpolated across the band)
         (engine applies mask + resynthesises, COLA)
      → recombine to broadband
      → sub-bass mono (<~120 Hz, elliptical)               ← EDM width fix
      → FinalCeilingLimiter (TruePeak, FAST ~5 ms release) ← catches ~nothing; residual TP only
      → outputGain → metering/history
```

**Why this breathes where wideband can't:** step 3's gain is **per-band**. A kick peak ducks only the low band; the mids/highs stay at full level, so the *broadband* 300 ms level is not pulled down → the macro envelope keeps its breathing. TP safety is achieved by the per-band ducking + a light final backstop, not by a broadband gain pump. This is the measured mechanism of Ozone's advantage, implemented directly.

---

## 4. The novel core — a *breathing, masking-aware* per-band gain computer

A plain per-band limiter (duck each band independently) already buys IRC4/5-style openness. To **leapfrog** rather than match, two ideas — both grounded in what we measured — sit on top. This is what separates the engine from "another spectral EQ."

> **⭐ Key insight from the P-0 failures (2026-07-06) — attribution must be SURGICAL, not proportional.** The whole engine hinges on *how* the broadband peak-overshoot reduction is split across bands:
> - **Proportional-to-energy attribution collapses to wideband.** If each band is cut proportional to its energy share `w_k = e_k/Σe`, then `g_k = 1 − excess·w_k/e_k = ceil/E` for *every* band — a uniform (wideband) gain. No breathing benefit. (Proven the hard way in P-0 attempt 2's variants.)
> - **The benefit requires NON-proportional (waterfilling) attribution:** remove the overshoot from the **loudest bands first**, leaving bands below the waterline at **full gain**. Only then do non-offending bands keep their level → the broadband macro-envelope breathes while the peak is still held. This *is* Ozone IRC4/5's "when no limiting necessary the spectrum is unaltered."
> - So §4a's "per-band breathing" is really **surgical (waterfilling) attribution + per-band release**. Band-splitting alone does nothing; the attribution law is the mechanism. P-0 must implement waterfilling to test the hypothesis.

### 4a. Per-band program-dependent release (the breathing, generalized per band)
`INTELLIGENT_RELEASE_DESIGN.md` designed an `AdaptiveLookahead` release — LookaheadFollower's peak-catching + AdaptiveSigma's program-dependent *rate* + **leakage** (recover past the window-min). The combo test showed it's inert wideband (FinalCeiling masks it). **In the spectral domain it is exactly right:** each critical band runs its own sustain tracker → fast recovery after that band's transients, slow during that band's sustained content. Dense material breathes because **each band recovers in the gaps of the *other* bands** — a dense mix has no broadband gaps but always has per-band gaps. This is the structural fix the wideband lacks.

### 4b. Masking-aware gain allocation (the leapfrog — IRC 3's real trick)
A per-band **spreading-function masking model** over the Bark scale: a band that is perceptually masked by a louder neighbour is allowed to limit *harder* (its distortion is inaudible) and to *recover sooner* (its ducking is inaudible). This pushes loudness toward the **threshold of audible distortion, not measured distortion** — the thing IRC 3 does and no wideband/multiband can. It also *directly* manufactures breathing: during masked moments the engine recovers gain it would otherwise hold. `SpectralReferenceCurve`'s neighbourhood reference is the seed of the masking estimate; the spreading function is the new DSP.

### 4c. (Later) online distortion-aware controller — roadmap #2 kernel
We already compute per-band IMD/THD offline (`tools/analysis/analyze.py`). An **online** per-band distortion monitor that reallocates gain to hold perceptual distortion under a threshold is the "brain" (roadmap #2), and the natural evolution once 4a/4b measure well. Full ML stems remain a separate long-term track.

**Innovation ladder:** 4a = close the measured gap (openness parity, IRC4/5). 4b = the defensible leapfrog (perceptual gain allocation). 4c = the moat (online distortion-aware brain). Ship 4a first, prove it on the rig, then 4b.

---

## 5. The hard problems (and the SDK primitive that mitigates each)

Spectral limiting is known DSP; the craft is in *not* sounding processed. Each risk has an owner:

| Risk | Mitigation |
|---|---|
| **Musical noise / birdies** (random per-bin gain flutter) | band-grouped gain (not per-bin-independent), **freq-neighbour smoothing** (engine already has `analysisGainSmooth_`), `SpectralNeighbourCoherence` + `SpectralTonalityGate`. |
| **Spectral smearing / transient softening** (STFT time-blur) | short synthesis window (√Hann) + high overlap (8×); optional **detection/synthesis split** (`analysisWindowSize` > `windowSize` for finer detection without extra synthesis latency); keep the wideband TP backstop for the true tip. |
| **Phase coherence** | `StftEngine` synthesis is fixed √Hann WOLA (COLA-transparent); per-bin *gain-only* modification (real, ≥0) preserves phase by construction. |
| **Pre-echo** on sharp transients | window length trade; masking model suppresses pre-echo gain excursions; escalate to shorter window / multi-resolution only if measured. |
| **Latency** (FFT frame) | see §7 — budget it explicitly; offer a low-latency `LinkwitzRileyBandSplitter` front-end (§6) as the 0-latency alternative style. |
| **CPU** | 4× overlap baseline; 8× only in a Quality mode; band-grouped gain computer (K≈24–32 bands, not 2049 bins). |

---

## 6. Front-end choice — STFT vs 0-latency IIR many-band

Two ways to get "many bands," both in the SDK. **Prototype both offline and let the rig decide** — don't assume STFT.

- **STFT (`StftEngine`)** — true constant-Q-ish critical bands, cheap masking model in the bin domain, but frame latency (~window size). Best openness ceiling; the flagship path.
- **IIR many-band (`LinkwitzRileyBandSplitter`, up to 8 bands, 0 latency)** — a natural extension of today's multiband to 6–8 bands with per-band breathing release. Far less openness than true spectral, but **zero latency** and reuses the exact per-band `LimiterEnvelope` path we already ship. A strong low-latency "Spectral-lite" style and a fast way to test the per-band-breathing hypothesis (§4a) *before* committing to STFT.

Recommended: validate **§4a per-band breathing on the 8-band IIR first** (cheapest test of the core hypothesis), then build the true STFT flagship for the openness ceiling + masking model (§4b).

---

## 7. Latency budget

Current engine already reports integer latency via `setLatencySamples()` (base lookahead ×2 + limiter OS + FinalCeiling + crossover OS + clipper OS). Spectral adds the STFT frame latency:
- `StftEngine.getLatencySamples()` = windowSize (e.g. 2048–4096 @48k = ~43–85 ms). This is the dominant new cost and must be surfaced (mastering-acceptable, but declare it).
- The IIR many-band front-end adds **0**.
- Keep the existing FinalCeiling/OS latencies. Report the **exact integer** sum (the recurring HF-tilt lesson: `setUsingIntegerLatency`, report exact host-rate integers — `CUSTOM_FILTERS.md`).
- **Style-dependent latency:** the host must be told latency per active style; changing style re-reports latency (already have the pattern from crossover kernel swaps).

---

## 8b. ⭐ Test methodology — MATCH OZONE'S SETTINGS (avishali, 2026-07-06 correction)

My combo/prototype tests were confounded by testing in **TruePeak mode with FinalCeiling on** — and FC is a macro-flattener (combo test), and large-oversampling FC/TP behaviour muddied it further. **Ozone IRC 1 was benchmarked with True-Peak OFF and a −1 dB ceiling** — i.e. it allows small inter-sample overs; it is a **sample-peak −1 limiter**. So the apples-to-apples test is to match that:

- **Ceiling −1 dB, Sample-Peak (True-Peak OFF), NO FinalCeiling, NO clipper, NO hard brickwall in the comparison.**
- **The MAIN ENGINE does the limiting.** Tune its attack/release so peaks land only **1–2 dB above the −1 ceiling** (i.e. output ISP overs ≈ 0…+1 dBFS) — the same freedom Ozone-SP−1 takes. Oversampling's job is to *catch ISP*, not to enforce a brickwall zero.
- **Consequence for the engine hypothesis:** the spectral stage never needs to hold a *hard* true peak — it only needs the main-engine attack/release to keep overs within ~1–2 dB. This **dissolves the "summed peak not separable" wall** I hit (I was wrongly forcing the spectral stage to brickwall). The §4a question becomes cleanly testable: at matched loudness, SP−1, overs ≤ ~+1 dB, does per-band breathing lift 300 ms range vs wideband?
- The shipping engine still *offers* FC/TP as an opt-in safety mode (combo test: keep FC-5 ms), but it is **OFF for the Ozone comparison and for base operation.**

## 8. Validation-first plan (measure before C++ — mission discipline)

Per `manifest-mission` ("decisions come from data") and the repeated rig lessons, **prototype in Python before writing the C++ slice.** The rig has numpy/scipy/soundfile (no plugin build needed for the algorithm question).

- **P-0 — Offline STFT limiter prototype** (`tools/analysis/spectral_proto.py`): numpy STFT → K Bark bands → per-band gain with §4a breathing release → resynth. Run on avishali's **jazz + EDM** mixes; measure 300 ms range + RMS + TP + LF THD. **Gate:** does per-band breathing lift 300 ms range toward 4.7/5.1 *without* raising LF THD? If yes → the hypothesis holds; build C++. If no → iterate the algorithm in Python (cheap) before touching the SDK.
  - ⚠️ **Dependency:** needs the exact jazz + EDM source files behind the banked 4.7/5.1 numbers (not currently locatable on disk — resolve with avishali). Combo-test infra (`combo_test.py`) is the template for the measurement harness.
- **P-1 — add the masking model (§4b)** in Python; measure the loudness gained at equal *audible* distortion.

Only after P-0 gates green do we spend C++ slices.

### P-0 status — 2026-07-06 — ⚠️ HYPOTHESIS UNTESTED (two prototype attempts, both algorithmically flawed)

Honest record. `tools/analysis/spectral_proto.py`.

- **What's SOUND (validated):** the measurement harness. My `st_range` metric reproduces the banked Ozone numbers under the same code (**jazz 4.68 ≈ 4.7, EDM 5.11 ≈ 5.1**), and a *correct wideband* (K=1) lookahead limiter reproduces the shipping plugin's flatness (**jazz 2.48, EDM 3.35 ≈ banked "ours 2.1/2.5"**). So absolute ranges ARE comparable to the banked targets, and the wideband floor is real. Genre source files now located: `test Project/Samples/Processed/Consolidate/MIX 0003…` (JAZZ, 78 s) + `MIX 0001…` (EDM, 32 s); Ozone renders = `test_ozone_11 mix 1/2`.
- **Attempt 1 (spectral-energy massage + GR-match):** appeared to confirm on one mix (`mix_real_raw`, K1 3.96→K24 4.85) but **broke on the real genre files** (EDM went NEGATIVE). Flaw: the per-band stage massaged spectral *energy*, never controlled the time-domain *peak* → a final brickwall global-scaled the signal and swamped the per-band effect. **INVALID.**
- **Attempt 2 (independent per-band lookahead peak limiter):** worse — multiband range *collapsed* to ~1 and the loudness-match diverged (+46 dB drive). Flaw: limiting **each band independently to the full ceiling then summing** pushes every band to the ceiling → spectral flattening → dynamics destroyed, and the summed peak isn't actually held. **INVALID.**

**⭐ The real crux this surfaced (the actual engine core):** a spectral limiter must **attribute the *broadband* peak-overshoot reduction across bands** — duck only the band(s) causing the overshoot, leave the rest at full gain (Ozone IRC4/5's "when no limiting necessary the spectrum is unaltered"). Neither "limit each band to a fixed threshold" nor "energy massage" does this. The correct algorithm:
> compute broadband required gain `r[n] = min(1, ceil/env[n])`; distribute the reduction `(1−r)` across bands **weighted by each band's instantaneous contribution to the overshoot**, so the summed peak is held to `ceil` while non-offending bands keep full level. Then per-band breathing release (§4a) on that attributed reduction.

- **Attempt 3 (waterfilling attribution — surgical, cut loudest bands to a waterline):** the *right* attribution shape, but still catastrophically over-limited (K=24 needed +73 dB drive and still couldn't reach Ozone loudness; range collapsed). **Root cause = the deep one:** it targeted `Σ_k min(e_k, λ) = ceil`, but **Σ of per-band peak-envelopes hugely over-estimates the true summed peak** (K mostly-decorrelated bands don't peak in phase) → over-limits by ~×K. **INVALID.**

**⭐⭐ The core difficulty, now understood (why spectral limiting is genuinely hard, not a formality):** **the summed-signal peak is NOT separable from per-band envelopes.** Every cheap per-band attribution fails a different way — *proportional* → collapses to wideband; *independent-to-ceiling* → destroys dynamics; *Σe conservative bound* → over-limits ×K. There is no closed-form per-band gain that holds the true broadband peak, because the peak is phase-dependent.

**→ The correct architecture (separate the two concerns) — reframes §3 and what P-0 must measure:**
1. **The hard true-peak guarantee belongs to the final brickwall** (`FinalCeilingLimiter`, fast release — cheap, exact, phase-correct), NOT the spectral stage.
2. **The spectral stage does *perceptual* reduction only** — pull bands that stick out above a moving spectral reference/masking curve DOWN, so the brickwall has *less work to do*. Its target is a **reference curve**, not the ceiling.
3. **The metric becomes:** at matched output loudness/TP, does the spectral pre-conditioning let the **brickwall's gain-reduction (and thus its macro-flattening) drop** → higher 300 ms range? That's the breathing, measured directly. (Ties to the combo test: FinalCeiling is the flattener; the spectral stage's job is to *feed it a gentler signal* so it flattens less.)

This is exactly what the SDK's `SpectralReferenceCurve` (neighbourhood reference) + `SpectralGainMask` are built for — the spectral stage is a **dynamic spectral shaper feeding a wideband brickwall**, not a per-band brickwall itself. **P-0 v3 must be rebuilt on this model** (spectral shaper → measure brickwall-GR reduction + range), NOT "hold each band to ceiling." **Do not spend C++ slices until this measures the jazz/EDM range climbing toward 4.7/5.1 at Ozone loudness/TP.** ⚠️ This is a real design/prototyping task (the hard core of the engine), not a quick script — three fast attempts proved that.

### S-C first result — 2026-07-06 — ✅ PRELIMINARY POSITIVE (real production DSP, not Python)
After the SDK module arc (ADR-0013: `SingleBandLimiter` + `MultibandLimiter`, both committed & null-tested), we finally ran §4a on the REAL DSP via the `mbl_bench` CLI + `tools/analysis/mbl_measure.py` (Claude orchestrates drive-matching + measures). **Per-band limiting recovers macro-breathing vs wideband, at matched loudness:**

| | K=1 wideband | N=2 | N=8 | Ozone |
|---|---|---|---|---|
| JAZZ 300 ms range | 3.84 | 5.68 | **5.92** | 4.68 |
| EDM 300 ms range | 5.66 | 6.47 | **6.41** | 5.11 |

**Findings:** (1) multiband lifts 300 ms range **+1.8–2.1 dB (jazz) / +0.8 dB (edm)** over wideband, **meeting/exceeding Ozone**; (2) **most of the gain is at N=2–3** (strong diminishing returns after) — even a few bands captures it; (3) **per-band *release profile* (fast-HF/slow-LF) does NOT beat uniform release → the lever is independent per-band *limiting* (spectral separation of GR), not per-band release time-constants.** This refines §4a.

⚠️ PRELIMINARY (superseded by the peak-matched run below): hybrid-default attack passed transients (TP hot).

### S-C peak-matched — 2026-07-06 — ✅ §4a CONFIRMED (production DSP, RMS- AND TP-matched, both genres)
After S-C.1 added Ramp attack (`--attack-mode ramp` holds sample-peak to −1). Re-ran `mbl_measure.py` peak-controlled. The valid (peak-matched, TP≈Ozone) comparison:

| | K=1 wideband (TP) | **N=2 (TP)** | Ozone (TP) |
|---|---|---|---|
| JAZZ 300 ms range | 2.12 (0.52) | **4.76 (0.18)** | 4.68 (−0.49) |
| EDM 300 ms range | 2.96 (−0.08) | **5.35 (−0.05)** | 5.11 (−0.48) |

- **Sanity anchor PASSES:** peak-controlled K=1 = 2.12 / 2.96 ≈ the banked shipping floor (2.1 / 2.5).
- **N=2 closes the entire macro-breathing gap** at matched RMS **and** matched TP — 2.1→4.8 (jazz), 3.0→5.4 (edm), landing at/above Ozone. Consistent across both genres. **This is the confirmation** (rigorous: peak+loudness matched, sanity-anchored, real DSP — not a proxy).
- **Just 2 bands does it.** N=4/N=8 range is also high BUT NOT peak-matched (safety off → summed bands overshoot, TP +5…+10; more bands → more constructive-sum overshoot). To use N≥4 peak-controlled you must control the SUM peak (append the safety `SingleBandLimiter`, or a final limiter) — a separate step.
- **Mechanism refined:** we close the gap via **band-separation**, not release intelligence — per-band *release profile* (fast-HF/slow-LF) still doesn't beat uniform. Note: Ozone **IRC1 is single-band** and gets its breathing from program-dependent *release*; we reach the same macro-range with a **2-band split + simple release** — a different path to the same result.
- **Caveat (voicing, not measurement):** Ramp attack catches peaks but adds LF distortion (memory: THD −41 vs Real −51). It's the right tool to *measure* breathing peak-matched; the shipped engine needs a cleaner transient catcher. Breathing (macro-range) is unaffected by this.

**⚠️ CORRECTION 2026-07-06 (musical crossovers, `mbl_musical.py`) — "cheap 2-band parity" was a MIRAGE.** The clean peak-controlled "N=2 ≈ Ozone + TP-safe" used the `LinkwitzRileyBandSplitter` **N=2 default crossover = 16 kHz** (an artifact of `setDefaultCrossovers(60,16000)`'s log-spacing dropping the single crossover at the top). 16 kHz = "whole mix as one band + air as the other" → the air band is trivially small → the sum barely overshoots → the safety sits idle → breathing survives. **Non-musical; we will never use it.** At MUSICAL crossovers (2-band 120/150, 3-band 100/2500), measured jazz/edm:
- **safety OFF: breathes hard** (range 5.7–6.4 ≫ wideband 2.1/2.9, > Ozone) **but overshoots TP +4–5 dB** (two real bands sum past ceiling — not shippable).
- **safety ON: peak-safe (TP ~0) but FLAT** (2.2–3.2 ≈ wideband floor) — the wideband safety, catching the summed overshoot, re-flattens the macro-envelope.
- **3-band ≈ 2-band** (no extra breathing from the 3rd band).
**→ MECHANISM proven (band-splitting breathes); the lever for MUSICAL+TP-safe = a smarter peak catcher.** **FAST-catcher probe (`mbl_fastsafety.py`, 2-band@120 + wideband safety, release sweep):** a FAST safety release recovers most of the breathing WHILE holding TP — jazz 150ms→2.19 vs **5ms→4.02 (TP −0.39, ≈Ozone 4.68)**; edm 150ms→3.30 vs **5ms→3.83 (TP −0.29, partway to 5.11)**. So **musical multiband + a fast transient catcher (fast-release limiter or clipper on the sum tips) = breathing AND TP-safe, partially** (jazz near Ozone, edm partway; faster/clipper should close more). This is the shippable direction, all real DSP. **KILL the 16 kHz default** (2-band default → ~120–150 Hz). Next: push the catcher (clipper/2ms) + the spectral-attribution leapfrog for the rest.

**Implication (superseded above):** the cheap win is a **2-band, 0-latency (`LinkwitzRileyBandSplitter`) peak-controlled multiband limiter** — closes the gap at 16 kHz only (mirage).

### N-band + sum-safety probe — 2026-07-06 — ⭐ 2 BANDS IS THE SWEET SPOT (N≥4+safety collapses)
`tools/analysis/mbl_safety.py` (two-pass: multiband safety-off → wideband safety limiter on the sum; peak-controlled, drive-matched final RMS to Ozone):

| | N=2+safety | N=4+safety | N=8+safety | Ozone |
|---|---|---|---|---|
| JAZZ range (TP) | **4.61 (−0.35)** | 2.22 (−0.52) | 1.75 (0.84) | 4.68 (−0.49) |
| EDM range (TP) | **5.03 (−0.52)** | 1.02 (0.40) | 2.16 (0.15) | 5.11 (−0.48) |

**N≥4 + safety COLLAPSES the breathing** (range → ~1–2): more bands → bigger summed overshoot → the safety limiter works harder → its heavy wideband GR **re-flattens** the macro-range (the original "wideband safety masks per-band" effect). **N=2 + safety keeps ≈ Ozone range AND is fully TP-safe** (sum barely overshoots → safety does almost nothing → breathing survives).

**CONCLUSION — the engine is simple and cheap:**
- **2-band, 0-latency, peak-controlled multiband limiter** = Ozone-parity macro-breathing.
- **Base mode** = 2-band, no safety (TP ~+0.2, ISP overs like Ozone SP−1).
- **"TP mode"** = 2-band + light final safety limiter (TP-safe, negligible breathing cost).
- **Do NOT go many-band for loudness/breathing** — it needs sum-peak control, which re-flattens. Parity does not need STFT/many-band.
- **Voicing gap:** Ramp attack (used to peak-match) adds LF distortion → the shipped 2-band engine still needs a cleaner transient catcher (Idea #1 dual/fast-catcher, or a better per-band attack). Breathing is solved; clean peak-catching is the remaining voicing work.

### Crossover robustness + correction — 2026-07-06 (`mbl_xover.py`)
The N=2 breathing recovery is **robust to crossover frequency** — range stays high (jazz 4.9–5.8 / edm 5.6–6.5, all ≫ K=1 2.1/3.0) for crossovers 120 Hz–3 kHz. NOT a crossover artifact. **Correction to the earlier "N=2 peak-matched 4.76":** that used the bench *default* crossover (behaves like ~16 kHz — near-wideband + an air split), which is the one config that's **peak-controlled** (TP ~0.1). Sensible low/mid crossovers (120–3 kHz) breathe *more* but **overshoot the summed peak** (TP +4–5). So there is a real **crossover ⇄ sum-peak trade-off**: low crossover = more breathing but needs sum-peak control (which re-flattens, per the safety probe). **Engine-design open item:** pick the crossover (and any light sum-peak control) that maximises peak-controlled breathing — likely a mid/high crossover, or a smarter attribution. (Mechanism nuance: even the ~16 kHz split breathes because separating the highs stops HF transients from yanking the main-band envelope down — a real per-band effect.)

### Phase-linearity probe — 2026-07-06 (`mbl_phase.py`, S-D) — LR wins on cost
LR IIR split vs `LinearPhaseCrossover`, 2-band, matched loudness:
| | range | crest10 | latency |
|---|---|---|---|
| LR (IIR) | 5.4–6.4 | 7.5 / 7.7 | **960 (0-latency xover)** |
| LinearPhase | 5.3–7.0 | 7.4 / 7.7 | 4046 (+79 ms FIR) |
**No measurable objective benefit** (breathing ±0.5 inconsistent; crest ≈ identical → no transient softening) and **+79 ms latency.** → **LR 0-latency is the default** unless avishali's low-end-coherence audition (files in `Music/ML_audition/phase_*`) justifies the latency. Objective data does not support linear-phase for this 2-band limiter.

**Beyond parity (the leapfrog, roadmap #2/#3, avishali's direction 2026-07-06):** the **"spectral dynamic threshold lookahead" engine** — STFT analyses the incoming program (lookahead) and sets per-band thresholds adaptively (the content-aware "brain"; subsumes "STFT + spectral-intelligence"). *Differentiation above parity*, not required to match Ozone. Design doc next.

---

## 9. C++ slice breakdown (post-prototype, Trinity flow: Claude specs → Cursor builds → avishali auditions)

1. **S-1 — `SpectralLimiterCore` skeleton** consuming `StftEngine` frame callback: bins→K bands, per-band detector, flat per-band gain (no breathing yet), gainLinear write-back. Null-safe, additive. Rig: confirm bit-transparent when threshold high; correct per-band ducking when driven.
2. **S-2 — per-band breathing release (§4a):** port the `AdaptiveLookahead` rate+leak per band (reuse `LimiterEnvelope` or a compact per-band gain-state). Rig: 300 ms range lift.
3. **S-3 — musical-noise guard:** freq-smoothing + `SpectralTonalityGate`/`NeighbourCoherence`. Rig: null-material transparency + no birdies.
4. **S-4 — masking model (§4b):** Bark spreading function; perceptual gain allocation. Rig: loudness at equal audible distortion.
5. **S-5 — Style seam + UI:** wire "Spectral" into the decomposition-style selector (see §10); style-dependent latency reporting; DEV knobs (band count, overlap, masking depth, per-band fast/slow/leak).
6. **S-6 — sub-bass mono + bass preservation** (EDM fix): elliptical mono <~120 Hz (trivial in STFT: null side channel in low bins) + verify low-band no longer over-ducks.
7. **S-7 (later) — online distortion-aware controller (§4c).**

Each slice is gated by the rig on the two mixes; nothing ships until it wins the A/B (`project-masterlimiter-slices`).

---

## 10. The Style-selector seam (design now, minimally)

`LIMITER_TYPES.md` deferred the seam until voicing locked; the spectral engine forces a minimal version. Proposal:
- A **`DecompositionStyle` enum**: `Multiband` (today), `Spectral`, later `Dual`. The selector chooses the **front-end**; everything downstream (recombine → sub-mono → FinalCeiling → metering) is shared.
- Each style owns its front-end object + its latency; switching style triggers a latency re-report + a click-free swap (reuse the crossover duck-and-swap machinery — `anticrackle-arc`).
- Per-style params scoped under the style; shared back-end params (ceiling, output, FinalCeiling) global.
- **Keep it additive:** `Multiband` path bit-identical; `Spectral` is opt-in until it wins.

---

## 11. Decisions (avishali, 2026-07-06)

1. **Latency:** ~43–85 ms STFT latency is **fine** for the flagship (mastering context). STFT may use a full window for best frequency resolution / openness — no need to lead low-latency.
2. **Front-end order:** **prototype BOTH in parallel** (STFT flagship + 0-latency 8-band IIR) in Python and let the rig pick. (P-0 builds both.)
3. **v1 scope:** **ship §4a (per-band breathing) first** — prove the measured gap is closed, ship, then add the masking model (§4b, the leapfrog) as a second release. §4b/§4c are post-v1.
4. **Open dependency:** the exact **jazz + EDM source files** behind the banked 4.7/5.1 numbers — needed for P-0's real validation (smoke-test runs on `mix_real_raw.wav` meanwhile).

---

*This engine is the mission bet: the measured Ozone gap (macro-breathing under a TP ceiling) is structurally unsolvable wideband and structurally solvable per-band. The SDK already carries the STFT substrate. The novelty we own is the breathing, masking-aware per-band gain computer — not the FFT.*
