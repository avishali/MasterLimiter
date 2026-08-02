# SLICE LINK-1 — Stereo Link / M/S Link is inert: the channels are ALWAYS fully linked

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** `tools/analysis/mbl_calibrate.py --group L` (Claude) · **Audition:** avishali
**Scope:** plugin only. **No SDK change** expected (the blend is in the plugin's wideband stage).
**Pre-existing** — not caused by CLIP-1/1.1. Found by the calibration suite on 2026-08-02.

## The measurement

Probe: **L = 220 Hz at −4 dBFS** (drives the limiter hard), **R = 3 kHz at −37 dBFS** (a quiet passenger).
Input gain +15 dB, ceiling −1 dB, Transparent engine. R is rendered twice per setting — once with L
**silent**, once with L **loud** — and we ask how far L's gain reduction pulls R down.

| mode | link | R ducked by L | expected |
|---|---:|---:|---|
| Stereo | 100% | **−14.08 dB** | ~−14 (L's GR is 14.05 dB) ✅ |
| Stereo | **0%** | **−14.08 dB** | **~0 dB** ❌ |
| M/S | 100% | **−14.08 dB** | ~−14 ✅ |
| M/S | **0%** | **−14.08 dB** | **~0 dB** ❌ |

50% behaves the same. The link **percentage has no effect at all**: the channels are permanently at the
fully-linked behaviour.

**So the defect is not "linking is broken" — linking works. Unlinking is what never happens.**
Per `docs/SIGNAL_FLOW.md` §2.13, Stereo Link / M/S Link blends the L/R gain toward `min(L,R)`, and only at
**≥99.95%** should it take the single-envelope fast path. At 0% the two envelopes should be independent.

**Audible consequence:** a loud left channel drags down a quiet right channel by 14 dB no matter where the
control is set. On wide or asymmetric material that is a real, hearable defect — and it is a **main-window
shipping control that has silently done nothing**.

> ⚠️ **Retrieval log first.** Read and report: the wideband stage in `processCore` (`SIGNAL_FLOW` §2.13) —
> `envelope_` / `envelope_R_`, `stereoLinkPct_` / `msLinkPct_`, the `>= 99.95%` single-envelope fast path,
> and the per-channel band path (`dev_band_stereo_link_pct`, §2.10) which is a **separate** control — do not
> conflate them. State whether the two-channel envelope path is being constructed at all.

## Likely causes to check (in order)
1. The single-envelope fast path is entered unconditionally (condition inverted, or comparing a 0–1
   normalised value against a 0–100 threshold — 100% arrives as `1.0`, and `1.0 >= 99.95` is false while
   `100.0 >= 99.95` is true; a units mismatch here would pin it one way permanently).
2. `envelope_R_` is never prepared/updated, so the R gain silently falls back to the L/linked gain.
3. The blend coefficient is computed but the result is discarded — both channels written from `min(L,R)`.

## Gate
- [ ] `mbl_calibrate.py --group L` — **all 4 checks PASS**: at 100% R ducks ≈ L's GR; at **0% R ducks < 1 dB**.
- [ ] Intermediate 50% lands between the two (monotonic in the link percentage).
- [ ] Both `Stereo` (`stereo_link`) and `M/S` (`m_s_link`) modes.
- [ ] **No regression:** full `mbl_calibrate.py` still 47+/50 — in particular groups F (peak safety) and
      B (latency) unchanged, and Open still holds −1.00 dB.
- [ ] Mono/correlated material is unaffected (L==R ⇒ min(L,R) == L, so linked and unlinked coincide) —
      confirm with a null test so we know this fix cannot move any existing voicing on mono-ish sources.
- [ ] Build clean, AU + VST3, **both installed**, mtimes reported.

## Non-goals
- Do not touch the **band** stereo link (`dev_band_stereo_link_pct`) or band M/S in this slice — different
  stage, different control, and it is DEV-only. If it has the same defect, report it and we slice it separately.
- No engine/breathing DSP change. No SDK edits.

## Output requirements
1. Retrieval log. 2. Diff. 3. Root cause in one sentence — which of the three causes above (or what else).
4. Build + install mtimes for BOTH formats. 5. Full `mbl_calibrate.py` output. 6. Confirm no SDK edits.

## Note
This control has been in the product since the early slices and has never done anything. Two other shipping
controls turned out inert this same day (Drive was a bit-exact no-op; Ceiling@Clip never clipped at the
ceiling). That is the argument for running `mbl_calibrate.py` as a **build gate**, not on demand.
