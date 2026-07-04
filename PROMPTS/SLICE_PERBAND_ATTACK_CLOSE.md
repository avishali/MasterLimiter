# SLICE CLOSE — Per-band attack scale experiment

**Closed:** 2026-07-05 · **Repo:** MasterLimiter (plugin-only)

## What shipped
- Three DEV params: `dev_low/mid/high_band_attack_scale` (0.25…4.0×, default 1.0).
- `configureEnvelope` scales both `dev_attack_ms` and `dev_real_attack_ms` per band; wideband fixed at 1.0×.
- DEV UI group **ATTACK · per-band trim (× base)** with Low/Mid/High Atk × sliders.

## Files touched
- `Source/parameters/ParameterIDs.h`, `Parameters.cpp`
- `Source/PluginProcessor.h`, `PluginProcessor.cpp`
- `Source/ui/DevControlsComponent.h`, `.cpp`
- `docs/SIGNAL_FLOW.md`, `docs/PROGRESS.md`, `PROMPTS/PLAN.md`

## Acceptance
1. Build + auval PASS; latency unchanged.
2. All scales = 1.0 → bit-identical (attackScale param defaults to 1.0f at wideband + when params at default).
3. DEV sliders present, attached, formatted like release-scale sliders.
4. Rig (synthetic bass+snare mix, LA 2/5 ms): per-band Low 4× / High 0.3× improves 100 Hz THD ~0.8 dB vs global fast Ramp (−47.3 vs −46.5 dB); HF crest unchanged (~38 dB). Weak support — needs program-material audition.
5. Program-material audition — **pending** (avishali/Asaf).
