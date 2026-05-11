# MasterLimiter — Progress Log

Append-only. Each entry: date, slice, gate result, notes, artifact links.

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
