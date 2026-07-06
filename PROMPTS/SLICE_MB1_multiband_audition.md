# SLICE MB-1 — 2-band parity engine, auditionable in the plugin (DEV-toggled)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (build/rig/diff) · **Audition:** avishali (Ableton — THE point of this slice)
**Repos:** plugin `MasterLimiter` (+ SDK already has the modules, committed). **Additive, DEV-gated — the current shipping engine is untouched when the toggle is off.**
**Why:** we PROVED (rig, `docs/SPECTRAL_ENGINE_DESIGN.md` S-C) that a **2-band peak-controlled `MultibandLimiter`** reaches Ozone-parity macro-breathing. Now avishali needs to *hear* it and voice it. This wires the committed `MultibandLimiter` into the plugin as an A/B-able path with the voicing knobs exposed — NOT the full migration (that comes later if it wins).

> ⚠️ **Retrieval log first.** Read: the plugin's `processBlock`/`processCore` limiter section + where input-gain/ceiling/output are applied + latency reporting (`setLatencySamples`) + how DEV params are declared/cached (`ParameterIDs.h`, `Parameters.cpp`, `PluginProcessor.{h,cpp}`, `DevControlsComponent.*`); and the SDK `dynamics/MultibandLimiter.h` + `SingleBandLimiter.h` API. Output the exact insertion point + the setters you'll call.

---

## What to build — an alternate limiter path behind one DEV toggle

**New DEV param `dev_mb_engine` (bool, default OFF).** When ON, route the signal through a plugin-owned `mdsp_dsp::MultibandLimiter` instead of the existing inline limiter; when OFF, byte-for-byte the current engine (no behaviour change).

**Signal path when ON (keep it simple — HOST RATE is fine for this audition; OS integration is a later slice):**
```
input → inputGain (existing) → MultibandLimiter (2-band, LR) → [optional safety] → output (existing) → meters
```

**Member (PluginProcessor.h):** `mdsp_dsp::MultibandLimiter mbEngine_;` — `prepare()` it in `prepareToPlay` (numChannels, maxBlock, sampleRate, numBands=2, lookaheadSamples from `dev_mb_lookahead_ms`). All allocation in prepare (RT rule §3).

**Per-block config (only when the value changed — rule §6):** set from these DEV params:
- `dev_mb_crossover_hz` — Float 40…3000, default **120**. `mbEngine_.setCrossoverFrequencies(&hz, 1)`.
- `dev_mb_attack_mode` — choice {Ramp, Hybrid, Real}, default **Ramp** (holds peaks; the proven config). Apply to each band via `mbEngine_.band(i).setAttackMode(...)`.
- `dev_mb_release_ms` — Float 5…400, default **150**. `band(i).setReleaseMs(...)` both bands.
- `dev_mb_safety` — bool, default **ON** (the TP-safe "TP mode"). `mbEngine_.setSafetyEnabled(...)`; configure `mbEngine_.safety()` (Ramp, ceiling −1) when on.
- Ceiling: reuse the existing `ceiling_db` param → `band(i).setThresholdLinear(dbToLinear(ceilingDb))` and `setCeilingLinear(1)` (mirror the SingleBandLimiter recipe); safety ceiling same.
- `dev_mb_lookahead_ms` — Float 1…10, default **5** (prepare-time; if changed, defer/re-prepare on the message thread — do NOT reallocate in process).

**Latency:** when the toggle is ON, report `mbEngine_.getLatencySamples()` via `setLatencySamples` (account safety on/off); when OFF, the existing latency. Keep the dry/bypass delay aligned.

**Metering:** output meters must work (they're post-path). GR meter: best-effort — if easy, feed `mbEngine_.getBandGrDb(i)`/safety GR to the existing GR readout; if not clean, leave the GR meter reading zero when the MB engine is on and note it (don't spend risk here — this is an audition slice).

**DEV UI (`DevControlsComponent`):** a labelled group "MB Engine (2-band parity)" — the toggle + the 5 knobs above. So avishali can A/B and voice live.

---

## Non-goals (keep scope tight)
- **No full migration** of the existing engine to the modules (later slice). The current path stays exactly as-is when the toggle is OFF.
- **No oversampling integration** for the MB path (host-rate is fine for auditioning breathing; OS is a follow-up).
- No clipper/FinalCeiling interaction with the MB path (bypass them when MB engine is ON, or leave them after — simplest: MB path replaces the limiter+FC block; clipper OFF by default anyway).
- No new SDK edits (modules are done).

## Build, verify, audition
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cmake --build build --config Release 2>&1 | tail -6
bash scripts/install_user.sh build     # install from build
auval -v aufx MaLm Melc 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–3; avishali auditions 4):**
1. Builds clean, AU validates. Toggle OFF ⇒ output identical to current engine (spot-check a render null vs pre-slice ≤ −120 dB). Latency correct in both states.
2. Toggle ON: no NaNs/denormals, RT-safe (no alloc in process), no clicks when toggling (reuse the existing debounce/duck-swap machinery if the swap clicks — see anti-crackle pattern).
3. (Claude rig) render jazz/EDM through the installed plugin with MB engine ON (crossover 120, Ramp, safety ON, ceiling −1, gain to ~Ozone loudness) → confirm **300 ms range ≈ the bench result (jazz ~4.6 / edm ~5.0)** and TP ≤ ceiling+~1 dB. This confirms the plugin path matches the proven bench DSP.
4. **Audition (avishali):** A/B the MB engine vs the current engine on his mixes; voice crossover / attack / release / safety by ear. Listen for the Ozone-like openness AND the Ramp LF-distortion caveat (try Hybrid/Real attack to trade breathing vs cleanliness — the breathing is from band-separation, so a cleaner attack may keep most of it).

## Output requirements
1. Retrieval log (insertion point + setters). 2. Diffs (params, wiring, DEV UI). 3. Build+auval. 4. Latency ON/OFF. 5. Toggle-OFF null-test dB. 6. Open questions.

## Notes for the architect (not for Cursor)
- This is the REUSE step of the ADR-0013 extraction workflow, done additively for audition. If avishali likes it → proper migration slice (OS integration, metering, retire the inline engine) + it becomes the base engine; the adaptive-threshold brain (`ADAPTIVE_THRESHOLD_ENGINE.md`) layers on top later.
- Voicing caveat to watch: Ramp attack peak-holds but adds LF distortion. The knobs let avishali find the breathing-vs-clean sweet spot; his ears set the default before we lock it.
