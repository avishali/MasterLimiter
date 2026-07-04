# SLICE CLOSE — M/S per-band (opt-in)

**Closed:** 2026-07-04 · **Repo:** MasterLimiter (plugin only; no SDK bump)

## What shipped
- `dev_band_ms` (default off) + `dev_band_ms_link_pct` (0–100%, default 100).
- Band stage M/S path: encode L/R→M/S at crossover input → A2 two-channel detect/envelopes/link → decode at `bandLimitedBuf_` write. Mutually exclusive with Stereo unlink; wideband M/S untouched.
- `bandMsActive_` + GR meter sub-bar labels **M/S** when active.
- DEV **BAND · M/S per-band** toggle + M/S Link slider.

## Files touched
- `Source/PluginProcessor.{h,cpp}`, `Source/parameters/{ParameterIDs.h,Parameters.cpp}`
- `Source/ui/DevControlsComponent.{h,cpp}`, `Source/ui/meters/GainReductionMeter.cpp`
- `docs/SIGNAL_FLOW.md` §2.9–2.12/§6/§7, `docs/PROGRESS.md`, `PROMPTS/PLAN.md`

## Acceptance (pending avishali/Asaf audition)
1. Build + auval PASS; latency unchanged.
2. `dev_band_ms=false` bit-identical all modes (headline null).
3. Stereo mode unaffected at any `dev_band_ms` value.
4. M/S + toggle on @ link 100: M/S-linked band detection (differs from toggle-off by design).
5. M/S + toggle on @ link 0: independent Mid/Side per-band GR; meter shows M/S.
6. TP ≤ ceiling across link 0/50/100.
7. Live-toggle click-safety audition.

## Follow-up
- ADR-0009 §4 per-band M/S rung note (HQ, architect).
- Live `dev_band_ms` toggle crossfade if zipper on audition.
