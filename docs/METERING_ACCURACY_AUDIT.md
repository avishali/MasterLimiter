# Metering Accuracy Audit — MasterLimiter

**Date:** 2026-07-02 · **Status:** findings accepted; fixes slotted into slices M1–M4.
**Trigger:** GR bars showed no reduction until ~−2 dB; loss of trust in readouts/labels on a mastering device where **every 0.1 dB matters.**
**Method:** three parallel audits of the full chain — DSP tap → atomic → UI sync → ballistics → dB-conversion → pixel mapping.

---

## 0. Headline

**The DSP measurements are accurate to < 0.1 dB. The trust problem is entirely in the UI/ballistics layer**, with one root cause repeated everywhere:

> For a given quantity, the **number**, the **bar body**, and the **max line** are driven by **three different smoothed estimators** with unrelated time constants. They agree only at steady state; during any transient or decay they diverge by ≥ 0.1 dB (often several dB). Several "current" readouts also **hold a stale value** for ~1 s.

Fix philosophy (mastering meter):
1. **One source of truth per quantity** — number and bar show the *same* value.
2. **Live where a number is read; held only where explicitly labeled** (MAX, TP). No silent 1 s holds on a "current" readout.
3. **Smooth in the right domain, once** (RMS in power; no second stage).
4. **Scales spend resolution where mastering lives** (GR 0–3 dB expanded).

---

## 1. What is accurate (leave alone)

- Peak: `gainToDecibels(getMagnitude(...))`, `0 dBFS = 1.0` (`PluginProcessor.cpp:1067,1785`).
- True-peak: `TruePeakDetector::measure()` = max|x| of 4× oversampled stream; correctly ≥ SP (`TruePeakDetector.cpp:41-52`).
- RMS *measurement*: one-pole mean-square, τ = 0.3 s in the **power domain**, then to dB (`PluginProcessor.cpp:456-457,1093,:82`). Correct.
- GR values: `−gainToDecibels(minGain, −120)`, per-band taken **post-Color** (`gLowOut/gHighOut`), floored only at 0 (clamps expansion, not small GR). Accurate < 0.1 dB (`PluginProcessor.cpp:1678-1688`).
- LUFS K-weighting: BS.1770 (shelf 1681.974 Hz, RLB HP 38.135 Hz Q 0.5003), M=0.4 s, S=3.0 s, correct labels (`LoudnessAnalyzer.cpp:44-59,142-143`).
- Input tap is on the latency-aligned dry path (`dryScratch_`).

---

## 2. Findings (prioritized by how far the display lies)

### Tier 1 — actively misleading

| # | Finding | Where | Impact | Fix | Slice |
|---|---|---|---|---|---|
| 1 | **"SP" number is a 0.87 s hold + 0.95 s-τ release, not the live peak** | `MeterGroupComponent.cpp:46-47,56-80,314-329` | reads high up to ~17 dB mid-decay | SP = live peak with a short **~150 ms** honest hold that decays to the true value | **M1 ✅** |
| 2 | **Bar peak ≠ SP number** (bar 60 dB/s; number frozen 26 frames) | `MeterGroupComponent.cpp:120-137` vs `56-80` | 2–20 dB mismatch on transients | drive bar peak + SP number from **one** value | **M1 ✅** |
| 3 | **Bar MAX line ≠ MAX number** (line from smoothed peak; number from raw max) | `MeterRenderStateProvider.cpp:121-122` vs `PluginProcessor.cpp:1072-1075` | dB mismatch on short transients | bar MAX line = `dbToNormForScale(getMax*PeakLDb())` | **M1 ✅** |
| 4 | **RMS number double-smoothed** (300 ms power avg → 2nd 0.87 s hold) | `MeterGroupComponent.cpp:316-317,335-336` | RMS reads high + lags on decays; bar≠number | RMS number = **direct** `rmsDbFromMeanSquare(ms)` | **M1 ✅** |
| 5 | **GR bars invisible < 2 dB** — linear 0–12 dB scale + 0.5px threshold + 50 ms flicker | `GainReductionMeter.cpp:14,32-35,214,22-30` | the reported bug; 0.1–3 dB range unreadable | 0–6 dB **sqrt / low-end-expanded** scale; drop threshold; release ~150–250 ms | M2 |
| 6 | **Integrated LUFS not BS.1770-gated** (no −70 abs / −10 rel) | `LoudnessAnalyzer.cpp:105-107,145-151` | **I** reads systematically low, often > 1 dB | add absolute + relative gating | M3 |

### Tier 2 — real, smaller

| # | Finding | Where | Fix | Slice |
|---|---|---|---|---|
| 7 | **"Clip x.x/y.y" is clipper GR-depth, not output overs**; current-depth decays 50 ms, no hold (`clipHoldMs` ignored by `MeterBallistics`) | `PluginProcessor.cpp:1176-1191`, `ClipBallistics.cpp:11-26`, `MainView.cpp:127-129` | relabel/clarify; add hold to the current-depth number | M4 |
| 8 | **Inconsistent floors −100/−120/−200** → real −110 dBFS prints "-inf" | `PluginProcessor.h:364-384`, `MeterGroupComponent.cpp:16-18,52`, conversions at −120 | unify one `kMeterFloorDb = -120` for conversion, init, reset, "-inf" test | **M1 ✅** |
| 9 | GR numeric readout is **total (band×wide)**; bars are **per-band** — looks inconsistent | `GainReductionMeter.cpp` readout | label it "total" (or add a total bar) | M2 |
| 10 | Dead peak-hold-decay path (`holdEnabled_==true` bypasses the 1500 ms/12 dB·s decay → latches); FullRange uses a **local** anchor table not the shared `normaliseDb` | `MeterRenderStateProvider.cpp:129-137`, `MeterComponent.cpp:23-34,205-206` | reconcile intent (latch is fine, delete/annotate dead code); unify FullRange mapping on shared lib | M4 |

---

## 3. Slice plan

- **M1 — Estimator unification** ✅ (product): SP number = bar peak (`DisplayLevelSmoother`); bar MAX line = `getMax*PeakLDb`; RMS number = direct DSP tap; floor −120. → findings 1–4, 8.
- **M2 — GR mastering scale** (product, `GainReductionMeter`): 0–6 dB sqrt scale, drop 0.5px threshold, release ~150–250 ms, "total" label. → 5, 9. **Do first** (live complaint).
- **M3 — Integrated LUFS gating** (SDK `LoudnessAnalyzer`): BS.1770 absolute + relative gating. → 6.
- **M4 — Clip readout clarity + hold + hygiene** (product): clarify clipper-depth label + hold; delete/annotate dead peak-hold-decay; unify FullRange mapping. → 7, 10.

Most findings are in the **shared `mdsp_ui`** meter helpers or apply family-wide — fixes there benefit every MelechDSP product.

## References
- Shared: `melechdsp-hq/shared/mdsp_ui/src/meters/{MeterBallistics,PeakHoldModel,MeterRenderStateProvider}.cpp`; `melechdsp-hq/shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp`.
- Product: `Source/ui/meters/{MeterGroupComponent,MeterComponent,GainReductionMeter,ClipBallistics}.cpp`, `Source/ui/MainView.cpp`, `Source/PluginProcessor.cpp`.
- `docs/SIGNAL_FLOW.md` §7 (metering taps).
