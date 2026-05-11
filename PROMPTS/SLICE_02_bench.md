# Cursor Task — MasterLimiter Slice 2: DSP Bench Harness

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Working directory note.** This slice touches HQ (`melechdsp-hq`), not the
> MasterLimiter product repo. Open Cursor with `melechdsp-hq` as the workspace
> root, or be explicit about absolute paths.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE (MANDATORY)
────────────────────────────────────────

Obey `PROMPTS/00_SYSTEM_RULES.txt` in `melechdsp-hq`.

Adapted scope for this slice (it is offline tooling, not plugin code):

- You are an IMPLEMENTATION ASSISTANT. One prompt = one task.
- Modify ONLY files listed in §4. If anything else must change, STOP and ask.
- Real-time-safety rules and Engine/UI boundary rules do NOT apply here —
  this is an offline measurement bench, no audio thread, no UI.
- Minimal diff. No speculative features beyond the listed scope.
- The bench must be deterministic: same input WAVs + same plugin build →
  bit-exact same results across runs.

────────────────────────────────────────
1. TRINITY PROTOCOL RETRIEVAL GATE
────────────────────────────────────────

Before writing code, complete retrieval and output the log:

1. `melech_internal_server` — confirm:
   - `shared/smoke_test/` exists and works as a JUCE-based headless test pattern
   - `shared/mdsp_dsp/include/mdsp_dsp/loudness/LoudnessAnalyzer.h` is the
     reference for our in-runtime K-weighting (used for sanity comparison
     against `pyloudnorm`, not reused in the bench)
   - No existing Python tooling under `shared/` (expected: none)
2. `juce_api_server` — not applicable (no JUCE code in this slice).
3. `melech_dsp_server` — confirm no existing measurement utilities to reuse
   (expected: none in Python; C++ utilities exist but are not in scope here).

If any retrieval is missing, output ONLY a Retrieval Plan and STOP.

────────────────────────────────────────
2. TASK TITLE
────────────────────────────────────────

Create `shared/dsp_bench/` in `melechdsp-hq` — a Python-based offline
measurement bench that loads any MelechDSP plugin (starting with
MasterLimiter), runs a configurable corpus through it at multiple gain-reduction
depths, and produces a JSON + Markdown + plots report with pass/fail against
documented criteria.

Slice 2 ships the bench *infrastructure* and proves it on the current
pass-through MasterLimiter (bit-exact dry-vs-processed). DSP slices 3+ will
then use it.

────────────────────────────────────────
3. CONTEXT
────────────────────────────────────────

- Architecture rationale: `melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md`
- Pass criteria the bench will eventually enforce:
  product-repo `MasterLimiter/docs/SPEC.md` §5
- Slice plan: product-repo `MasterLimiter/PROMPTS/PLAN.md`

Tooling decisions already made (do not relitigate):
- **Pure Python.** No C++ in this slice.
- **Plugin loading:** `pedalboard` (Spotify, MIT) — loads real VST3/AU.
- **K-weighting / LUFS:** `pyloudnorm` — reference BS.1770 implementation.
- **FFT / signal math:** `scipy.signal` + `numpy`.
- **Plots:** `matplotlib`.
- **Env management:** plain `venv` + `requirements.txt` (no `uv`/`poetry`).
- **Bench is plugin-agnostic.** Per-plugin "driver" modules under
  `drivers/` say *which* plugin to load and *how* to set its parameters.
- **MasterLimiter is the first driver.** Future products will add drivers.

────────────────────────────────────────
4. SCOPE — files you may CREATE (everything is new, all in melechdsp-hq)
────────────────────────────────────────

All paths below are under `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench/`.

Top-level:
- `README.md`
- `requirements.txt`
- `.gitignore`
- `bench.py`                        (CLI entrypoint)
- `signals.py`                      (synthetic test-signal generators)
- `measure.py`                      (measurement functions)
- `report.py`                       (JSON + Markdown + plot output)
- `criteria.py`                     (pass/fail thresholds per SPEC §5)
- `host.py`                         (pedalboard wrapper, render orchestration)

Drivers:
- `drivers/__init__.py`             (empty file, makes it a package)
- `drivers/base.py`                 (Driver protocol / dataclass)
- `drivers/master_limiter.py`       (MasterLimiter v0.1 driver)

Corpus directory (synthetic signals are generated at runtime — no committed WAVs):
- `corpus/README.md`                (manifest of expected real-world files;
                                     filenames the bench will look for if present)
- `corpus/.gitkeep`

NON-GOALS for this slice:
- NO reference-plugin comparison (Pro-L 2 / L3 / Ozone) — deferred to Slice 5 prep.
- NO CC0 audio files committed — corpus README is enough; user supplies real material later.
- NO CI integration. Local-run only.
- NO C++ code anywhere.
- NO changes to `shared/mdsp_dsp/`, `shared/smoke_test/`, or any other HQ module.
- NO changes outside `shared/dsp_bench/`.

────────────────────────────────────────
5. PYTHON DEPENDENCIES (pinned in requirements.txt)
────────────────────────────────────────

Pin minor versions for reproducibility. Suggested floors (use latest stable
patch at time of writing):

```
pedalboard>=0.9,<0.10
pyloudnorm>=0.1.1,<0.2
numpy>=1.26,<2.2
scipy>=1.11,<1.15
matplotlib>=3.8,<3.10
soundfile>=0.12,<0.13
```

Python ≥ 3.10 required. State this in `README.md`.

────────────────────────────────────────
6. SIGNALS — `signals.py`
────────────────────────────────────────

Module exports a `generate(name, sample_rate, channels=2) -> np.ndarray`
function returning float32 audio in shape `(num_samples, channels)`,
range −1.0 to +1.0.

Synthetic signals (always available, deterministic — fixed seeds):

| name                  | spec                                                              |
|-----------------------|-------------------------------------------------------------------|
| `pink_noise_60s`      | 60 s pink noise, normalised to −20 LUFS (use `pyloudnorm`).       |
| `sine_sweep_log_30s`  | 20 Hz → 20 kHz log sweep, 30 s, peak −6 dBFS.                     |
| `imd_smpte`           | 60 Hz + 7 kHz sum, 4:1 amplitude ratio per SMPTE RP120, 10 s, peak −6 dBFS. |
| `dirac_impulse`       | 1 s of silence with a single +1.0 sample at index 0 of each channel. |
| `transient_train`     | 10 s of synthetic clicks at 2 Hz: 0.5 ms half-sine pulses, peak +0.9. |

All synthetic signals: deterministic seed (`numpy.random.default_rng(seed=...)`),
seed listed in a constant at the top of `signals.py`.

Real-world signals (loaded from `corpus/` if files exist; gracefully skipped if absent):

| name                  | filename in corpus/      |
|-----------------------|--------------------------|
| `drum_loop_dense`     | `drum_loop_dense.wav`    |
| `full_mix_dense`      | `full_mix_dense.wav`     |
| `full_mix_dynamic`    | `full_mix_dynamic.wav`   |
| `vocal_solo`          | `vocal_solo.wav`         |

The Slice 2 gate must pass using only synthetic signals (real-world files are
unavailable at slice time).

────────────────────────────────────────
7. MEASUREMENTS — `measure.py`
────────────────────────────────────────

All functions take `dry: np.ndarray, wet: np.ndarray, sample_rate: int` unless
noted. All accept stereo (2-channel) input.

Required functions (pure, no I/O):

- `null_residual_kweighted_db(dry, wet, sample_rate) -> float`
  Time-align dry and wet by the measured plugin latency (see `latency_samples`
  below), gain-match (RMS of wet to RMS of dry), subtract, then K-weight per
  ITU-R BS.1770 (use `pyloudnorm`'s filter), return integrated LUFS of the
  residual, expressed as dBFS-equivalent. Lower = better.

- `thd_plus_n_pct(dry, wet, sample_rate, frequency_hz=1000.0) -> float`
  Apply tight band-pass at fundamental, measure energy outside the band.
  Returns percent. Used on pink noise (broadband variant: integrate residual
  after spectral matching) — implement both: `thd_plus_n_pct_tone` and
  `noise_residual_pct`.

- `imd_smpte_pct(dry, wet, sample_rate) -> float`
  SMPTE IMD on the dual-tone signal: detect sidebands around 7 kHz at
  ±60 Hz and harmonics, return percent.

- `crest_factor_db(audio) -> float`
  Peak / RMS in dB. Used for `transient_crest_delta_db = crest_factor_db(wet) - crest_factor_db(dry)`.

- `latency_samples(dry_impulse, wet_impulse) -> int`
  Find the peak in `wet_impulse`, return its sample index relative to the
  expected position in `dry_impulse`.

- `true_peak_overs(audio, sample_rate, ceiling_dbtp=-1.0) -> int`
  4× polyphase upsample (use `scipy.signal.resample_poly`), count samples
  above ceiling. Returns integer count.

- `aliasing_residual_db(dry, wet, sample_rate, cutoff_hz=20000.0) -> float`
  Render at 96 kHz, measure energy above `cutoff_hz` in `wet` minus in `dry`,
  return dBFS.

All functions:
- Must not raise on silence (return sensible sentinel like `-inf`).
- Must accept stereo and return a single scalar (averaged or max-channel as
  appropriate per metric — document choice in docstring).
- Must be deterministic.

────────────────────────────────────────
8. HOST — `host.py`
────────────────────────────────────────

Wraps pedalboard. Single class:

```python
class PluginHost:
    def __init__(self, plugin_path: str, sample_rate: int): ...
    def set_parameters(self, params: dict[str, float | str]) -> None: ...
    def process(self, audio: np.ndarray) -> np.ndarray: ...   # shape preserved
    def reported_latency_samples(self) -> int: ...            # via pedalboard's plugin.latency if exposed; else None
```

Must handle stereo input. Must reset plugin state between renders (re-instantiate
or use pedalboard's reset hook).

────────────────────────────────────────
9. DRIVERS — `drivers/master_limiter.py`
────────────────────────────────────────

Implements the `Driver` protocol from `drivers/base.py`:

```python
@dataclass
class Driver:
    name: str
    plugin_path: str            # absolute or relative; resolved at runtime
    base_params: dict           # params common to all runs
    def params_for_gr_db(self, target_gr_db: float) -> dict: ...
    def supports_slice(self, slice_num: int) -> bool: ...
```

For MasterLimiter Slice 2:
- `name = "MasterLimiter"`
- `plugin_path` default: `${HQ_ROOT}/../MasterLimiter/build-debug/MasterLimiter_artefacts/Debug/VST3/MasterLimiter.vst3`
  (overridable via `--plugin-path` CLI flag)
- `base_params = {"ceiling_db": -1.0, "ceiling_mode": "TruePeak", "stereo_link_pct": 100.0, "ms_link_pct": 100.0, "release_ms": 100.0}`
- `params_for_gr_db(...)`: in Slice 2 returns `base_params` unchanged
  (plugin is pass-through; GR depth is meaningless). Future slices override.
- `supports_slice(n)`: returns `True` for `n >= 1`.

────────────────────────────────────────
10. CRITERIA — `criteria.py`
────────────────────────────────────────

Encodes SPEC §5 thresholds as data. For Slice 2, the only criteria that
matter are the self-validation set (current plugin is pass-through):

```python
SLICE_02_SELF_VALIDATION = {
    "null_residual_kweighted_db_max": -140.0,
    "thd_plus_n_pct_max_pink": 0.001,
    "imd_smpte_pct_max": 0.001,
    "latency_samples_exact": 0,
    "true_peak_overs_max": 0,
}
```

Also encode (but do not enforce in Slice 2) the full §5 grid at 3/5/7 dB GR
so future slices can reference them.

────────────────────────────────────────
11. REPORT — `report.py`
────────────────────────────────────────

After all measurements, write to `--output-dir` (default
`shared/dsp_bench/runs/<ISO8601>/`):

- `results.json` — every measurement as a nested dict; pass/fail booleans;
  metadata block (timestamp, plugin path, plugin version if extractable,
  bench git SHA, sample rate, slice number, host machine info).
- `results.md` — human-readable pass/fail table, one row per (signal × metric).
  Failures highlighted with a leading ❌ glyph.
- `plots/` — per-signal: dry-vs-wet spectrum overlay PNG, null residual
  spectrum PNG, time-domain residual envelope PNG (matplotlib, headless backend).
- `nulls/` — null residual WAVs (gitignored at the project level — see §13).

Exit code: 0 if all criteria pass, 1 otherwise. Print summary line to stdout:
`PASS 7/7` or `FAIL 5/7 (see results.md)`.

────────────────────────────────────────
12. CLI — `bench.py`
────────────────────────────────────────

```
usage: bench.py [-h] --driver DRIVER --slice N
                [--plugin-path PATH] [--output-dir DIR]
                [--sample-rate {44100,48000,96000}] [--signals ...]
                [--gr-depths ...] [--quick]

required:
  --driver           e.g. master_limiter
  --slice            integer slice number (used for criteria selection)

optional:
  --plugin-path      override the driver's default plugin path
  --output-dir       write reports here (default: runs/<timestamp>)
  --sample-rate      default 48000
  --signals          space-separated; default = all synthetic + any real-world found
  --gr-depths        space-separated GR depths in dB; default per slice
  --quick            shorter signal lengths for fast smoke test (10 s pink, 5 s sweep)
```

Behaviour:
- Resolve plugin path; if missing, exit with clear error.
- Generate synthetic signals; load real-world if present.
- For each signal × GR depth: drive plugin, measure, accumulate.
- Run self-validation check against criteria for the slice.
- Write report. Exit code reflects pass/fail.

────────────────────────────────────────
13. .gitignore CONTENTS
────────────────────────────────────────

```
runs/
__pycache__/
*.pyc
.venv/
corpus/*.wav
!corpus/.gitkeep
!corpus/README.md
```

Synthetic signals are never written to disk; they exist only in memory during
a run.

────────────────────────────────────────
14. README.md CONTENTS
────────────────────────────────────────

Cover, in this order:

- What the bench is (one paragraph).
- Prerequisites: Python ≥ 3.10, a built MasterLimiter VST3 at the default
  path (or `--plugin-path`).
- Setup:
  ```bash
  cd shared/dsp_bench
  python3 -m venv .venv
  source .venv/bin/activate
  pip install -r requirements.txt
  ```
- First run (self-validation against current pass-through MasterLimiter):
  ```bash
  ./bench.py --driver master_limiter --slice 2 --quick
  ```
- Where reports go (default `runs/<timestamp>/`).
- Where to drop real-world corpus files (`corpus/`).
- How to add a new driver (one-paragraph pointer to `drivers/base.py`).

────────────────────────────────────────
15. SELF-VALIDATION GATE (this is the slice's actual gate)
────────────────────────────────────────

After implementing, run:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
./bench.py --driver master_limiter --slice 2
```

The bench is correct only when, against the current pass-through MasterLimiter:

- [ ] All synthetic signals run without error.
- [ ] Reported latency = 0 samples (matches plugin's `getLatencySamples()`).
- [ ] Null residual K-weighted ≤ **−140 dBFS** for every synthetic signal.
- [ ] THD+N delta on pink noise = **0.000 %** (bit-exact).
- [ ] SMPTE IMD on dual-tone = **0.000 %** (bit-exact).
- [ ] True-peak overs at −1 dBTP = **0**.
- [ ] `results.json`, `results.md`, and at least one PNG in `plots/` exist.
- [ ] Exit code 0; stdout final line is `PASS …`.

If any of these fails on a bit-exact pass-through plugin, the BENCH has a
bug (not the plugin). Fix the bench. Do not loosen the thresholds.

PREREQUISITE: the MasterLimiter VST3 must be built. If
`../MasterLimiter/build-debug/...VST3/MasterLimiter.vst3` is missing, STOP
and report — the user will build it before re-running you.

────────────────────────────────────────
16. OUTPUT REQUIREMENTS (what you return to the architect)
────────────────────────────────────────

1. RETRIEVAL LOG (§1 format).
2. File list created with one-line purpose each.
3. The `requirements.txt` content verbatim.
4. The exact bench invocation you ran and its stdout final line.
5. Pasted contents of `results.md` from your self-validation run.
6. Self-check against §15 gate — each item "verified" / "blocked: <why>".
7. Any open questions.

DO NOT skip the self-validation run. The slice is not done until the bench
proves itself against the pass-through plugin.

────────────────────────────────────────
17. POST-SLICE NOTES (for the architect, not Cursor)
────────────────────────────────────────

When this slice passes:
- Architect copies `results.json` + `results.md` + `plots/` (NOT nulls/) into
  `MasterLimiter/docs/bench/SLICE_02/<timestamp>/` and commits in the
  product repo.
- Architect commits the bench itself (HQ) and updates HQ `docs/INDEX.md` if
  appropriate.
- Slice 3 interview starts: lookahead delay + sample-peak single-band limiter.
