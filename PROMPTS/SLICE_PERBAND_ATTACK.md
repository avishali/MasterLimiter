# SLICE — Per-band attack (fast highs / slow lows) — EXPERIMENT

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (rig) · **Audition/decide:** avishali → Asaf → testers
**Repos:** plugin `MasterLimiter` ONLY. No SDK change (`LimiterEnvelope` already holds attack state per instance). Mirrors the existing per-band **release-scale** pattern exactly.
**Goal:** test the measured escape from the wideband attack tradeoff — *keep the low band's gain slow (clean bass) while the high band catches transients fast (the harsh HF crack)*. This decouples transient-catching from LF distortion, which no single wideband attack curve can (see `docs/LIMITER_TYPES.md` 2026-07-04 section: "any wideband attack fast enough to catch a transient distorts the low end").

> ⚠️ **Retrieval log first.** Re-confirm the `configureEnvelope` lambda + its call sites, the per-band release-scale params, and the DEV attack UI in the current tree (lines drift). Output the log.

---

## Why (measured)

Distortion when catching transients comes from **fast gain on the LOW band** (a 1–6 ms gain move is within a 50 Hz cycle). The multiband already splits frequency — but **attack is global**: `configureEnvelope` applies the *same* `attackMode` / `attackOverrideMs` / `realAttackMs` to all 8 envelopes ([PluginProcessor.cpp:1370-1388](Source/PluginProcessor.cpp:1370)), only the *release* scale differs per band. Give each band its own **attack scale** and you can run the low band slow (clean bass) and the high band fast (catch snares/cymbals — fast HF gain is far less audible). Kicks (LF transients) stay in the slow low band → pass to FinalCeiling, which is correct (LF transients can't be caught fast without distorting anyway).

**Null-safe & cheap:** each `LimiterEnvelope` already has its own `attackSamples_`/`realAttackMs_`; we just pass per-band values. All scales default 1.0 → **bit-identical to today**.

---

## Allowed files
```
Source/parameters/ParameterIDs.h  Source/parameters/Parameters.cpp
Source/PluginProcessor.h / PluginProcessor.cpp
Source/ui/DevControlsComponent.h / .cpp
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_PERBAND_ATTACK_CLOSE.md   (new, at close)
```
**Non-goals / STOP:** No SDK edit. No change to release scales, lookahead, or the wideband stage. No new frozen IDs (DEV only). Wideband envelope keeps attack scale = 1.0 (bands only). Attack *mode* (Ramp/Real/Hybrid) stays global — this scales attack *time* per band, mode-agnostically.

---

## Params (3 new DEV params — mirror `dev_low/mid/high_band_release_scale`)
`ParameterIDs.h` + `Parameters.cpp`, cache raw pointers + `jassert` exactly like the release scales:
1. **`dev_low_band_attack_scale`** — Float `0.25 … 4.0` step 0.01, default **1.0**, display "DEV Low Attack Scale".
2. **`dev_mid_band_attack_scale`** — same, default 1.0.
3. **`dev_high_band_attack_scale`** — same, default 1.0.

(Scale multiplies the effective attack *time*: `> 1` = slower, `< 1` = faster. Range gives ~4× either way from the base attack.)

---

## DSP — thread a per-band attack scale through `configureEnvelope`
1. Cache the 3 raw pointers (`devLowBandAttackScale_` etc.), like `devLowBandReleaseScale_`.
2. Add an `attackScale` parameter to the `configureEnvelope` lambda ([:1370](Source/PluginProcessor.cpp:1370)). Inside, scale **both** attack knobs so it works in every mode:
   ```cpp
   envelope.setAttackOverrideMs (devAttackMs   * attackScale);   // Ramp/Hybrid pre-ramp length
   envelope.setRealAttackMs      (realAttackMs * attackScale);   // Real/Hybrid RC time
   ```
   (Currently these pass the un-scaled globals — the `attackScale` defaults to 1.0 for callers that don't set it.)
3. At the call sites ([:1390-1405](Source/PluginProcessor.cpp:1390)): pass the per-band scale to each band envelope (`envelopeLow_`/`envelopeLowR_` → low scale; Mid → mid; High → high) and **`1.0f` to the wideband** `envelope_`/`envelope_R_`. Read each scale once per block near the release scales (`std::memory_order_relaxed`), clamp defensively.

> RT-safe (scalar reads, no alloc). Latency unchanged (attack ≤ lookahead, already padded). No new envelopes/buffers.

---

## DEV UI (`DevControlsComponent`)
Add 3 sliders **Low Atk ×** / **Mid Atk ×** / **High Atk ×**, mirroring the per-band release-scale sliders (same setup, attachment, formatting). Place in the ATTACK group (or a new "ATTACK · per-band ×" row group under it). Tooltip: *"Per-band attack-time scale. >1 slower (cleaner, esp. bass), <1 faster (catches transients). Try Low ~3–4, High ~0.3–0.5 to catch HF transients while keeping bass clean."*

---

## Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
```
**Acceptance (Claude verifies 1–3 + measures 4 on the rig; avishali auditions 5):**
1. Build clean, AU validates, **no latency change**.
2. **Null:** all 3 scales = 1.0 → bit-identical to HEAD in every mode (offline null or bench). Headline gate.
3. DEV: 3 sliders present, attached, formatted like the release-scale sliders.
4. **(Claude runs the rig)** the hypothesis: Ramp or Hybrid mode, **Low scale ~4 / High scale ~0.3**, on a bass+snare mix — does **50 Hz bass THD stay near Real's** (−50+) while the **HF-transient crest drops** (caught)? Compare to global attack (all 1.0). Report the table.
5. **Audition (avishali first):** does fast-highs/slow-lows tame the transient harshness without dulling/distorting the low end?

**Close gate:** update `docs/SIGNAL_FLOW.md` (§per-band attack) + `docs/PROGRESS.md` + `PROMPTS/PLAN.md`; commit **plugin-only, do not push** (Quell hold); archive CLOSE prompt.

## Output requirements
1. Retrieval log. 2. Param diff. 3. DSP diff (`configureEnvelope` attackScale + call sites). 4. UI diff. 5. Build + auval. 6. Null evidence. 7. Latency before/after. 8. `git status --short` + commit hash (no push). 9. Open questions.

## Notes for the architect (not for Cursor)
- This is the measured next lever after Hybrid: Hybrid gave a Ramp↔Real morph but couldn't escape the catch-vs-LF-distortion wall; per-band attack escapes it by *frequency*, using the multiband we already have.
- Expect it to help **HF transient harshness** (snares/cymbals), NOT LF transients (kicks) — those stay in the slow low band and rely on FinalCeiling/clipper. That's the physical limit, not a bug.
- Interacts cleanly with attack mode: in Ramp a big low-band scale = long/gentle pre-ramp (clean); in Hybrid it scales both pre-ramp + RC. Asaf can combine Hybrid + per-band attack.
