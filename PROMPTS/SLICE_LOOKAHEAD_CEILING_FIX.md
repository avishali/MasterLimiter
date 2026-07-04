# SLICE — Fix ceiling-hold: non-zero lookahead defaults + TruePeak final ceiling

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali (Asaf on DEV build)
**Repos:** plugin `MasterLimiter` only. **Param-default + docs change — no DSP logic, no SDK, no new params, no latency change.**
**Companion:** `docs/SIGNAL_FLOW.md` §2.9, §2.14.

> ⚠️ **Retrieval log first.** Re-confirm the cited lines in `Source/parameters/Parameters.cpp` and `Source/PluginProcessor.cpp` before editing (they drift). Output the actual current lines.

---

## Why (measured root cause)

The main limiter wasn't holding the ceiling — it passed **+2 to +9 dB of overs** that only FinalCeiling caught (audible pumping). Root cause, confirmed by offline rig + avishali's DAW testing + the SDK smoother code:

- **Lookahead ships at 0.0 ms** (`dev_lookahead_band_ms`/`dev_lookahead_wide_ms` default `0.0f`). With the **Real** attack mode (the shipping default, `dev_attack_mode`=1) the follower gain *smooths* toward the target (SDK `LimiterEnvelope.cpp` ~476-486) and **lags without lookahead** → peaks escape. (Ramp attack snaps and holds regardless — but distorts more.)
- Measured (100 Hz bass +18 dB, ceiling −1, FinalCeiling off, warm steady-state):

  | Config | out SP | out TP | THD |
  |---|---|---|---|
  | Ramp (snap) | −1.0 | −1.0 | −41 dB (worst distortion) |
  | Real, Wide 0 (**current default**) | +3.5 | +4.6 | −45 dB |
  | **Real, Wide 5 (this fix)** | **−1.0** | **+0.1** | **−58 dB** (cleanest) |

  → Real + lookahead **holds the ceiling AND is 17 dB cleaner than Ramp**. No tradeoff.
- **Latency is unaffected**: the lookahead budget (≤6 ms/stage) is pre-reserved; the wet-path pad shrinks as the active window grows (`padSamples = (osMax − laBand) + (osMax − laWide)`, PluginProcessor.cpp ~1940). Total reported latency stays constant.
- With lookahead holding the ceiling, FinalCeiling drops to a **~0.1 dB true-peak safety net** — its actual purpose. Set its mode to **TruePeak** so that residual inter-sample sliver is caught (currently defaults to SamplePeak).

The docs already claim lookahead "defaults to 5 ms" — so `0.0` was a placeholder/regression, not intent.

---

## Changes (all in `Source/parameters/Parameters.cpp`) — 3 default values

1. **`dev_lookahead_band_ms`** default `0.0f` → **`2.0f`** (~line 281).
2. **`dev_lookahead_wide_ms`** default `0.0f` → **`5.0f`** (~line 288).
3. **`ceiling_mode`** `AudioParameterChoice` default index `0` (SamplePeak) → **`1`** (TruePeak) (~line 151).

Nothing else. These are the measured starting points; Asaf dials the exact band/wide ms by ear in the DEV audition.

**No committed-lookahead wiring needed:** `prepareToPlay` already seeds `committedLookaheadBandMs_`/`WideMs_` from the param value ([PluginProcessor.cpp:333-335](Source/PluginProcessor.cpp:333)) — the new defaults flow through at startup. Just **verify** it (see acceptance).

---

## Docs
- `docs/SIGNAL_FLOW.md` §2.9: correct the lookahead defaults (Band 2 ms, Wide 5 ms — not "both 5 ms"). §2.14: note FinalCeiling now defaults to **TruePeak** (SamplePeak still selectable via `ceiling_mode`).
- `docs/PROGRESS.md` + `PROMPTS/PLAN.md`: log the ceiling-hold fix.

---

## Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
```
**Acceptance (Claude verifies 1–5; avishali/Asaf audition 6):**
1. Build clean, no new warnings; AU validates.
2. **No latency change** vs current HEAD (`getLatencySamples()` identical — lookahead within reserved pad; TruePeak FinalCeiling latency was already reserved). This is the critical guard — report the before/after integer latency.
3. **Committed lookahead picks up the defaults at startup:** on a fresh instance (no preset), `committedLookaheadBandMs_`≈2.0 and `WideMs_`≈5.0 on the first processed block (not 0). State how verified.
4. **Ceiling held (offline rig, warm, Real attack, FinalCeiling OFF):** 100 Hz bass +18 dB, ceiling −1 → output SP ≈ −1 dB (was +3.5); dense chord within ~0.5 dB of ceiling (was +9). Rig scripts + method (warm steady-state, measure the tail) as used in diagnosis.
5. **With FinalCeiling ON (now TruePeak):** true-peak ≤ ceiling on the same material (catches the ~0.1 dB residual). SP + TP.
6. **Audition (Asaf):** with FinalCeiling **off**, push hard in Real mode — overs should be gone and distortion lower vs the old 0 ms; then dial Band/Wide ms by ear (and optionally A/B Real vs Ramp). Confirm FinalCeiling ON is now transparent (not pumping).

> Note: this **intentionally changes the shipped voicing** (the limiter now holds the ceiling) — it is NOT a null test. Confirm the change is *only* the three defaults (diff is param-values + docs).

**Close gate:** update docs above; commit **plugin-only, do not push** (Quell hold); archive `SLICE_LOOKAHEAD_CEILING_FIX_CLOSE.md`.

## Output requirements
1. Retrieval log. 2. Diff (3 default values + docs). 3. Build + auval. 4. Before/after reported latency (must match). 5. Committed-lookahead startup check. 6. Rig evidence (ceiling held, Real attack). 7. `git status --short` + commit hash (no push). 8. Open questions.

## Notes for the architect (not for Cursor)
- Trivial diff, big audible change. The risk is entirely "did latency move" (guard #2) and "did the committed value actually take at startup" (guard #3) — both are why this is a verified slice, not a blind default edit.
- Exact band/wide ms is Asaf's voicing call; 2/5 are the measured floor that holds the ceiling. He can go higher (tighter/duller) or lower (snappier/looser) from there.
