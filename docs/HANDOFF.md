# MasterLimiter — Session Handoff (2026-06-10)

Context-clear handoff. Everything a fresh session needs. **Status: v0.3.1-beta is SHIPPED to testers** (signed/notarized, all 4 formats incl. PACE-signed AAX, published to the Cloudflare beta portal). Now **awaiting tester `.mlpreset` feedback to bake the voicing for 0.4.**

---

## 0. Workflow (Trinity — read first)
- **Claude** = architect / reviewer / **Cursor-prompt-writer** (does NOT edit plugin code directly; writes `PROMPTS/SLICE_*.md`). **Cursor** = coder/builder/committer. **avishali** = decides & auditions in Ableton/Pro Tools.
- Every Cursor prompt has an **"Allowed files to touch"** block. Slices are gated; `docs/PROGRESS.md` / `PROMPTS/PLAN.md` track state; each slice ends with a CLOSE archive + commit + **push**.
- **SDK gotcha:** the build compiles the **sibling** repo `…/MelechDSP/melechdsp-hq`, NOT `third_party/`. Edit the sibling for DSP/SDK changes.
- **Commit + push after every slice.** (A `rm -rf` once wiped the repo — fully recovered, but: push always.)
- Claude may directly edit: docs, `release/`, the analysis tools, and the **beta-portal** web app (infra, not plugin C++).

## 1. Repos, paths, tooling
- **Plugin:** `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter` → `git@github.com:avishali/MasterLimiter.git` (main). Clean + pushed.
- **SDK:** `…/MelechDSP/melechdsp-hq` (master). Clean + pushed.
- **JUCE:** `/Users/avishaylidani/DEV/SDK/JUCE` (export `JUCE_PATH`). **AAX SDK:** `/Users/avishaylidani/DEV/SDK/aax-sdk-2-9-0`.
- **Beta portal:** sibling `…/MelechDSP/melechdsp-beta-portal` (Cloudflare Pages+Worker+private R2 `melechdsp-beta`). Pages auto-deploys from `main`; Worker via `npx wrangler deploy` (wrangler in its `node_modules/.bin`, OAuth-authed as avishay).
- **Offline measurement rig:** `tools/analysis/.venv` (pedalboard, numpy, scipy, soundfile, matplotlib — **loads the AU, not the VST3** (VST3 load is flaky headless)). Scripts: `sweep_render.py`, `sweep_isolate.py`, `hammerstein.py`. PDF gen: `release/make_guide_pdf.sh`, `release/make_manual_pdf.sh` (pandoc + Chrome headless).
- **Test files:** `/Users/avishaylidani/Music/Test Files/`.
- **Note:** avishali's `cyrptoBot/binance-bot-v3` (paper-trading binance bot under PM2) was eating RAM/CPU and OOM-killing plugin renders — **stopped** (`pm2 stop` + saved). It belongs on AWS, not local.

## 2. Build / install / measure
- **Dev build:** `export JUCE_PATH=… ; cmake --preset default && cmake --build build` (Release, native arm64, into `build/`). Dev Mode just limits formats/LTO; it does **NOT** gate the DEV params.
- **Install (dev):** copy `build/MasterLimiter_artefacts/Release/{AU,VST3}` to install folders. **Primary install path = SYSTEM** `/Library/Audio/Plug-Ins/{Components,VST3}` (world-writable → no sudo, EXCEPT a stale **root-owned** system AU may need a one-time `sudo rm -rf /Library/Audio/Plug-Ins/Components/MasterLimiter.component`).
- **Measure (proven clean):** `tools/analysis/.venv/bin/python tools/analysis/hammerstein.py` → THD + phase; `sweep_isolate.py` → worst non-fundamental. Run AFTER any DSP change for before/after numbers.

## 3. Product / DSP state (v0.3.1, signal flow)
Canonical reference: **`docs/SIGNAL_FLOW.md`** (kept current). Chain in `processCore`:
clipper(opt) → input gain → **4× oversample** → 2-band split (LR @120 Hz) → per-band limiter envelopes → **Color** band-link blend → lookahead delay + apply → wideband limiter → ceiling gain → downsample → **FinalCeilingLimiter** (true-peak catch).
- **Release: two engines** (`mdsp_dsp::LimiterEnvelope`): legacy **AdaptiveSigma** (audible rate-switching) and the new **LookaheadFollower** (window-min gated recovery + fixed-time N-pole cascade — smooth, **the current winner**, confirmed by avishali).
- **Attack** = cosine ramp over the lookahead window; **DEV Attack knob overrides Character** (Character greyed out while tuning).
- **Lookahead:** two DEV windows (`dev_lookahead_band_ms`, `dev_lookahead_wide_ms`), **0.00–6.00 ms / 0.01 step**, default 5; constant-latency (padded; reported latency fixed at 6 ms max ≈ 14 ms total).
- **Color:** 0% = linked/transparent, 100% = independent/multiband (low-IMD, breaks the loudness cap). Intermediate values dip the low end (known — needs linear-phase crossover).

## 4. UI / features shipped this session
- **History Graph window** (header "Graph" button, always-on-top): per-sample-accurate GR/output/input traces, clip threshold + red clip markers, selectable dB range + 0.75–30 s scroll. Lock-free SPSC ring in the processor.
- **DEV window** (header "DEV" button): all DEV controls grouped by section (Attack / Lookahead / Release·Engine / ·Lookahead / ·Adaptive / ·Band scaling / ·Manual). No inline DEV strip.
- **User presets** (full APVTS state incl. DEV) → `~/Library/Audio/Presets/MelechDSP/MasterLimiter/*.mlpreset`; menu Save/Load-from-file/Delete/Reveal.
- **A/B compare** (header A / B / A→B): full-state scratch slots, persisted in plugin state.
- Removed dead `lookahead_ms` param; exposed hidden `release_sustain_ratio` as a DEV knob. Bypass-button text + default combo arrow fixed.

## 5. DEV controls (TEMPORARY — bake & remove for 0.4)
In the DEV window. Defaults reproduce current voicing. To find the final voicing: Engine=**Lookahead**, then sweep **Attack**, **LA Band/Wide**, **LA Release ms**, **Poles** while watching the History Graph.
`dev_release_engine` · `dev_la_release_ms` · `dev_la_release_poles` · `dev_attack_ms` · `dev_lookahead_band_ms` · `dev_lookahead_wide_ms` · `dev_sigma_attack_ms` · `dev_sigma_decay_scale` · `dev_low_band_release_scale` · `dev_high_band_release_scale` · `release_sustain_ratio`.

## 6. Measurement findings (definitive)
Offline render through the AU (latency-independent): **passthrough −119 dB, hard-limiting THD −94 dB, magnitude flat ±0.1 dB to 22 kHz, audible-band excess phase ~15°.** The Plugin Doctor "harmonic tent" avishali saw was a **measurement artifact** (latency smears its swept-sine deconvolution), NOT real audio. The "90° HF phase" is a benign linear-phase delay. So the filter-quality items (below) are *refinements*, not bug-fixes.

## 7. Release / publish (DONE for 0.3.1-beta; reuse for next)
Signed pipeline + Cloudflare publish — full detail in `release/PUBLISH.md` and memory `release-signing-aax`. Key gotchas baked into memory: **iLok** needs an *attached* dongle or *open Cloud Session* (sync alone fails: "Eden Tools License Failure"); **WCGUID = `BDE0ED80-…`** (the `75B5E420-…` is the catalog Product GUID, not the WCGUID); verify the **`-signed.pkg`** not the unsigned intermediate; `SIGN_AND_NOTARIZE_SKIP_AAX=1 sign_and_notarize_pkg.sh` skips rebuild when AAX already PACE-signed.
- **Portal now serves:** installer .pkg + **Tester Guide PDF** + **Manual PDF** + release notes (guide folded in), all Access-gated presigned downloads. Manifest fields `guide_path`/`manual_path` + endpoints `/api/download/{guide,manual}` (Worker deployed).
- Tester guide source: `docs/BETA_TESTER_GUIDE.md`. Manual source: `docs/SIGNAL_FLOW.md`.

## 8. Roadmap (after tester feedback)
- **`docs/LIMITER_TYPES.md`** — limiter "types" as *decomposition* front-ends on a shared back-end. Sequence: **#1 Dual** (fast catcher + slow leveler; fast stage is a dedicated fast limiter, NOT the clipper) → **#3 Spectral** (STFT critical-band) → **#2 content-aware adaptive** (ML stems = long-term).
- **`docs/FILTER_QUALITY_ROADMAP.md`** — keystone = **custom linear-phase FIR** → fixes Color phase (D), enables movable/3-band crossover (E), HF phase / anti-alias high-cut (F). Measure with `hammerstein.py`.

## 9. IMMEDIATE next steps
1. avishali: add 1–2 testers (portal `docs/ADD_TESTER.md`), send the portal link (point them at the **tester guide** first).
2. Collect their **`.mlpreset` files + notes**.
3. Claude: load each preset through the offline rig, read out the chosen DEV values → **bake** (constants / promote keepers to real params, map onto Auto modes), **delete all DEV params**, bump to **0.4**.
4. Then start the **filter-quality arc** (custom FIR) and/or **#1 Dual** limiter type.

## 10. Memory (auto-memory, persists)
`project-release-voicing` (current voicing/DEV state), `release-signing-aax` (signing/AAX/portal gotchas), `project-masterlimiter-slices`, `product-control-model`, `workflow-trinity`, `juce-build-gotchas`, `ui-aesthetic-direction`. In-repo refs: `docs/SIGNAL_FLOW.md`, `docs/LIMITER_TYPES.md`, `docs/FILTER_QUALITY_ROADMAP.md`.
