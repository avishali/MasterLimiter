# MasterLimiter — Progress Log

Append-only. Each entry: date, slice, gate result, notes, artifact links.

---

## 2026-05-11 — Slice 2: DSP bench harness

**Status:** ✅ Closed. Self-validation PASS 5/5 against pass-through MasterLimiter.

**Deliverables (HQ)**
- `melechdsp-hq/shared/dsp_bench/` — Python bench:
  - `bench.py`, `signals.py`, `measure.py`, `host.py`, `report.py`, `criteria.py`
  - `drivers/master_limiter.py` (+ `base.py`)
  - `requirements.txt`: pedalboard, pyloudnorm, numpy, scipy, matplotlib, soundfile
  - `corpus/` with README slot for user-supplied real-world WAVs
- Loads the real VST3 binary via `pedalboard`; renders synthetic corpus + any
  user files present; emits `results.json` / `results.md` / `plots/*.png` /
  `nulls/*.wav`. Exit code reflects pass/fail.

**Deliverables (product repo)**
- Bench artifacts archived under `docs/bench/SLICE_02/<timestamp>/`
  (JSON + MD + plots only — WAVs gitignored per Q2-6).

**Self-validation gate (§15 of slice prompt)**
- [x] All synthetic signals run without error.
- [x] Reported latency = 0, measured = 0 (matches `getLatencySamples()`).
- [x] Null residual K-weighted = −∞ dBFS on every synthetic (bit-exact dry==wet,
      pyloudnorm returns −∞ on silence; trivially passes the −140 dBFS threshold).
- [x] Noise residual on pink = 0 % (bit-exact).
- [x] SMPTE IMD on dual-tone = 0 % (bit-exact).
- [x] True-peak overs (aggregate, pink + sweep + IMD only) = 0.
- [x] `results.json`, `results.md`, and 15 PNG plots exist.
- [x] Exit code 0; final stdout line `PASS 5/5`.

**Caveats noted for Slice 3 prep**
- Per-row TP pass column on `dirac_impulse` (4 overs) and `transient_train`
  (360 overs) shows ❌ in the table because the row check is `tp == 0`.
  These are excluded from the **aggregate** gate via `_tp_gate_signals` —
  expected behaviour (their dry input has intersample overs near 0 dBFS).
  Cosmetic cleanup deferred (small follow-up).
- K-weighting math is unverified against non-zero residuals — pyloudnorm
  returned −∞ for the bit-exact case. Slice 3's actual GR introduces the
  first non-trivial residual and gives the K-weighting its first real workout.

**Open follow-ups spawned**
- Fix `lookahead_ms` `NormalisableRange (5.0, 5.0+1e-5)` in product repo
  `Source/parameters/Parameters.cpp` — triggers JUCE assertion spam when
  hosts scan the plugin. Not gate-blocking (pedalboard tolerates), but real.

**Decisions confirmed in this slice**
- Bench is plugin-agnostic and Python-only.
- Reference-plugin comparison (Pro-L 2 / L3 / Ozone) stays deferred to Slice 5 prep.
- Plugin Doctor is the dev microscope; this bench is the gate keeper.

---

## 2026-05-11 — Slice 1: Product repo skeleton

**Status:** ✅ Closed. Audition passed in Ableton Live.

**Deliverables**
- Product repo created at `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/`,
  pushed to `git@github.com:avishali/MasterLimiter.git`, branch `main`.
- HQ submodule wired at `third_party/melechdsp-hq`.
- CMake / presets mirror AnalyzerPro. Identifiers frozen: `MasterLimiter` /
  `MaLm` / `Melc` / `MelechDSP` / `0.1.0` / `com.MelechDSP.MasterLimiter`.
- Formats: VST3, AU, Standalone (no AAX in dev). Stereo I/O only.
- APVTS layout v1: all 9 parameter IDs declared with version hint 1 (inert).
- `mdsp_ui::LookAndFeel` + `mdsp_ui::Theme` applied to placeholder editor.
- Pass-through `processBlock`, 0-sample latency.
- LICENSE (proprietary), README, .gitignore, etc.

**Patches during slice**
- Slice 1.1: removed `isDiscreteLayout()` over-restriction that rejected stereo;
  collapsed self-assign loop to clean no-op.

**Gate result**
- [x] Builds clean (VST3 + AU + Standalone), 0 errors, 0 new warnings.
- [x] Repo at correct sibling path; pushed to GitHub.
- [x] HQ submodule resolves.
- [x] Audition pass in Ableton Live (VST3 + AU + Standalone, UI, params, null,
      latency, persistence). Reported by avishali 2026-05-11.

**Notes**
- HQ submodule URL: `git@github.com:avishali/melechdsp-hq.git` (SSH).
- Origin set via SSH `git@github.com:avishali/MasterLimiter.git`.

---

## 2026-05-11 — Slice 0: Architecture & plan

**Status:** ✅ Closed (committed `dc54c29`).

**Deliverables**
- [ADR-0004](../third_party/melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md) — DSP architecture (HQ)
- [SPEC.md](SPEC.md) — v1 product spec
- [PLAN.md](../PROMPTS/PLAN.md) — slice plan
- This progress log

**Gate**
- [x] ADR-0004 approved
- [x] SPEC.md approved (criteria tightened to beat Pro-L 2 / L3 / Ozone)
- [x] Slice list & ordering approved

**Notes**
- Reference targets locked: Pro-L 2, Waves L2/L3, Ozone Maximizer (IRC).
- Latency budget: ~5–6 ms reported; reserved headroom for future "Max Transparency" mode.
- Transient/sustain split method deferred to ADR-0005 (decided on bench in Slice 5 prep).
- Auto-release algorithm deferred to ADR-0006 (Slice 9 prep).
- Pass criteria tightened to beat Pro-L 2 / L3 / Ozone by ≥ 3 dB null residual on
  the same corpus + absolute thresholds at 3 / 5 / 7 dB GR. See SPEC §5.
- Product repo path locked: `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/`
  (sibling to AnalyzerPro). Name is a placeholder; marketing rename later.
