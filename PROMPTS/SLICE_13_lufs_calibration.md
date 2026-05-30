# Cursor Task — Slice 13: LUFS calibration (~1 LU offset vs WLM / Insight 2)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ repo only — primary DSP fix.** No product-repo source
> changes (only the standard end-of-slice submodule bump after the
> HQ commit lands). Fixes the K-weighting filter coefficients in
> `mdsp_dsp::LoudnessAnalyzer` so integrated/short-term/momentary
> LUFS readings match BS.1770-4 reference meters (WLM, Insight 2)
> within ±0.1 LU. No new params, no API change, no ADR
> (calibration bugfix). Branch off HQ `master`. **Do NOT push** —
> architect handles push + close.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- HQ edits permitted (this is an HQ-side slice).
- No product-repo changes in this slice. The standard
  `chore: bump HQ submodule` commit on the product side happens
  in the close prompt after HQ lands.
- Branch `slice-13-lufs-calibration` off HQ `master`. **Do NOT
  commit, do NOT push.**
- `MCP` nested-submodule pointer in HQ remains dirty as it has
  since the ADR-0007 close — leave it untouched throughout.

────────────────────────────────────────
1. WHY (audition findings, Slice 12.2)
────────────────────────────────────────

avishali compared MasterLimiter's integrated/short-term LUFS
readout against WLM and iZotope Insight 2 on the same source
material and consistently measured a **~1 LU positive offset**
(ours reads ~1 LU louder than reference). The offset is
program-independent, suggesting a calibration error in the
K-weighting filter chain, not a metering window or accumulation
bug.

Architect inspection of
`shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp` identified
two BS.1770-4 deviations in the K-weighting filter coefficients:

**Bug A — Stage 1 high-shelf frequency off by ~180 Hz.**
BS.1770-4 §A.1 specifies the Stage 1 filter as a high-shelf at
`f₀ = 1681.974 Hz, Q = 1/√2, gain = +4 dB`. Current code uses
`1500.0f` Hz. A 180-Hz-low shelf raises HF energy more
aggressively → measured loudness reads higher.

**Bug B — Stage 2 RLB Q wrong (default Q used instead of spec Q).**
BS.1770-4 §A.2 specifies the Stage 2 filter as a high-pass at
`f₀ ≈ 38.135 Hz, Q ≈ 0.5003`. Current code uses
`makeHighPass(currentSampleRate, 38.0f)` — JUCE's two-argument
overload defaults to Q = 1/√2 ≈ 0.707. The wrong Q changes the
low-end rolloff shape; together with bug A this plausibly
accounts for the measured ~1 LU offset.

What's already correct (verified):
- Reference constant `−0.691 dB` in `unitsToLufs()` (BS.1770).
- Stereo channel weights `G_L = G_R = 1.0` (`msL + msR`).
- Mean-square accumulation, momentary 0.4 s / short-term 3 s
  windowing, integrated accumulation across the session.

Bench impact on the product: NONE. The analyzer output feeds UI
meters only — not the audio path. Slice 11b2's Track loop reads
short-term LUFS, but Track is OFF at bench defaults
(`gain_match_auto = false`), so the analyzer numerics don't
influence DSP output. Slice 3/4/5 PASS unchanged.

────────────────────────────────────────
2. TRINITY (lightweight)
────────────────────────────────────────

Read:
- `shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp` —
  especially `updateFilters()` (lines 44–58).
- `shared/mdsp_dsp/include/mdsp_dsp/loudness/LoudnessAnalyzer.h`
  — to confirm the API surface stays untouched.
- BS.1770-4 §A (Annex 1: pre-filter + RLB stage specifications).
  Reference values (analog-prototype):
  - Stage 1: high-shelf, `f₀ = 1681.974 Hz`, `Q = 0.7071752`,
    `G = 3.999843 dB`.
  - Stage 2: high-pass, `f₀ = 38.135 Hz`, `Q = 0.5003270`.
- JUCE `juce::dsp::IIR::Coefficients<float>::makeHighShelf` and
  `makeHighPass` (3-arg overload that takes Q).

Output the retrieval log.

────────────────────────────────────────
3. SCOPE — files
────────────────────────────────────────

**MODIFY:**
- `shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp` — fix the
  two coefficient calls in `updateFilters()`. Two lines change.

Do NOT touch any other file. No header changes (API surface
unchanged). No product-repo source changes. No ADR (calibration
bugfix, not architectural).

────────────────────────────────────────
4. WHAT TO BUILD
────────────────────────────────────────

### 4.1 Stage 1 — high-shelf frequency

Replace:

```cpp
auto coefsStage1 = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
    currentSampleRate,
    1500.0f,
    1.0f / std::sqrt (2.0f),
    juce::Decibels::decibelsToGain (4.0f));
```

with:

```cpp
auto coefsStage1 = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
    currentSampleRate,
    1681.974f,                                       // BS.1770-4 §A.1
    1.0f / std::sqrt (2.0f),                         // Q = 0.7071752 ≈ 1/√2
    juce::Decibels::decibelsToGain (4.0f));          // G = +4 dB (3.999843)
```

The `Q = 1/√2` value already matches the spec to 4 decimal
places (`0.7071068` vs spec `0.7071752`); no Q change needed
here.

### 4.2 Stage 2 — RLB high-pass Q

Replace:

```cpp
auto coefsStage2 = juce::dsp::IIR::Coefficients<float>::makeHighPass (
    currentSampleRate, 38.0f);
```

with:

```cpp
auto coefsStage2 = juce::dsp::IIR::Coefficients<float>::makeHighPass (
    currentSampleRate,
    38.135f,                                         // BS.1770-4 §A.2
    0.5003f);                                        // explicit Q (default would be 1/√2 — wrong)
```

JUCE's 3-argument `makeHighPass (sampleRate, freq, Q)` overload
takes the explicit Q. Verify the signature exists in the JUCE
version this project uses; if not, fall back to constructing
the Coefficients directly from BS.1770-4 §A.2 bilinear-transform
numerics — but the 3-arg overload has been in JUCE for many
versions, so it should be available.

### 4.3 What stays unchanged

- The `reset()` semantics (filter state reset, history buffer
  clear, sentinel `-100.0f`).
- `process()` per-sample pipeline order (pre-filter → RLB → sum
  of squares).
- `computeWindow` math and the momentary/short-term/integrated
  reductions.
- `unitsToLufs` formula `−0.691 + 10·log₁₀(z)`.
- API: `LoudnessSnapshot`, `getSnapshot`, public method names.
- Comments, namespaces, headers.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

### 5.1 Build

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short                                   # product clean expected
cd third_party/melechdsp-hq
git status --short                                   # ' M MCP' expected; nothing else
git checkout -b slice-13-lufs-calibration
# (edit shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp per §4)
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```

Zero new warnings in any `Source/` or `shared/mdsp_dsp/`
files.

### 5.2 Product bench (must PASS unchanged)

The analyzer feeds UI meters only; audio path unaffected at
bench defaults (Track off, `gain_match_auto = false`).

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE13_REGRESSION_S$SLICE"
done
```

All three end `PASS N/N`.

### 5.3 LUFS calibration validation — EBU TECH 3341

EBU TECH 3341 publishes a test corpus with known reference LUFS
values. If the corpus is already available in
`shared/dsp_bench/refs/ebu-tech-3341/` or similar, use those;
otherwise download from the EBU site (or generate the canonical
sine/pink reference signals from BS.1770-4 §B.5).

For Slice 13, the **minimum acceptable validation** is to run
the analyzer offline against at least these reference signals:

1. **−23 LUFS sine wave** (the BS.1770 calibration tone:
   1 kHz, stereo, scaled to give exactly −23 LUFS integrated).
   - Expected integrated LUFS: `−23.0 ± 0.1`.
   - Expected short-term LUFS (steady-state): `−23.0 ± 0.1`.
2. **EBU TECH 3341 test sequence 1 (`seq-3341-1-16bit.wav`)**:
   stereo 1 kHz tone at −23 LUFS for 20 s.
   - Expected integrated LUFS: `−23.0 ± 0.1`.
3. **EBU TECH 3341 test sequence 3 (`seq-3341-3-16bit.wav`)**:
   100 Hz / 1 kHz / 10 kHz tones at various levels — exercises
   the K-weighting frequency response.
   - Expected integrated LUFS: `−23.0 ± 0.1`.

If those references aren't readily available, the architect-
acceptable fallback is:

- Generate a **1 kHz sine, stereo, peak amplitude that gives
  −23 LUFS integrated** (gain calibration: for a sine at
  amplitude `A`, the linear mean-square per channel is
  `A² / 2`; sum = `A²`; after K-weighting at 1 kHz the gain is
  approximately +1 dB so `mean_square_post_K ≈ A² · 10^(1/10)`;
  set `A` so the LUFS formula yields −23). Cursor can compute
  this analytically; the BS.1770 reference is `A ≈ 0.7943`
  (stereo, 1 kHz, before K-weighting).
- Validate that our analyzer reports `−23.0 ± 0.1` integrated
  on this synthesized buffer fed through the analyzer's
  `process()` method.

**Implementation note for offline validation:** the cleanest
path is a small standalone harness inside `shared/dsp_bench/`
that instantiates a `LoudnessAnalyzer`, calls `prepare(48000,
1024)`, feeds it the reference buffer in 1024-sample chunks,
and reads `getSnapshot().integratedLufs` after the buffer
exhausts. If such a harness already exists, reuse it;
otherwise write one in `shared/dsp_bench/loudness_calibration.cpp`
(or `.py` if the Python bench harness has a wrapper for HQ
classes — pick whichever fits the existing bench convention).

Report the measured LUFS for at least one reference signal.
Pass criterion: measured integrated LUFS within `±0.1 LU` of
the BS.1770-4 expected value.

### 5.4 Smoke checks for the architect / avishali audition

In Ableton against avishali's familiar reference material:

1. Open the same source she used for the original "1 LU offset"
   measurement.
2. Compare MasterLimiter's integrated LUFS readout against WLM
   and Insight 2 on the same track.
3. Expected: agreement within typical inter-meter variance
   (±0.2 LU on real-world material is normal; the ~1 LU
   systematic offset should be gone).
4. Sanity: short-term and momentary readouts also track within
   tolerance.

If the offset is reduced but not eliminated (e.g. residual
~0.3 LU), this is acceptable for Slice 13 close — the two
biggest coefficient bugs are fixed, and remaining smaller
deltas may come from bilinear-transform vs analog-prototype
approximation. A follow-up Slice 13.1 could hardcode exact
BS.1770-4 §A coefficients per sample rate (44.1, 48, 88.2, 96)
if avishali finds the residual unacceptable.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. File modified, one-line purpose
   (`shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp`).
3. Exact before/after of both coefficient calls (Stage 1 freq;
   Stage 2 freq + Q).
4. Build summary (Debug + Release, warnings).
5. Product bench: invocation + `PASS N/N` for Slice 3 / 4 / 5.
6. LUFS calibration validation: which reference signal(s) were
   used, the measured integrated LUFS, the expected value, the
   delta, and pass/fail vs the ±0.1 LU criterion.
7. `git status --short` on HQ (working tree dirty on the slice
   branch, no commit; `MCP` still dirty from before).
8. `git status --short` on product (clean — no product changes
   this slice).
9. Open questions.

────────────────────────────────────────
7. ARCHITECT AUDITION + CLOSE
────────────────────────────────────────

avishali re-tests in Ableton per §5.4. On approval:

- Single HQ commit on `slice-13-lufs-calibration` → push → FF
  HQ `master` (same `MCP`-stash-aside pattern as ADR-0010 close
  if needed).
- Product `chore: bump HQ submodule` commit picking up the
  Slice 13 fix → push → FF product main.
- PROGRESS.md entry at the TOP noting the calibration fix.
- PLAN.md updated to mark Slice 13 closed and promote
  ADR-0009 (multiband detection) to active.

No product source touched, so no product-source close commit.
