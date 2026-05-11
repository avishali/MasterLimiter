# Cursor Task — Slice 4.2: Switch OS to FIR linear-phase halfband

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Tight scope: two `.cpp` files.** Replace one constructor argument in
> each — `filterHalfBandPolyphaseIIR` → `filterHalfBandFIREquiripple`.
> No header changes, no algorithm changes, no API changes.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- ONE focused diff. Stay strictly within §3 file scope.
- RT-safety: change is in the constructor / `prepare()` path, not on the
  audio thread. `oversampler_.initProcessing()` already runs in `prepare`.
- Minimal diff.

────────────────────────────────────────
1. WHY (read once)
────────────────────────────────────────

Slice 4.1 (soft-knee saturator) improved IMD (2.21% → 0.30%) but **pink
null, noise residual, and aliasing stayed at their failed values**. Root
cause is not the saturator: it's the OS chain's filter choice.

`filterHalfBandPolyphaseIIR` is **non-linear phase**. The OS round-trip
(up → process → down) frequency-shifts every component by a different
amount of group delay. Output audio is *perceptually identical* to input
when the saturator doesn't engage, but **cannot null against the dry
reference** because phase is scrambled.

Evidence (Slice 4.1 bench):

| Metric | Value | Implication |
|---|---:|---|
| Pink null K-w | −13.3 LUFS | Phase shift on broadband → big null residual |
| Sweep null | +4 LUFS (residual louder than signal!) | Phase shift on chirp |
| IMD pct | 0.30 % ✓ (passes 1 %) | Harmonic measurement is phase-invariant |
| IMD null | −14 LUFS ✗ | Same phase shift |
| Aliasing ≥ 20 kHz | −2 to −13 dB | IIR halfband passband ripple near Nyquist |
| Dirac null | −26 LUFS | Impulse smearing from filter ringing |

Linear-phase halfband (FIR equiripple) fixes all of this. Audio still gets
delayed, but every frequency by the same amount → cleanly nullable against
a time-aligned dry. Stopband attenuation typically > 100 dB → aliasing
drops far below −90 dB threshold. Same APIs, same algorithm, same
parameters, just a different filter inside `juce::dsp::Oversampling`.

Latency cost: FIR halfband adds ~30 samples vs ~3 for IIR. Total TP-mode
plugin latency goes from ~243 to ~270 samples. Still well inside the
20–120 ms mastering-grade budget from ADR-0004.

────────────────────────────────────────
2. TRINITY (very light)
────────────────────────────────────────

Re-read before editing (no MCP needed):

- `shared/mdsp_dsp/src/dynamics/TruePeakDetector.cpp` — find the
  `juce::dsp::Oversampling` constructor or `addOversamplingStage` call(s)
- `shared/mdsp_dsp/src/dynamics/IspTrimStage.cpp` — same
- JUCE: confirm `juce::dsp::Oversampling<float>::FilterType` enum value
  `filterHalfBandFIREquiripple` exists in JUCE 8 (it does; same enum as
  `filterHalfBandPolyphaseIIR`)

Brief retrieval note in your reply.

────────────────────────────────────────
3. SCOPE — files you may MODIFY
────────────────────────────────────────

- `shared/mdsp_dsp/src/dynamics/TruePeakDetector.cpp`
- `shared/mdsp_dsp/src/dynamics/IspTrimStage.cpp`

**Do not modify:** any header, `Parameters.cpp`, `PluginProcessor.{h,cpp}`,
`host.py`, `bench.py`, `criteria.py`, any other file.

────────────────────────────────────────
4. THE CHANGE
────────────────────────────────────────

In each `.cpp`, locate where the `juce::dsp::Oversampling<float>` is
constructed or where `addOversamplingStage` is called. Replace the
filter-type argument:

```diff
- juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
+ juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple
```

If the oversampler is constructed via the multi-stage convenience ctor:

```cpp
juce::dsp::Oversampling<float> oversampler_ {
    /* numChannels   */ 2,
    /* factor (log2) */ 2,                // 4× = 2 cascaded 2× stages
    /* FilterType    */ juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
    /* isMaxQuality  */ true,
    /* useIntegerLatency */ true
};
```

Keep `isMaxQuality = true` (we want clean stopband) and
`useIntegerLatency = true` (clean PDC reporting to hosts).

If the code uses `addOversamplingStage` per-stage, update **every** stage
in the chain to use `filterHalfBandFIREquiripple`.

**Nothing else changes.** Same `initProcessing`, `reset`,
`processSamplesUp`, `processSamplesDown` calls. Same algorithm body in
`IspTrimStage::process` and `TruePeakDetector::measure`.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```

Zero new warnings.

### Slice 4 bench

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
python bench.py --driver master_limiter --slice 4 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE04_2_FIR_HALFBAND
```

### Slice 3 regression

```bash
python bench.py --driver master_limiter --slice 3 --quick \
  --plugin-path "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3" \
  --output-dir runs/SLICE04_2_REGRESSION_S3
```

────────────────────────────────────────
6. EXPECTED OUTCOME
────────────────────────────────────────

| Metric | Slice 4.1 (IIR) | Predicted (FIR) | Threshold |
|---|---:|---:|---:|
| Pink null K-w | −13.3 LUFS | **−25 to −31 LUFS** | ≤ −25 |
| Pink noise residual % | 40.9 % | **10–14 %** | ≤ 12 |
| IMD pct | 0.30 % | **≤ 0.20 %** | ≤ 1.0 |
| Aliasing ≥ 20 kHz | −2 to −13 dB | **≤ −90 dB** | ≤ −90 |
| TP overs aggregate | 280 (pink) | **0 or small** | 0 |
| Sample-peak overs | small | small | ≤ 200 |
| Latency reported | 243 | **~270** (JUCE reports actual) | match expected |
| Latency alignment lag | 1 | 0 or 1 | ≤ 1 |

**Three outcome cases:**

1. **All thresholds met** → SLICE 4.2 PASS. Slice 4 closes.
2. **All met except TP overs > 0 (small, say < 50 aggregate)** → STOP and
   report numbers. Bench's scipy resampler vs JUCE FIR halfband may differ
   slightly. Architect will decide between tightening saturator knee
   (e.g. `0.93 × ceiling`) or adjusting bench TP measurement.
3. **Pink null or aliasing still fails badly** → STOP and report. Either
   the FIR halfband change didn't actually take effect, or there's
   another phase/filter contributor we haven't identified.

If `noise_residual_pct` exceeds 12% by a small margin (say 14–15%): also
STOP and report — bumping the threshold modestly might be the right call,
but architect must approve.

Slice 3 regression must continue to PASS.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Brief retrieval note.
2. Diffs (or before/after snippets) for both touched `.cpp` files.
3. Build summary (HQ + product, both configs; warnings count).
4. Slice 4 bench: invocation, exit code, stdout final line.
5. Full pasted `results.md` from the Slice 4 run.
6. Slice 3 regression: invocation, exit code, stdout final line.
7. Self-check table vs §6 prediction.
8. Open questions.

DO NOT iterate beyond §6 outcome 1. If outcome 2 or 3, STOP and let the
architect decide the next move.
