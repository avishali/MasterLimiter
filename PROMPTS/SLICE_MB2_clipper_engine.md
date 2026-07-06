# SLICE MB-2 — musical 2-band + clipper tip-catcher (kill 16 kHz), auditionable

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (rig: range/TP vs bench) · **Audition:** avishali (THE point)
**Repo/scope:** plugin `MasterLimiter` (the MB-1 engine path). Additive/DEV-gated; toggle-OFF path untouched. No SDK edits.
**Why:** measured (rig, `docs/SPECTRAL_ENGINE_DESIGN.md` "THE CATCHER ANSWER"): a **musical 2-band split (~120 Hz) → sum → clipper** gives **breathing at/above Ozone AND sample-peak held to −1** (jazz 5.34/edm 6.02 vs Ozone 4.68/5.11). The clipper (zero release) catches transient tips without holding the level down (which a slow wideband safety does → flattens). This is the shippable near-term engine and uses DSP we already have (multiband module + the 8× OS clipper). Two changes:

> ⚠️ **Retrieval log first.** Read: the MB engine branch in `processCore` (where `mbEngine_.process` runs), the existing **oversampled clipper** stage (`clipper_*` params + its 4×/8× OS processing + pre/post position), and the MB DEV params in `Parameters.cpp`. Output the clipper API/insertion point + the MB crossover param default.

---

## Change 1 — KILL the 16 kHz default (avishali: never using it)
- `dev_mb_crossover_hz`: keep range (40–18000) but set **default = 120 Hz** (musical 2-band low/hi split). 
- **Root footgun to document (comment in code + note):** `LinkwitzRileyBandSplitter::setDefaultCrossovers(60,16000)` log-spaces N−1 crossovers between 60 and 16000, so for **N=2 the single crossover lands at 16000** (the top). Any 2-band use MUST pass an explicit musical crossover (we do, via `setCrossoverFrequencies`). Do NOT rely on `setDefaultCrossovers` for 2 bands.

## Change 2 — route the MB sum through the OS clipper (the tip-catcher), not the slow safety
- **Default `dev_mb_safety` → OFF** (the clipper replaces the wideband safety; the slow safety flattens the breathing — measured).
- When the MB engine is ON: after `mbEngine_.process(buffer)` (2-band, safety off), run the summed output through the **existing oversampled clipper** at ceiling −1 (post position), then output. Reuse the shipping OS clipper stage — do not add a new clipper.
- Expose the clipper for the MB path via the existing clipper controls (or a DEV `dev_mb_clip_db`, default −1/0 → clip to the ceiling). Let avishali dial clip amount to trade breathing-vs-clip-distortion.
- Signal path (MB ON): `input_gain → MultibandLimiter(2-band@xover, safety OFF) → OS clipper(−1) → output`.

## Build, verify, audition
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cmake --build build --config Release 2>&1 | tail -6 && bash scripts/install_user.sh build && auval -v aufx MaLm Melc 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–3; avishali auditions 4):**
1. Build clean, AU validates, toggle-OFF byte-identical, latency correct (MB + clipper OS latency).
2. Defaults: MB crossover 120, safety OFF, clipper catching to −1.
3. (Claude rig, `mbl_plugin_verify.py`-style) render jazz/EDM through the plugin (MB ON, 120, ramp, clipper −1), matched to Ozone RMS → confirm **300 ms range ≈ 5.3 (jazz) / 6.0 (edm)** and **sample-peak ≤ −1** (matching the bench `mbl_clip.py` hardclip result). This proves the plugin path reproduces the measured engine.
4. **Audition (avishali):** A/B the 2-band+clipper engine vs current on his mixes; dial crossover (~120), clip amount (breathing↔distortion), attack. Listen for Ozone-like openness with the clipper holding peaks; judge the clipper's transient character.

## Non-goals
- No full migration; no SDK edits; no new clipper (reuse the OS one). Don't touch the toggle-OFF path.
- Ignore the mid/high-crossover over-limit (BUG 2) for now — 120 Hz is close to bench; the parity harness handles BUG 2 separately.

## Output requirements
1. Retrieval log (clipper insertion + MB defaults). 2. Diffs. 3. Build+auval. 4. Latency ON/OFF. 5. Toggle-OFF null dB. 6. Open questions.

## Notes for the architect (not for Cursor)
- This makes the measured "musical multiband + clipper" engine auditionable — the real shippable parity candidate (not the 16 kHz mirage). After avishali voices it, decide: ship as base engine (proper migration + OS) vs push straight to the adaptive/spectral leapfrog to cut the clipper's distortion.
- Still open + separate: bench BUG 0 (file overwrite) and plugin BUG 2 (mid-xover over-limit) — the C++ parity harness covers both.
