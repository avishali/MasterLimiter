# Cursor Task — Slice 14.3: band-threshold headroom (bands = primary limiter)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product DSP change + HQ bench sweep, on the existing Slice 14 branches.**
> Slice 14.2 proved the 2-band stage decouples but plateaus ~2.8 dB short of
> Ozone on HF ducking, because every stage shares the ceiling threshold: the
> band limits the bass *to the ceiling*, so ceiling-pinned-bass + highs still
> exceeds the ceiling and the wideband catch re-ducks the highs. This slice
> makes the band envelopes limit to a threshold **`bandHeadroomDb` below the
> wideband threshold** (ADR-0009 Revision R2), turning the bands into the
> primary limiter and the wideband stage into a light final catch. It sweeps
> the headroom (and confirms kBandLink) against the Ozone HF-ducking number to
> find the best value, then re-tightens Slice 3/4/5. **Do NOT commit, do NOT
> push.**

> **Continue on the existing Slice 14 branches**: product
> `slice-14-multiband-2band`, HQ `slice-14-multiband-bench`. No new branches.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product source edits permitted (`Source/PluginProcessor.{h,cpp}`): the
  band-threshold offset + a default-preserving macro for `bandHeadroomDb`,
  mirroring the existing `MDSP_BAND_LINK` guard.
- HQ `shared/dsp_bench/` edits permitted (extend the 14.2 pumping runner with
  a headroom sweep + a loudness/peak readout).
- No `mdsp_dsp` source edits. ADR-0009 already revised by the architect (R2)
  — do NOT edit it.
- RT-safety: the new band threshold is computed once per block from existing
  params; no new audio-thread allocation/logging/locks.
- **Do NOT commit, do NOT push.** `MCP` stays dirty.

────────────────────────────────────────
1. WHY (ADR-0009 Revision R2)
────────────────────────────────────────

Read `third_party/melechdsp-hq/docs/DECISIONS/ADR-0009-...md` → "Revision —
2026-05-30", sections R2 and R3. Summary:

- Bands limiting to the same ceiling as the wideband stage cannot beat a
  wideband stage that re-couples at that same threshold. The residual HF
  ducking (Slice 14.2: −19.97 dB vs Ozone −22.80 dB) is the wideband stage
  firing on `ceiling-pinned-bass + highs`.
- Fix: band threshold = wideband threshold − `bandHeadroomDb`. The bands do
  the primary limiting with headroom; the wideband stage rarely fires, so the
  highs are spared. Trade: more headroom = more HF sparing but isolated
  band-dominant transients peak slightly below ceiling (wideband still
  guarantees true-peak; sustained loudness largely unaffected).

────────────────────────────────────────
2. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `Source/PluginProcessor.cpp` lines ~576–594: `thresholdLin` (576),
  `envelope_.setThresholdLinear` (581, wideband — UNCHANGED), and the band
  threshold sets `envelopeLow_.setThresholdLinear (thresholdLin)` (586),
  `envelopeHigh_.setThresholdLinear (thresholdLin)` (591).
- `Source/PluginProcessor.h` lines ~14–15, 105: the `MDSP_BAND_LINK` macro
  guard + `kBandLink` constant (mirror for headroom).
- `shared/dsp_bench/` — the Slice 14.2 pumping runner (`--pumping`,
  `--master-decoupled-path`), `pumping_kick_hf`, `per_band_gr_db`,
  `hf_ducking_modulation_db`; and any existing integrated-loudness / true-peak
  measure to reuse for the loudness-cost readout.

Output the retrieval log.

────────────────────────────────────────
3. PRODUCT — band-threshold headroom
────────────────────────────────────────

### 3.1 Header (`PluginProcessor.h`)

Mirror the `MDSP_BAND_LINK` pattern:

```cpp
#ifndef MDSP_BAND_HEADROOM_DB
 #define MDSP_BAND_HEADROOM_DB 2.0f
#endif
// ...
static constexpr float kBandHeadroomDb = MDSP_BAND_HEADROOM_DB;
```

Also update the **shipped default** `MDSP_BAND_LINK` from `0.5f` to `0.0f`
(Slice 14.2 R3: full decoupling is materially better for pumping at
negligible tonal cost). Keep the macro guard so it stays build-overridable.

### 3.2 processBlock — band threshold below the wideband threshold

The wideband threshold (`thresholdLin`, line 576) and
`envelope_.setThresholdLinear(thresholdLin)` (line 581) stay exactly as they
are. Compute a band threshold once and apply it to **both band envelopes
only**:

```cpp
const float bandThresholdLin = thresholdLin
    * juce::Decibels::decibelsToGain (-kBandHeadroomDb);   // bands sit headroom dB below the wall
```

Then change line 586 and 591:
```cpp
envelopeLow_.setThresholdLinear  (bandThresholdLin);   // was thresholdLin
envelopeHigh_.setThresholdLinear (bandThresholdLin);   // was thresholdLin
```

Everything else in the multiband region (the band-limited-sum wideband
detection from 14.1, the blend, the apply pass) is unchanged. The GR
snapshots remain wideband-driven.

Sanity: with `kBandHeadroomDb = 0`, behaviour is identical to the current
build (band threshold == wideband threshold). The default ships at 2.0.

### 3.3 Build variants for the sweep

Default build (`build-release`): `kBandLink = 0.0`, `kBandHeadroomDb = 2.0`.

Build the headroom sweep set, all at `kBandLink = 0.0`, via separate build
dirs (same `CMAKE_CXX_FLAGS` override mechanism as 14.2):
```bash
for H in 0 1 2 3 4 6; do
  cmake -B build-release-hr$H -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-DMDSP_BAND_LINK=0.0f -DMDSP_BAND_HEADROOM_DB=${H}.0f"
  cmake --build build-release-hr$H -j
done
```
Confirm each override took (CMakeCache / artefact SHA differ).

────────────────────────────────────────
4. BENCH — headroom sweep + loudness cost
────────────────────────────────────────

### 4.1 Extend the pumping runner

Reuse the Slice 14.2 `pumping_kick_hf` signal and metrics. For a **fixed
input drive** (the drive that produced ~3 dB wideband GR at headroom 0 — pin
it so all headroom values see the same input, exposing the loudness trade),
render and report for each headroom build {0,1,2,3,4,6} at kBandLink 0.0,
plus the single-band baseline and Ozone:

Per config:
- `measured_gr_db` (wideband), and **per-band GR** (`gr_low_db`,
  `gr_high_db`).
- `hf_ducking_modulation_db` (the headline — want it approaching Ozone's
  −22.80 dB).
- **Loudness cost**: output integrated LUFS (or broadband RMS, dB) and output
  true-peak dBFS — so we see how much loudness each dB of headroom costs.

Print a sweep table. The goal is the smallest headroom whose
`hf_ducking_modulation_db` is close to Ozone (within ~1 dB) while the LUFS
cost stays small (target ≲ 0.5–1.0 LU vs headroom 0).

### 4.2 Verdict logic

- **Good headroom found** if some value gets `hf_ducking_modulation_db`
  within ~1 dB of Ozone at acceptable loudness cost → report it as the
  recommended `kBandHeadroomDb`; proceed to §5.
- **Plateau short of Ozone** if even large headroom (6 dB) stays well short
  of −22.80 dB → report the plateau; this bounds 2-band IIR and the architect
  decides on the next rung (3-band / STFT). Do NOT proceed to §5 in that case
  beyond reporting; the final config is the architect's call.

Do NOT pick the shipped `kBandHeadroomDb` / `kBandLink` yourself — report the
table and a recommendation; the architect picks.

────────────────────────────────────────
5. RE-TIGHTEN SLICE 3/4/5 (at the default candidate config)
────────────────────────────────────────

Only if §4.2 found a good headroom: rebuild the default `build-release` with
that headroom (and kBandLink 0.0) and re-run Slice 3/4/5 quick. Pull the
thresholds loosened in Slice 14 back to the smallest loosening that still
passes; only the static-LR4-allpass null residual should remain loosened.
Report before→after; confirm IMD/THD not loosened; if transient crest still
needs near −10 dB, STOP and report.

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE14_3_REGRESSION_S$SLICE"
done
```

────────────────────────────────────────
6. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new `Source/` warnings. Then run §3.3, §4, §5.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Product diff: the headroom macro + `bandThresholdLin` + the two band
   `setThresholdLinear` changes; the `MDSP_BAND_LINK` default → 0.0.
3. Build summary (Debug + Release; the sweep build set + override confirmation).
4. **Headroom sweep table**: {0,1,2,3,4,6} dB + baseline + Ozone, with
   `measured_gr_db`, `gr_low_db`, `gr_high_db`, `hf_ducking_modulation_db`,
   output LUFS, output true-peak.
5. §4.2 verdict + recommended `kBandHeadroomDb` (and any kBandLink note).
6. If applicable: Slice 3/4/5 before→after thresholds.
7. `git status --short` product + HQ (uncommitted on Slice 14 branches; `MCP`
   dirty); list any extra build dirs created.
8. Open questions.

────────────────────────────────────────
8. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

Architect reads the sweep, picks final `kBandHeadroomDb` + `kBandLink`
(pumping vs loudness/tilt trade), sets them as the shipped defaults, then
avishali auditions the candidate build on bass-heavy material (de-pumping
audible, tone stable, loudness acceptable, no clicks at 48 k/256 Clean heavy
GR). On approval → Slice 14 consolidated close (ADR-0009 + product DSP + bench
+ PROGRESS/PLAN + prompt archive). **Do not self-close, do not pick the final
constants.**
