# MasterLimiter — Session Handoff (2026-06-09)

Context-clear handoff. Everything a fresh session needs to continue the limiter voicing work.

---

## 0. Workflow (read first)
- **Trinity roles:** Claude = architect / reviewer / **Cursor-prompt-writer** (does NOT edit code directly). Cursor = coder/builder/committer. **avishali** = decides & auditions in Ableton.
- **Every Cursor prompt MUST include an "Allowed files to touch" block** (scoped to the slice + PLAN/PROGRESS) so Cursor doesn't prompt for permission each file.
- **SDK path gotcha:** the build compiles the **sibling** repo `…/MelechDSP/melechdsp-hq`, NOT `third_party/melechdsp-hq`. Edit the sibling for SDK changes.
- **Commit + push after EVERY slice.** (A bare `rm -rf` wiped the repo this session — fully recovered, but the lesson is: push always. Never run unscoped `rm -rf`.)
- Slices are gated; PLAN.md / PROGRESS.md track state.

## 1. Repos, paths, remotes
- **Plugin:** `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter` → remote `git@github.com:avishali/MasterLimiter.git` (branch `main`). Recovered + pushed, builds clean.
- **SDK:** `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq` → remote `git@github.com:avishali/melechdsp-hq.git` (branch `master`). Recent limiter work committed + pushed.
- **JUCE:** `/Users/avishaylidani/DEV/SDK/JUCE` (export `JUCE_PATH` to build).
- **Analyzer:** `tools/analysis/analyze.py` + venv `tools/analysis/.venv` (numpy/scipy/soundfile/pyloudnorm).
- **Test files:** `/Users/avishaylidani/Music/Test Files/` — mixes (pre-master) + `Audio Tests/` (60+7k IMD, basswave2, Rumble50Hz, sweeps, pink/white).
- Stale dirs to delete when comfortable: `MasterLimiter-DAMAGED-delete-later`, `MasterLimiter-RECOVERED-delete-later`.

## 2. Build / install
- **Audio testing:** Debug is fine (identical audio). `JUCE_PATH=… bash scripts/build.sh` (Debug → build-debug).
- **CPU testing:** MUST be Release. `cmake --preset default && cmake --build build` (Release, native arm64).
- **Install (dev, no sudo, recommended):** copy `build/MasterLimiter_artefacts/Release/{VST3,AU}/*` to `~/Library/Audio/Plug-Ins/{VST3,Components}`. (TODO: wire `build.sh` to do this automatically.)
- **System install (needs sudo):** `sudo CONFIG=Release BUILD_DIR="$(pwd)/build" bash scripts/install_system.sh` — note env vars go AFTER `sudo` (it strips env otherwise).
- No binary assets / `juce_add_binary_data` — UI is fully programmatic.

## 3. Product / DSP state (what's implemented)
Maximizer, v0.3.0 (beta). Signal flow (in `processCore`, inside `if (limiterActive_)`):
clipper(optional) → input Gain → **4× oversample** → 2-band split (LR @120Hz) → per-band limiter envelopes → recombine → wideband limiter → **ceiling output gain** → downsample → **FinalCeilingLimiter** (true-peak residual catch) → I/O out.

Key models (all recent, all in this session):
- **Ceiling decoupled from GR (Ozone model):** limiter threshold fixed at **1.0 (SP) / 0.965 (TP)**; **Ceiling = output gain** applied after limiting. GR responds only to Gain, not Ceiling. `ceiling_db` default **0.0**.
- **Color knob** = `mapBandColorToLink = 1 - color/100`: **0% = fully linked (transparent/wideband-like), 100% = independent (multiband character)**. `band_color` default **0**. `MDSP_BAND_HEADROOM_DB = 0` (pre-shave removed).
- **FinalCeilingLimiter** (`mdsp_dsp`, new): final true-peak/sample-peak brickwall, catches residual ISP so output TP = ceiling. **Wideband TP=−1.0 ✓. Multiband TP still leaks to −0.4 (BUG, deferred — see backlog).**
- **True-peak meters:** real ISP via `mdsp_dsp::TruePeakDetector` (4 instances: IN L/R + OUT L/R), shown above peak/RMS readouts; overs >0 dBFS render **red with "+"**.
- **Clipper power switch** (`clipper_active`, default on; gated inside limiter block).
- **Auto-release (program-dependent)** — the current focus, see §5.
- **Lookahead = 7 ms** (was 5). Total latency ~**934 samples**.
- Oversampler = manual 2-stage FIR (flat to 20 kHz, −0.00004 dB @20k; tw 0.03/0.10).

## 4. Shootout findings (the "why")
60+7k IMD test (low+high tone; intermod sidebands around 7 kHz = bass→treble distortion = release-induced AM). `IMD↓car`, lower = cleaner.
- **At matched loudness/GR, MasterLimiter (multiband + auto-release) = −80 dB IMD — beats Ozone Maximizer (−15…−45) and Waves L4 (−17).** Pro-L 2 ≈ −39 in its clean wideband mode (the transparency benchmark).
- **Bass-ducking cap:** on bass-heavy signal, wideband loudness walls out (~−5.4 LUFS) because the 60 Hz "owns" the gain and the 7 kHz rides fixed. **Multiband breaks the cap** → reaches −4.5 LUFS (Pro-L's loudness) while staying −80 IMD. **Multiband + auto-release is our edge.**
- Method: **loudness-match (or GR-match) + cleanest mode + extras off**, then compare IMD. Use `analyze.py`. (Renders must have silence trimmed — analyzer auto-trims.)
- References avishali owns: Pro-L 2 (transparency target), Weiss MM-1, L2, L4, Ozone 11, KClip3, bx_clipper, The God Particle.

## 5. CURRENT TASK — auto-release voicing (in progress)
**Goal:** make the auto-release clean + program-dependent like Pro-L ("track the coming envelope, release per the signal, use lookahead"). Clean on sustained bass (no pump) AND punchy on acoustic transients (no dulling).

**How our auto-release works:** per sample, `depth` = current GR; `sigma` = smoothed depth (how *sustained* limiting is); `relAlpha = fast + sigma·(slow−fast)` → low sigma = fast release, high sigma = slow. 2-stage cascaded smoother.

**Fixes already shipped this session:**
- **Asymmetric sigma** (fast 5 ms attack, slow decay) — so sustained-but-oscillating bass keeps sigma high → slow release → no pump. (Was symmetric → averaged to mid → bass ripple.)
- **Frequency-dependent release:** `LimiterEnvelope::setAutoReleaseScale()`; **low band 3× slower** (`kLowBandAutoReleaseScale`), high/wideband 1×.
- Lookahead → 7 ms.

**Status:** avishali says "a bit better but still not smooth — the release is audible" (couldn't isolate pump vs grit vs adaptation-being-heard). On an **acoustic** mix at **Color 100, Auto Transparent**.

**Active experiment — DEV real-time tuning controls** (TEMPORARY, remove before 0.4): 4 APVTS dev params + a "DEV RELEASE" UI strip:
- `dev_low_band_release_scale` (def 3.0), `dev_high_band_release_scale` (def 1.0), `dev_sigma_attack_ms` (def 5), `dev_sigma_decay_scale` (def 1.0). Live, RT-safe.
- **Next action:** avishali runs the 3-step sweep — (1) freeze adaptation (sigma_decay_scale↑, attack↑) to test if the *adaptation* is the audible artifact; (2) sweep low/high band scales for a smooth fixed rate; (3) reintroduce slight adaptation. Report which step clicked + the 4 values → **bake them as constants, delete dev params.**
- **If no setting sounds Pro-L-smooth:** the exponential-smoother-with-adaptive-time architecture has a ceiling → rebuild release as a **lookahead envelope-follower** (gain rides a smooth curve from the buffered upcoming signal; no audible rate-switching). Design is understood, it's a bigger slice.

## 6. Open backlog (prioritized)
1. **Auto-release smoothness** (§5) — active.
2. **CPU / clicks:** ~50% + intermittent clicks were **Debug build + 6 oversamplers**. Release build pending CPU confirmation from avishali. Regardless: **trim the 4 metering TruePeakDetectors** (wasteful — share/decimate) for free CPU.
3. **OS-quality slice (groups two bugs, same 4× cause):** (a) **multiband TP-ISP leak** (output TP −0.4, over ceiling in TP+Color100), (b) **harmonic aliasing** (limiter harmonics fold back at 4×; pre-existing). Fix = higher OS (8×+) and/or proper ISP control. Use the new TP meter to watch the leak live.
4. **Color knob intermediate bug:** at Color 0<x<100 the low end drops (phase cancellation — blends full-band original-phase with allpass band-split). Behaves like on/off. Fix = linear-phase complementary crossover (adds latency). Endpoints (0/100) are fine.
5. **Integer-latency HF phase** (~90° @20kHz, inaudible) — low-pri polish; fix = native-integer FIR (lose meter-align) or accept.
6. **Ableton VST3 not listed** (AU works) — Ableton setting: Preferences→Plug-Ins→"Use VST3 System Folders" ON + rescan. Binary is fine (loads as AU + in Plugin Doctor).
7. **Pre-0.4:** UI tidy-up; **remove DEV release params**; TP/SP metering user control (planned); decide Ceiling default (0 vs −1).

## 7. Tools quick-ref
- Analyze renders: `tools/analysis/.venv/bin/python tools/analysis/analyze.py "<folder or wavs>"` → LUFS, sample/true peak, crest, IMD↓car, THDlo, %active. Auto-trims silence. IMD only meaningful on 2-tone signals.
- Bench DSP in isolation: `/tmp/xotest` had a JUCE console harness (gone after context clear — rebuild if needed; it linked juce_dsp to test crossover/OS/limiter behavior).

## 8. Immediate next steps
1. avishali: confirm Release **CPU %** + clicks gone (load the **AU** in Ableton; VST3 listing is a separate Ableton setting).
2. Run the **dev-control release sweep** (§5) → report values + which step fixed it.
3. Claude: bake the release values (or spec the envelope-follower), delete dev params, commit + **push**.
4. Then: OS-quality slice (TP leak + aliasing), then continue shootout on real music.
