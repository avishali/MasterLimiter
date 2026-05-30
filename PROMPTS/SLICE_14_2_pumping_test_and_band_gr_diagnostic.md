# Cursor Task — Slice 14.2: real pumping test + per-band GR diagnostic

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Bench-focused investigation, on the existing uncommitted Slice 14
> branches.** Slice 14.1's flat kBandLink sweep was inconclusive: kBandLink
> is a compile-time constant, so the "sweep" measured one binary three
> times, AND the 60 Hz+10 kHz test signal is degenerate (each band stays
> under the ceiling, so the band stage never engages). This slice replaces
> the synthetic pumping signal with one that actually exercises the bands,
> adds a **per-band effective-GR diagnostic** computed from plugin I/O, and
> makes kBandLink overridable at build time so we can measure full
> decoupling (0.0) vs the shipped 0.5. Goal: determine whether the band
> stage decouples at all, or whether the architecture (band threshold)
> needs to change. **Do NOT commit, do NOT push.**

> **Continue on the existing Slice 14 branches**: product
> `slice-14-multiband-2band`, HQ `slice-14-multiband-bench`. The product
> rewire from Slice 14.1 stays. No new branches.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- HQ `shared/dsp_bench/` edits permitted (the bulk of this slice).
- ONE small product edit permitted: a **default-preserving** compile-time
  override for `kBandLink` (§3). No new params, no new frozen IDs, no
  runtime/UI surface, no RT-path behaviour change at the default.
- No `mdsp_dsp` source edits. No ADR edits.
- RT-safety unchanged (no new audio-thread work).
- **Do NOT commit, do NOT push.** `MCP` stays dirty.

────────────────────────────────────────
1. WHY (architect review of 14.1)
────────────────────────────────────────

Two confounds made the 14.1 sweep uninformative:

1. **kBandLink is `static constexpr` (compile-time).** A runtime "sweep"
   cannot change it, so 0.0/0.25/0.5 returned bit-identical numbers.
2. **The 60 Hz+10 kHz signal is degenerate.** Each tone (one per band)
   sits *below* the ceiling individually; only their sum limits, which is
   inherently a wideband event. The band envelopes share the ceiling
   threshold, so they never engage → multiband output ≡ single-band output
   (both 7.81%). That match is the real evidence.

Also note the blend math: at `kBandLink = 0.5`, even a fully passive high
band is pulled down by half the low band's reduction
(`gHighOut = 0.5·min(gLow,gHigh) + 0.5·gHigh`). The shipped 0.5 is therefore
~50% re-coupled *by design*. To see whether the mechanism works at all we
must measure at `kBandLink = 0.0`.

This slice answers one question: **on a signal where the bass alone exceeds
the ceiling, does the low band reduce while the high band is spared?**

────────────────────────────────────────
2. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `shared/dsp_bench/signals.py` — `_cross_band_imd_60_10k` (~90),
  `generate()` dispatcher (~114), `list_synthetic_signal_names` (~194).
- `shared/dsp_bench/measure.py` — `cross_band_sidebands_dbc` (~341),
  `crest_factor_db`, `_rms`, and the band-pass helper pattern (scipy
  `sps.butter(..., btype="band"|"low"|"high", output="sos")`, ~249).
- `shared/dsp_bench/bench.py` — `_process_for_cross_band_imd` (~77) and
  `_run_slice14_cross_band_imd` (~132): the matched-GR iteration and the
  master/Ozone/baseline plumbing.
- `shared/dsp_bench/criteria.py` — `SLICE_14_CROSS_BAND_IMD` (~465) and its
  check (~517).
- `Source/PluginProcessor.{h,cpp}` — the `kBandLink` definition.
- `shared/dsp_bench/drivers/ozone_maximizer.py` — its settings.

Output the retrieval log.

────────────────────────────────────────
3. PRODUCT — make kBandLink build-overridable (default-preserving)
────────────────────────────────────────

Replace the constant definition with a macro-guarded default so the shipped
behaviour is unchanged (0.5) but a build can override it:

```cpp
#ifndef MDSP_BAND_LINK
 #define MDSP_BAND_LINK 0.5f
#endif
// ...
static constexpr float kBandLink = MDSP_BAND_LINK;
```

Produce two Release builds:
- **Default build** (`kBandLink = 0.5`, shipped) — the existing
  `build-release`.
- **Decoupled build** (`kBandLink = 0.0`) — a separate build dir, e.g.:
  ```bash
  cmake -B build-release-banlink0 -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-DMDSP_BAND_LINK=0.0f"
  cmake --build build-release-banlink0 -j
  ```
  (Adapt to this project's CMake setup; the only requirement is a VST3 with
  `kBandLink = 0.0`.) Confirm the override took (print the effective value
  or diff the artefacts).

Do NOT change the shipped default. After the investigation, the default
build remains `0.5` unless §6 concludes otherwise.

────────────────────────────────────────
4. BENCH — new pumping signal + diagnostics
────────────────────────────────────────

### 4.1 New signal: `pumping_kick_hf` (`signals.py`)

A repeating bass transient whose **peak alone exceeds the ceiling**, plus a
**steady, quiet high tone** that stays well under the ceiling:

- **Bass:** a decaying sine burst, ~60–80 Hz, smoothly windowed (raised-
  cosine attack+decay, ~120 ms total) to keep harmonic energy out of the
  high band. Repeat at `f_kick ≈ 4 Hz` (every 250 ms). Scale so the bass
  burst peak is the dominant peak of the whole signal (it will be driven a
  few dB over the ceiling by the matched-GR iteration → the LOW band
  engages).
- **High tone:** a continuous sine at a frequency that is **not** a harmonic
  of the bass (e.g. bass 70 Hz, high 6500 Hz → 92.86×, non-integer), at a
  fixed **−18 dBFS** so the HIGH band stays passive (it is the "victim" we
  measure for ducking).
- Sum, stereoize, length ~4 s (or `quick` ~2 s). Verify (assert/print) that
  the high-tone band energy is steady in the *dry* signal (no bass leakage
  into the high band beyond ~−60 dB).

Register it in `generate()` and `list_synthetic_signal_names()` following
the existing pattern; give it its own RNG seed constant.

### 4.2 New measures (`measure.py`)

**(a) Per-band effective GR — the diagnostic.** Band-split BOTH dry and wet
(latency-aligned) with an LR4-equivalent ~120 Hz split (scipy `butter`
low/high, 4th order, matching the plugin's crossover frequency), then:

```python
def per_band_gr_db(dry, wet, sample_rate, crossover_hz=120.0) -> dict:
    # align wet to dry (reuse latency_samples / xcorr), gain-match the
    # broadband first if the chain applies makeup, then per band:
    #   gr_low  = 20*log10(rms(dry_low)/rms(wet_low))
    #   gr_high = 20*log10(rms(dry_high)/rms(wet_high))
    return {"gr_low_db": gr_low, "gr_high_db": gr_high,
            "decoupling_db": gr_low - gr_high}
```

`decoupling_db = gr_low − gr_high` is the headline: large positive ⇒ low
band reduced while high band spared ⇒ decoupling works. ~0 ⇒ both ducked
equally ⇒ no decoupling.

**(b) HF ducking modulation.** Take the wet HIGH band, compute its amplitude
envelope (e.g. abs→one-pole or Hilbert magnitude), and measure the energy
at `f_kick` (and 2–3 harmonics) relative to its DC level — i.e. how deeply
the steady high tone is amplitude-modulated at the kick rate:

```python
def hf_ducking_modulation_db(wet, sample_rate, hf_hz, f_kick, crossover_hz=120.0) -> float:
    # band-pass wet around hf_hz; take envelope; FFT the envelope;
    # return 20*log10( sum(energy at k*f_kick, k=1..3) / energy at DC )
```

Lower (more negative) ⇒ less pumping. Single-band should be high (deep
ducking); decoupled multiband should be much lower; Ozone is the reference.

### 4.3 New runner (`bench.py`)

Add a `--slice 14` mode variant (or a `--pumping` flag — match the existing
runner-dispatch style) that, for the `pumping_kick_hf` signal at matched
~3 dB GR (reuse `_process_for_cross_band_imd`'s GR-matching iteration,
generalized to this signal), renders and reports, for each of:

- `master_limiter` **default** build (kBandLink 0.5),
- `master_limiter` **decoupled** build (kBandLink 0.0),
- `pre_slice14` single-band baseline,
- `ozone_maximizer` (its own transparent settings, matched ~3 dB GR),

the following per config: `measured_gr_db`, `per_band_gr_db`
(`gr_low_db`, `gr_high_db`, `decoupling_db`), and
`hf_ducking_modulation_db`. Print a clean table.

The decoupled and default master builds are selected via two
`--plugin-path` invocations (point at `build-release` and
`build-release-banlink0`); you may run the runner twice and merge, or accept
a second `--master-decoupled-path` arg — whatever fits the existing CLI.

### 4.4 Criteria / decision gates (`criteria.py`)

This is a **diagnostic** slice; encode the fork as explicit pass/stop logic:

- **Mechanism validated** if the **decoupled build (kBandLink 0.0)** shows
  `decoupling_db ≳ +6 dB` (low band clearly reduced, high band largely
  spared) AND `hf_ducking_modulation_db` materially below the single-band
  baseline (target: within a few dB of Ozone). Record the exact margins you
  observe; do not invent a pass you didn't measure.
- **Architecture problem** if the decoupled build shows `decoupling_db ≈ 0`
  (low ≈ high GR) on a signal where the bass burst alone exceeds the ceiling
  — i.e. the bands are *still* not engaging. In that case **STOP** and
  report the per-band GR; do not proceed to §5. This means band threshold =
  ceiling is too high and the architecture needs separate band thresholds
  (a follow-up ADR-0009 revision, architect's call).

────────────────────────────────────────
5. IF MECHANISM VALIDATED — re-tighten Slice 3/4/5 (deferred from 14.1)
────────────────────────────────────────

Only if §4.4 validates the mechanism: the 14.1 rewire removed the
double-limiting over-squash, so re-run Slice 3/4/5 quick (default build) and
pull the thresholds loosened in Slice 14 back to the **smallest loosening
that still passes**. Expect transient-crest to recover from −10.3 toward its
pre-multiband −2; only the genuine static-LR4-allpass null residual should
remain loosened. Report before→after for every threshold; confirm IMD/THD
were not loosened. If crest still needs ~−10 to pass, STOP and report
(over-squash not actually fixed).

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE14_2_REGRESSION_S$SLICE"
done
```

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Product: the kBandLink macro guard diff; confirmation both builds exist
   and the override took (effective kBandLink per build).
3. Bench: new signal, the two measures, the runner — short description each.
4. **The diagnostic table** (default 0.5 / decoupled 0.0 / single-band /
   Ozone) with `measured_gr_db`, `gr_low_db`, `gr_high_db`, `decoupling_db`,
   `hf_ducking_modulation_db`.
5. The §4.4 verdict: **mechanism validated** or **architecture problem**,
   with the numbers that decide it.
6. If validated: Slice 3/4/5 before→after thresholds (§5).
7. `git status --short` product + HQ (uncommitted on Slice 14 branches;
   `MCP` dirty). Note any extra build dir created.
8. Open questions — especially a recommended final `kBandLink` if the
   default/decoupled comparison suggests 0.5 is too re-coupled.

────────────────────────────────────────
7. ARCHITECT REVIEW
────────────────────────────────────────

Architect reads the diagnostic table and decides:
- **Validated** → pick the final `kBandLink` (decoupling vs tonal-tilt
  trade), then avishali auditions, then consolidated close.
- **Architecture problem** → revise ADR-0009 to give the bands their own
  (lower) thresholds, and write the follow-up slice.

**Do not self-close, do not pick the final kBandLink yourself, do not
revise ADR-0009.**
