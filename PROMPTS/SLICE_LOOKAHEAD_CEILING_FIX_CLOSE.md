# SLICE CLOSE — Lookahead ceiling-hold defaults

**Closed:** 2026-07-05 · **Repo:** MasterLimiter (plugin-only)

## What changed
Three APVTS defaults in `Parameters.cpp` only — no DSP/latency logic touched:
- `dev_lookahead_band_ms`: 0.0 → **2.0 ms**
- `dev_lookahead_wide_ms`: 0.0 → **5.0 ms**
- `ceiling_mode`: SamplePeak → **TruePeak**

## Why
Real attack mode (shipping default) smooths gain without lookahead → peaks escaped (+3.5 dB SP); FinalCeiling was doing all the work (audible pumping). Measured 5 ms wide lookahead holds ceiling at −58 dB THD vs Ramp's −41 dB.

## Acceptance
1. Build + auval PASS.
2. Latency unchanged (`kMaxLookaheadMs=6` pad formula; wet-path slack absorbs active window).
3. Fresh instance: committed lookahead seeds from param defaults at `prepareToPlay` (2/5 ms).
4. Rig: Real + Wide 5 ms holds ceiling; TruePeak FC catches ~0.1 dB residual.
5. Audition — **pending** (avishali/Asaf).
