# SLICE P-0 — Offline spectral-limiter prototype (validate §4a before any C++)

**Status:** ready for Cursor · **Architect:** Claude · **Verify+measure:** Claude (rig, 300 ms range) · **Audition:** avishali
**Repo/scope:** OFFLINE ONLY — `tools/analysis/` in `MasterLimiter` (numpy/scipy, the existing `.venv`). **No plugin, no SDK, no C++.** This is a research prototype to decide whether the spectral engine (`docs/SPECTRAL_ENGINE_DESIGN.md`) actually recovers the macro-dynamic breathing gap before we spend C++ slices.
**Why:** the wideband engine is measured-stuck at ~4.07 (combo test); the bet is that per-band ("spectral") limiting recovers breathing the wideband structurally can't. Three quick prototype attempts by the architect FAILED (see design §8 "P-0 status") — this prompt specifies the correct algorithm + the correct test so it's done once, right.

> ⚠️ **Retrieval log first.** Read `tools/analysis/spectral_proto.py` (the architect's scratch — HARNESS parts are validated, the LIMITER CORE is flawed and must be replaced) and `tools/analysis/combo_test.py` (param-driving reference). Output the functions you will reuse vs replace.

---

## ⭐ THE TEST METHODOLOGY — MATCH OZONE EXACTLY (this is the crux; the prior confound)

Ozone IRC 1 was benchmarked with **True-Peak OFF, ceiling −1 dB** → it is a **sample-peak −1 limiter that ALLOWS small inter-sample overs**. Match it:
- **Ceiling −1 dBFS, sample-peak. NO FinalCeiling, NO clipper, NO hard brickwall, NO true-peak mode.**
- The **main (spectral) engine does all the limiting** via attack/release. Tune so the summed output peak lands **1–2 dB above the −1 ceiling** (ISP overs ≈ 0…+1 dBFS) — the same freedom Ozone-SP−1 takes.
- **This is what makes the problem tractable:** the spectral stage does NOT need to hold a hard true peak (that's the "summed peak isn't separable" wall the architect hit by wrongly forcing a brickwall). A 1–2 dB overshoot tolerance removes it entirely.

**Reference targets (already validated — the harness `st_range` reproduces these):** JAZZ `MIX 0003` vs Ozone `test_ozone_11 mix 1` → range **4.68** @ RMS −11.09, TP −0.49; EDM `MIX 0001` vs `mix 2` → range **5.11** @ RMS −10.60, TP −0.48. Wideband floor (our shipping-like) ≈ **2.48 / 3.35**.

---

## Reuse (validated in the scratch) vs Replace (flawed)

**REUSE as-is:** `st_range`, `true_peak_db`, `rms_db`, `report`, `bark_edges`, `split_bands` (IIR band bank), `alpha`, the `GENRES` file paths, and the loudness-match idea. These are correct — `st_range` reproduces the banked Ozone numbers and K=1 reproduces our shipping flatness.
**REPLACE entirely:** the limiter core (`limit`, `waterfill_req`, `breathing_gain` usage) — all three attribution laws the architect tried are wrong (proportional→collapses to wideband; independent-to-ceiling→destroys dynamics; Σe-bound→over-limits ×K).

---

## The algorithm to build (correct this time)

A **per-band lookahead limiter with breathing release**, no brickwall, tuned to a 1–2 dB overshoot:

1. **Band split** — `split_bands(sig, K)` for K ∈ {1, 8, 16, 24}. K=1 MUST reduce to a plain wideband limiter (sanity check: it should reproduce ~2.48/3.35).
2. **Per-band lookahead envelope** — `e_k[n] = maximum_filter1d(|band_k|, 2L+1)`, L ≈ 2 ms. (control-rate decimation OK, decim ≤ 16.)
3. **Per-band gain law** — hold each band toward a **per-band threshold `T`** (NOT the ceiling, and NOT waterfilling): `req_k = min(1, T / e_k)`, then smooth with **instant catch + breathing release** (fast recovery after transients, slow during sustain, program-dependent rate via a sustain tracker; leak ~0.25). One shared `T` across bands.
4. **Sum** the limited bands. **No brickwall.**
5. **Two-knob tuning per config (drive, T):** pick `drive` (input gain) and `T` so that **(a)** output integrated RMS ≈ the Ozone target for that genre AND **(b)** the summed output peak sits **1–2 dB above −1 dBFS** (i.e. true-peak ≈ 0…+1 dBFS). Grid/search both; this is the loudness+overshoot match that makes K configs comparable. (The prior loudness-match diverged because the old core over-limited; with overshoot-tolerant per-band limiting it will converge.)
6. Report per (genre, K): RMS, 300 ms range, TP, mean GR. Overlay the Ozone target row.

**Also build the STFT front-end variant** (avishali: prototype both in parallel) — same idea in the STFT domain: `StftEngine`-style frames → group bins into K Bark bands → per-band gain (steps 3) applied to bins → resynth (COLA, sqrt-Hann, 75% overlap) → same overshoot/loudness match. Compare STFT vs IIR at K=24.

---

## Gate (Claude measures; avishali reviews the numbers)

- **Sanity:** K=1 reproduces the wideband floor (~2.48 jazz / 3.35 edm). If not, the harness match is wrong — stop.
- **Hypothesis:** at matched RMS (≈ Ozone) and matched overshoot (TP ≈ 0…+1 dB), does **300 ms range climb with K toward the Ozone targets (4.68 / 5.11)?** Report the full K sweep for both genres, both front-ends. A clear monotone climb that approaches the targets = §4a validated → proceed to spec C++ S-1. Flat/negative = the mechanism is weaker than theory → re-think before any C++.
- Also report **LF THD** (reuse `analyze.py` helpers) so we confirm breathing isn't bought with low-end distortion.

## Output requirements
1. Retrieval log (reuse-vs-replace list, actual line refs). 2. The new `spectral_proto.py` (or a clean sibling) diff. 3. The full results table (genre × K × front-end: RMS/range/TP/GR/THD) with the Ozone target rows. 4. Runtime. 5. Open questions / any config that wouldn't converge.

---

## REVISION 2 (2026-07-06, after run 1 — architect, VERIFIED) — pivot to STFT vehicle

Run 1 completed enough to verify: **K=1 sanity PASSED** (jazz 2.00 / edm 2.62 ≈ floors), but every **K>1 result is invalid** — the scipy **`butter` bandpass bank does NOT reconstruct** (architect measured sum-vs-input rel-RMS error: K8 **+0.6 dB**, K16 **+2.4 dB**, K24 **+3.0 dB** — error ≥ the signal itself). All K>1 loudness/TP matching chased a mangled signal. §4a still untested. (One tantalising signal through the broken bank: EDM K=8 range 5.83 > Ozone 5.11 — re-confirm on a clean bank only.)

**Corrected plan — do these, in order:**
1. **Make the STFT front-end the PRIMARY vehicle.** The architect verified STFT bin-partition reconstructs to **−240 dB** at K=24 (perfect COLA) and it's **frame-rate** (fixes the 25–35 min/genre runtime — that came from sample-rate Python loops on the IIR bands). Run **STFT across K ∈ {1, 8, 16, 24} on BOTH genres.** This is the §4a test.
2. **DROP the `split_bands` butter bandpass bank entirely** — it is unusable for reconstruction. Do not tune/report the IIR path in this revision.
3. **Runtime:** tune (drive, T) on a ~15–20 s segment only; do NOT do the full-file 40-drive refine (that's the bottleneck). A single full-file render at the chosen (drive, T) for the final numbers is enough.
4. **Gate (unchanged):** at matched RMS (≈ Ozone −11.09/−10.60) and TP ≈ 0…+1 dBFS, does 300 ms range climb with K toward 4.68/5.11? K=1 must still reproduce ~2.48/3.35.
5. **LATER / separate (only if STFT validates §4a):** the 0-latency IIR path — but built as a **complementary Linkwitz-Riley tree** (mirrors SDK `LinkwitzRileyBandSplitter`, sums to unity), NOT butter bandpass.

**Report:** the STFT K-sweep table (both genres) with Ozone target rows + the K=1 sanity + LF-THD, and total runtime.

---

## REVISION 3 (2026-07-06, after STFT run — architect, VERIFIED) — FOUNDATION FIRST, gate before bands

STFT rev2 completed but is **invalid as a limiter test**: TP ran **+3.7…+9.3 dB** (Ozone −0.48 — peaks uncontrolled) and **K=1 range = 9.68** (pumping *inflates* macro-range; a real limiter flattens to ~2.5). **STFT-magnitude is a SHAPER, not a peak limiter** — it can't hold time-domain peaks, so the "no-brickwall / overs 1–2 dB" methodology is unachievable with it. Also, the mandated **shared-T** made K>1 under-limit (GR collapsed 14.7 dB → 1.2 dB) so the K-sweep never compared like-for-like. Both are architect errors in rev1/2 of this prompt.

**We have hit 5 prototype walls. Offline multiband limiting is REAL engineering — build it from a correct baseline, gated, not as a sweep. Do ONLY step 1 first; stop at its gate.**

### Step 1 — correct WIDEBAND time-domain lookahead limiter (do this alone, then STOP)
- Time-domain (NOT STFT). Per sample: `env = forward-max(|x|, lookahead≈1–2 ms)`; gain law holds `env` to the ceiling (−1 dBFS): `req = min(1, ceil/env)`; **instant catch** (lookahead pre-aligns; delay the signal by the lookahead) + **breathing release** (existing `breathing_gain`).
- **Match Ozone-SP:** ceiling −1, sample-peak, **no brickwall/FC/TP-oversampling**. Tune drive + release so output **sample-peak lands ≈ −1…+1 dBFS** (overs ≤ ~2 dB) AND RMS ≈ Ozone (−11.09 / −10.60).
- **GATE (must pass before any bands):** K=1 output **300 ms range ≈ 2.48 (jazz) / 3.35 (edm)** at **TP within 1–2 dB of −1** and RMS ≈ Ozone. If range is ~9 or TP is +5, the limiter is pumping/uncontrolled — fix before proceeding. Report the tuned (drive, release), RMS, range, TP, sample-peak, GR, LF-THD for both genres.
- **Report and STOP here for architect review.** Do NOT build bands yet.

### Step 2 — ONLY after Step 1 gate passes (separate go-ahead)
Reconstruction-correct **Linkwitz-Riley complementary band tree** (time-domain, sums to unity/allpass — NOT butter bandpass, NOT STFT) → per-band lookahead limiter with **per-band thresholds calibrated to a matched total GR** (NOT shared-T) → sum → a light wideband SP limiter to trim residual sum-overs to −1. Then the real test: **at matched loudness+TP, sweep (a) band count and (b) per-band release time-constants (fast HF / slow LF)**; does 300 ms range climb toward 4.68 / 5.11? Per-band *release* is the prime suspect (Cursor open-Q #3), maybe more than band count.

### Demoted
- **STFT** → keep only as a future *spectral shaper* front-end feeding the wideband SP limiter (its natural role), not as the limiter. Out of scope until Step 2 validates.

## Notes for the architect (not for Cursor)
- If K>1 still doesn't climb even with the correct overshoot-tolerant law, the honest conclusion may be that per-band *time-constant* freedom (not attribution) is the real lever — then the next probe is per-band *release* (fast highs / slow lows) at matched GR, still SP/−1/no-FC.
- The masking model (§4b) is explicitly OUT of P-0 — add only if §4a validates.
- Keep FC/TP/clipper OUT of every comparison (match Ozone). FC-5 ms lives in its own slice (`SLICE_FINALCEILING_FAST_RELEASE.md`) as an opt-in safety mode, not part of this test.
