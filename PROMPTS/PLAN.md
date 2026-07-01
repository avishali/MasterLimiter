# MasterLimiter — Slice Plan

**Owner:** avishali (architect: Claude / coder: Cursor)
**Architecture:** [ADR-0004 (in HQ)](../third_party/melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md)
**Spec:** [SPEC.md](../docs/SPEC.md)
**Progress log:** [PROGRESS.md](../docs/PROGRESS.md)

## Slice gate (applies to every slice)

A slice is **done** only when **all four** are true:

1. Builds clean on macOS, no new warnings.
2. Slice-specific bench criteria pass (see each slice's "Gate").
3. avishali auditions the result and approves.
4. Claude (architect) signs off on diff scope and RT-safety.

If a slice fails its gate, we do not proceed. We either patch within the same
slice or split a follow-up slice.

## Workflow per slice

1. **Interview** — Claude asks scope-specific questions for this slice.
2. **Slice prompt** — Claude writes `PROMPTS/SLICE_NN_*.md`
   following the SDK Cursor-task format (Trinity Protocol preamble + scoped
   file list + non-goals + RT-safety reminders + test plan).
3. **Cursor implements** — only the files listed.
4. **Validation** — bench run + avishali audition.
5. **Gate** — pass or iterate. On pass, PROGRESS.md updated.

## Slice list

| #  | Title                                                  | Touches (HQ)                                | Touches (product repo) | Gate (headline)                                                |
|----|--------------------------------------------------------|---------------------------------------------|------------------------|----------------------------------------------------------------|
| 0  | Architecture + plan (this doc)                         | docs only                                   | —                      | Avishali approves ADR-0004, SPEC, PLAN.                        |
| 1  | Product repo skeleton @ `../MasterLimiter/`            | —                                           | new sibling repo       | Plugin loads in Live, audio passes through, params declared.   |
| 2  | Null-test bench harness                                | new `shared/limiter_bench/`                 | —                      | Bench can null any pass-through with residual ≤ −120 dBFS.     |
| 3  | Lookahead + sample-peak single-band limiter            | `mdsp_dsp/LookaheadDelay`, `LimiterEnvelope`| wire-in                | Clean 3 dB GR on pink noise + drum loop per §5 criteria.       |
| 4  | True-peak detector + 4× oversampler on ceiling         | `mdsp_dsp/TruePeakDetector`, `Oversampler`  | TP mode toggle         | Zero TP overs at ceiling −1 dBTP across corpus.                |
| 5  | Transient/sustain split (ADR-0005 decision)            | `mdsp_dsp/TransientSustainSplit`            | —                      | Clean 5 dB GR on drum loop + dense mix per §5 criteria.        |
| 6  | Soft-knee sub-audible saturator                        | `mdsp_dsp/SoftKneeSaturator`                | —                      | THD+N delta within bounds at 7 dB GR push.                     |
| 7  | ✅ **Shipped — Stereo Link % + 2ch GR meter + envelope SIMD** | ADR-0008; `LimiterEnvelope` SIMD ramp loop | `stereo_link_pct`, per-channel envelopes, 2ch GR meter, latched max readouts | Shipped 2026-05-29. Per-channel parallel envelopes with continuous Link % blend (frozen ID `stereo_link_pct`, default 100% = bit-exact fast-path). 2ch GR meter + latched-max readouts. NEON SIMD inner ramp loop (~4× speedup, resolves heavy-GR underrun). Constant latency 301/301. Bench Slice 3/4/5 PASS 13/13, 14/14, 25/25 unchanged. ADR-0008. M/S deferred. Multiband detection promoted as next architectural lever for "open" gap and CPU↔GR correlation parity with Ozone. |
| 7b | ✅ **Shipped — M/S mode** | ADR-0008 addendum | `stereo_mode`, active `ms_link_pct`, single Link knob | Shipped 2026-05-30. New frozen `stereo_mode` Choice (`Stereo`/`M/S`, default Stereo). Wideband final stage now runs the unlink in either existing L/R domain (`stereo_link_pct`) or lossless Mid/Side domain (`ms_link_pct`), reusing the same two wideband envelopes and `lookaheadWide_` with no added latency. UI exposes one Link knob re-targeted by mode, with per-mode value recall, plus a click Stereo/M-S toggle. GR meter reports the active components. Slice 3/4/5 PASS unchanged at default. |
| 7b.2 | ✅ **Shipped — M/S ceiling hotfix** | `dsp_bench` M/S overs test | M/S apply branch safety bound | Shipped 2026-05-30. Hotfix for shipped 7b M/S mode: with Link < 100%, independently limited Mid and Side could decode back to L/R above the ceiling. The M/S apply branch now applies a per-sample decoded-L/R safety gain (`thresholdLin/peak`) to both output channels and folds it into GR metering. Stereo path untouched. New M/S bench at Link 0 / ceiling 0 fails before (`SP 2`, `TP 8`) and passes after (`SP 0`, `TP 0`). |
| 8  | Meters (GR + TP) to mdsp_ui                            | `mdsp_ui/GainReductionMeter`, `TruePeakMeter`| UI assembly           | Meters track snapshots accurately, no UI-thread DSP access.    |
| 9  | ✅ **Shipped — Limiter character + Clipper Drive + on/off** | `LimiterEnvelope::Mode`; `dsp_bench` recal + Ozone reference driver | `limiter_active`, `clipper_drive_db`, expanded `character`, 4× OS limiter chain, TP envelope headroom | Shipped 2026-05-29. 3 Character modes, Clipper Drive stage, limiter on/off, 4× OS limiter chain, ispTrim removed, TP headroom in envelope. Bench Slice 3/4/5 PASS via treatment-B recalibration; Ozone IRC IV reference run documents the in-family character. ADR-0006 + ADR-0007. |
| 10 | Maximizer UI shell (Ozone-inspired two-panel layout)   | —                                           | `ui/MainView`          | Look-lock: two-panel layout, drive Gain at 0 dB hard-left, placeholders for new controls. Audition the look. |
| 11 | ✅ **Shipped — I/O gains + dual Gain-Match (Auto/Track + Learn + Bypass-with-match)** | ADR-0007 + Addendum 2026-05-29 | 8 new params + 1 hidden ValueTree state + DSP + UI | Family shipped 2026-05-29 → 2026-05-30. **11a** (✅): I/O Input/Output trims (independent L/R + link bools), positive-only Drive (0..+24 dB), Ceiling (0..−24 dB). **11b1** (✅): Gain⇄Ceiling structural Link (control coupling, no DSP). **11b2** (✅): `gain_match_auto` + hidden `learned_ref_lufs`; two new `LoudnessAnalyzer` instances at post-Input-Gain and post-Limiter; Learn arm-and-capture (3 s short-term snapshot); Track loop (clamp(ref−live, ±12 dB) at τ ≈ 1 s); compensation gain between limiter and Output. **11b2.1** (✅): `plugin_bypass` as JUCE host bypass param via `getBypassParameter()`; matched-bypass hardened (IO trims always survive); in-UI Bypass label+colour feedback; Learn relocated next to Auto/Track. **11b2.2** (✅): bypass-transition click fix — always-running limiter chain + `juce::dsp::DelayLine` sized to `baseLatencySamples_` + `LinearSmoothedValue` 5 ms crossfade between live and dry-aligned signals. Bench Slice 3/4/5 PASS unchanged across the entire family. Audition note in PROGRESS.md: Ableton's green Device Activator LED is a host-side track enable, not bound to VST3 bypass params — in-UI Bypass button is canonical. |
| 12 | ✅ **Shipped — Clipper gain-staging + Hard/Soft modes + threshold semantics + drive-attributable meter (ADR-0010)** | ADR-0010 (incl. §3 revision + §1b addition) | new `clipper_mode` param; range flip on `clipper_drive_db`; chain refactor; UI relocation | Shipped 2026-05-30. Drive (`input_gain_db`) moves AFTER the clipper inside the OS region — clipper input is now post-IO-Input only, independent of Drive and Ceiling. Removed the `if (clipper_drive_db > 0)` gate. New `clipper_mode` Choice (`Hard`/`Soft`, default `Hard`). Soft curve uses `tanh`-knee at `k = 0.891` (≈ −1 dBFS) asymptoting to ±1.0; Hard branch is the Slice 9 `clamp(±1.0)` exactly. Threshold-style semantic added mid-slice (12.2): `clipper_drive_db` range flipped to `[−12, 0] dB` with self-compensating math (`g = decibelsToGain(−drive)`, `out = curve(x·g) / g`) so peaks pull down to threshold while body level is preserved. Drive-attributable meter formula reports per-block max curve attenuation depth, unified for both modes. UI relocated to sit between Ceiling and GR meter at intermediate size. Bench Slice 3/4/5 PASS 13/13, 14/14, 25/25 unchanged at defaults (Hard + drive 0 + signals below ±1.0 → identity). Slice 11b1 Gain⇄Ceiling Link re-auditioned: math unchanged, audible feel approved post-chain-change. |
| 13 | ✅ **Shipped — LUFS calibration (BS.1770-4 K-weighting fix)** | HQ `LoudnessAnalyzer` | (submodule bump only) | Shipped 2026-05-30. Fixed two K-weighting coefficient bugs in `mdsp_dsp::LoudnessAnalyzer`: Stage 1 high-shelf 1500 → 1681.974 Hz; Stage 2 RLB high-pass Q 0.707 → 0.5003 (and 38.0 → 38.135 Hz), both per BS.1770-4 §A. Prior ~1 LU offset vs WLM / Insight 2 eliminated; real-world agreement within ~0.2 LU. Synthetic 1 kHz-sine residual −0.22 LU accepted (bilinear warping, worst-case probe). No ADR. No product source change. |
| 14 | ✅ **Shipped — 2-band multiband limiting + Color control (ADR-0009)** | ADR-0009 + `dsp_bench` cross-band IMD / pumping diagnostics / default criteria recal | 2-band LR4 limiter in 4× OS region; serial two-lookahead ceiling; `band_color` param + Color knob; total-GR meter | Shipped 2026-05-30. Two-band LR4 @ 120 Hz with 2 dB band headroom makes the bands primary and the existing limiter a final wideband brickwall. Serial two-lookahead topology guarantees the ceiling; latency 301 → 541 samples. New frozen `band_color` (`0..100 %`, default 50 %) maps Glued/Balanced/Open to band-link 0.5→0.0 and is smoothed per sample. GR meter now reports total band × wideband reduction. Bench Slice 3/4/5 PASS 13/13, 14/14, 25/25 at default Color after ADR-0009 treatment-B recalibration. Audition approved: de-pumping competitive with Ozone, slightly brighter. |
| 15 | ✅ **Shipped — Meter ballistics (GR + Clip)** | — | GR + Clip meter ballistics | Shipped 2026-05-30. GR meter now uses instant attack / ~300 ms release and per-channel peak-hold (~1 s hold, ~12 dB/s falloff). Clip readout is smoothed and the LED holds/fades instead of flickering. Reuses HQ `mdsp_ui::meters::MeterBallistics` + `PeakHoldModel`; no product-local duplicate helper. UI-only; no audio/param/bench change. |
| 15b | ✅ **Shipped — I/O meter features** | `mdsp_ui::meters::MeterScaleMode::Top48Db` | RMS snapshots + I/O meters | Shipped 2026-05-30. I/O meters now have read-only RMS measurement with always-visible peak+RMS numerics, light-blue sample-peak bars, optional RMS-colour overlay via shared default-OFF `RMS` toggle, shared range +/- stepping Full → 48 → 24 → 12 → 6 dB, click-to-reset peak holds, one shared centre dB scale, instant-attack / ~40 dB/s display release, and -120 dB measurement floor so bars empty at silence. I/O trim faders use the darker #1D1D37 handle tone. GR release tightened 300 → 50 ms. Meters are now complete: GR ballistics + full Ozone-style I/O meter. Slice 3/4/5 PASS unchanged. |
| Auto | ✅ **Shipped — Auto-release (ADR-0011)** | ADR-0011; `LimiterEnvelope` auto-release modes | `release_auto`, `auto_release_mode`, UI enable/disable wiring | Shipped 2026-05-30. New frozen `auto_release_mode` Choice (`Transparent`/`Balanced`/`Reactive`, default Transparent); existing `release_auto` bool now functional. `LimiterEnvelope` uses per-mode precomputed fast/slow release coefficients blended per-sample by sustain factor, with no per-sample `exp`, applied to all four envelopes. Auto OFF remains the default and Slice 3/4/5 PASS unchanged. UI greys the manual Release knob when Auto is on and enables the mode selector. Per-mode constant fine-tuning deferred post-beta. |
| 16 | ✅ **Shipped — UI/UX interaction** | — | UI interaction polish | Shipped 2026-05-30. UI-only pass: Ozone-style double-click type-in with unit-aware clamp, tooltips, label clarity, hidden Lookahead + T/S controls (frozen params retained at defaults), Clipper Hard/Soft toggle, Color below Clipper, Imaging compact + moved left clear of Color, Gain-Match centered footer, SP/TP + Gain⇄Ceiling Link + Limiter On fitted between Gain and Ceiling, `100%%`→`100%`, and clip-ballistics free functions split into `Source/ui/meters/ClipBallistics.{h,cpp}`. Slice 3/4/5 PASS unchanged. |
| 16b | ✅ **Shipped — Visual restyle** | — | UI visual polish | Shipped 2026-05-31. Visual pass per `design/ui_direction_v1.html`: refined clean/dark/teal palette, button redesign with clear on-state, Limiter power button, horizontal Ozone-style link icons, segmented selectors for Clipper/Stereo/Character/Auto-release mode, LUFS box restyle, knob/fader refinement, clean meter scale/readouts, and max-peak readout latch. UI-only; no DSP/param/audio/HQ change. Slice 3/4/5 PASS unchanged. |
| 17 | ✅ **Shipped — Beta packaging / presets / default state** | — | packaging + defaults | Shipped 2026-05-31. Product-only beta packaging: default/init state now matches the "Default" preset (`character` Clean, I/O links off, ceiling −1.0 dB, SamplePeak, limiter on), 5 factory presets via an in-UI header menu (`PresetManager`), `LinearSmoothedValue` smoothing on Drive/Ceiling/Clipper drive/I/O gains, and version/header moved to `0.3.0` / `v0.3.0 (beta) - Maximizer`. No parameter ID/range changes, no HQ/submodule bump. Slice 3/4/5 PASS unchanged. |
| 18 | ✅ **Shipped — Distribution tooling** | — | signing/build scripts/config/docs | Shipped 2026-05-31. Conditional AAX in CMake: SDK-less builds stay `AU;VST3;Standalone`; setting `AAX_SDK_PATH` enables AAX via `juce_set_aax_sdk_path`. Ported macOS sign/notarize, PACE wraptool, build, installer, and release orchestration scripts from AnalyzerPro. Real `scripts/.env` and `scripts/.aax_wraptool.env` stay gitignored; only templates/docs are committed. AAX Product GUID = `75B5E420-5C80-11F1-9221-00505692C25A` (catalog product GUID; verify whether it doubles as the wraptool wrap-config GUID). Manual shipped. |
| Dev | 🔶 **Implemented — Lookahead release A/B** | `LimiterEnvelope` | 3 dev params + DEV RELEASE strip | Added a temporary `Adaptive`/`Lookahead` release-engine switch. Lookahead mode gates release against the minimum gain visible in the lookahead window and recovers through a fixed-time 2..4 pole cascade. Default remains Adaptive for existing behavior. Release build clean and AU/VST3 installed 2026-06-09; audition picks LA Release + Poles before baking constants and removing dev params for `0.4`. |
| Dev | 🔶 **Implemented — History graph window** | — | processor history ring + separate graph window | Added a fixed 4096-frame, ~2 ms cadence SPSC metering ring for GR/output/input history plus a non-modal Pro-L-style graph window opened by a header `Graph` button. Release build clean and AU/VST3 installed 2026-06-09; audition pending. |
| Dev | 🔶 **Implemented — History graph v2** | — | per-sample history capture + graph controls + clip overlay | Upgraded the graph to a 65536-frame, ~0.5 ms cadence ring with per-sample GR/output/input/clip capture, 0.75..30 s windows, level-floor and GR-range selectors, clip threshold/clip markers, larger default window, and clip LED repaint fix. Release build clean, AU/VST3 installed, and AU validation clean 2026-06-09; audition pending. |
| Dev | 🔶 **Implemented — Lookahead time DEV knobs** | `LimiterEnvelope` active window | constant-latency band/wide lookahead + DEV strip | Added runtime active lookahead windows to SDK `LimiterEnvelope`; plugin now reports fixed latency for the 12 ms max window and pads the wet path while `dev_lookahead_band_ms` and `dev_lookahead_wide_ms` tune the per-band and wideband stages independently. Removed vestigial `lookahead_ms`. Exposed LA Band, LA Wide, and Sustain Ratio in the DEV strip. SDK committed/pushed first 2026-06-10; plugin build/install/AU validation clean. |
| Dev | 🔶 **Implemented — Attack DEV knob** | `LimiterEnvelope` attack override | `dev_attack_ms` + DEV strip | Added a temporary `dev_attack_ms` override that drives all low/high/wide envelopes directly and clamps to the active LA Band/Wide window. Character remains wired but is greyed out/inert while attack is tuned. SDK committed/pushed first (`0434a17`); plugin Release build/install/AU validation clean 2026-06-10; audition pending. |
| Dev | 🔶 **Implemented — Lookahead trim + fine increments** | — | max lookahead + `dev_lookahead_*` ranges | Trimmed constant-latency max lookahead from 12 ms to 6 ms and changed LA Band/Wide to 0.00..6.00 ms with 0.01 ms steps and 5 ms defaults. Existing process clamp maps 0.00 ms to one OS sample. Release build clean, AU/VST3 installed including system VST3, and AU validation clean 2026-06-10; audition pending. |
| Dev | 🔶 **Implemented — Attack mode Ramp vs Real** | `LimiterEnvelope` AttackMode | `dev_attack_mode`, `dev_real_attack_ms` | SDK attack modes: Ramp (cosine-in-lookahead, default) vs Real (decoupled 2-pole TC; slow = transient punch-through). Plugin params + DEV UI. SDK commit first. Build/AU validation pending. |
| Dev | 🔶 **Implemented — DEV controls dock panel** | — | `DevPanel` dock in editor | DEV tuning controls dock beside the main view: editor grows ~360 px on open, panel in the right strip, no scrim — meters/GR stay visible while tuning. Still embedded in the AAX-hosted hierarchy (no separate window). History Graph unchanged. UI-only. Build/install/AU validation pending. |
| Dev | 🔶 **Implemented — DEV controls embedded panel (AAX fix)** | — | `DevPanel` overlay in editor | Moved temporary DEV tuning controls from a separate `DocumentWindow` into an in-editor overlay panel toggled by the header **DEV** button (with scrim + close ✕). Fixes AAX/Pro Tools param commit — detached windows are not host-managed. **Superseded by dock panel slice.** History Graph stays a separate read-only window. UI-only; no DSP/param changes. |
| Dev | 🔶 **Implemented — DEV controls window** | — | `DevControlsComponent` + header DEV window | Moved the temporary Attack/Lookahead/Release tuning controls out of the main view and into a separate always-on-top, resizable DEV window opened from the header. Reused the History Graph `DocumentWindow` ownership/deferred-close pattern. Main view no longer shows the inline orange DEV strip. **Superseded by embedded panel slice (AAX fix).** Release build clean, AU/VST3 installed including system VST3, and AU validation clean 2026-06-10; audition pending. |
| Dev | 🔶 **Implemented — User presets** | — | full-state `.mlpreset` save/load/delete/reveal | Added user presets under `~/Library/Audio/Presets/MelechDSP/MasterLimiter`, saved as full APVTS XML snapshots so every DEV voicing parameter round-trips for A/B comparison. Factory presets remain curated subsets. Header menu now has Factory/User sections plus Save/Delete/Reveal actions and a small Save button. Release build clean, user AU/VST3 + system VST3 installed, and AU validation clean 2026-06-10; system AU copy blocked by stale root-owned component; audition pending. |
| Dev | 🔶 **Implemented — Preset A/B + header polish** | — | A/B APVTS snapshots + Load-from-file + header L&F | Added persistent full-state A/B compare slots in the processor wrapper state, header A/B/copy controls, async Load from file rooted at the user preset folder, JUCE stock combo arrow rendering, and explicit Bypass text drawing with a wider header button. Release build clean, user AU/VST3 + system VST3 installed, and AU validation clean 2026-06-10; system AU copy blocked by stale root-owned component; audition pending. |
| Dev | ✅ **Build/AU — DEV clamp toggles + metering** | — | `dev_ms_safety_clamp` / `dev_final_ceiling` + readouts | Diagnostic DEV instrument for M/S density investigation. Both default On (bit-identical). Audition pending. |
| Dev | ✅ **Build/AU — FinalCeiling meter fix** | — | `getLastBlockMaxReductionDb()` readout | Replaced bogus pre/post buffer peak diff (lookahead offset) with SDK applied-reduction accessor. Proof test + Color-100 audition pending. |
| Dev | ✅ **Build/AU — I/O meter readouts polish** | — | labels + MAX hold + numeric ballistics + GR frame | TP/SP/MAX/RMS captions; processor-latched max sample-peak; slower SP/RMS readout decay; GR panel matches I/O. Audition pending. |
| Meter | ✅ **Build/AU — M1 estimator unification** | — | one source per quantity + floor −120 | SP=bar peak; MAX line=number; RMS direct; no stale 1 s holds. Audition pending. |
| Dev | 🔶 **Implemented — UI widen + relayout (post-A2)** | — | `PluginEditor` + `MainView` layout | Canvas 1100→1172; clipper knob 100×100; GR meter 198px wide; right cluster +72px. UI-only. Build/AU + audition pending. |
| Dev | 🔶 **Implemented — Per-band stereo unlink (Slice A2)** | — | per-band L/R DSP + DEV Band Stereo Link + L/R sub-bars | Stereo-mode per-channel band path when `dev_band_stereo_link_pct` &lt; 100%; default 100% and M/S unchanged. GR meter shows L/R sub-bars per band in existing 88px footprint. Build/AU validation + audition pending. |
| Dev | 🔶 **Implemented — Per-band GR visualization (Slice A)** | — | per-band GR atomics + GR meter + history traces | Metering-only pass for the 3-band arc: taps post-Color `gLowOut`/`gHighOut`, publishes N=3 atomics and `HistoryFrame` band fields (Mid reserved at 0). GR meter shows LO/MID/HI columns instead of L/R; history graph adds band traces. No DSP/audio/param/latency change. Build/AU validation + audition pending. Companion: `SLICE_3BAND_DSP.md` (Slice B fills Mid). |
| Beta | **Remaining — signed/notarized beta build** | — | release artifacts | avishali builds AAX with the SDK, registers/signs it with PACE, tests in Pro Tools, then cuts the signed + notarized beta build (VST3 + AU, plus AAX once signed) for the tester. |

Note: Auto-release shipped 2026-05-30 (ADR-0011). The dead-control sweep
has turned M/S and auto-release into real features, Slice 16 hid the remaining
Lookahead + T/S controls while preserving frozen defaults, Slice 16b put the
visual look in place, Slice 17 packaged the beta defaults/presets/version, and
Slice 18 shipped distribution tooling plus the beta manual. Temporary
lookahead/release/attack DEV controls are installed for audition in an embedded
editor dock (header **DEV** button grows the editor and docks controls on the right),
with Character greyed out while `dev_attack_ms` overrides
envelope attack. The lookahead trim pass lowers the max window to 6 ms and makes
LA Band/Wide fine from 0.00 ms for transient tuning. After avishali picks Attack,
LA Release + Poles, and the lookahead times, bake constants and remove the dev
params/window for `0.4`. The history graph window has a v2 accuracy/range pass
for release/time-constant audition. User presets now save full APVTS snapshots,
including all DEV params, so audition voicings can be stored and compared before
the 0.4 bake. Header A/B slots provide the fast scratch comparison path for two
live full-state voicings, while named user presets remain long-term storage.
Remaining to beta: avishali builds/signs/registers AAX, tests in Pro Tools, then
cuts the signed + notarized tester build. STFT "Max Transparency" remains the
backlog path to fuller Ozone parity if real-world use demands it. Slice 6
(saturator) remains backlogged.

## Backlog

| Status | Title | Touches (HQ) | Touches (product repo) | Rationale |
|--------|-------|--------------|------------------------|-----------|
| Backlog | STFT "Max Transparency" multiband / spectral path | ADR-0009 R4 / Alternatives | TBD | Fuller Ozone-parity path if real-world use demands more transparency than the shipped 2-band Color design. Kept out of Slice 14 because the audition-approved 2-band design is simpler, deterministic, and beta-ready. |
| Backlog | Auto-release mode fine-tuning | `mdsp_dsp::LimiterEnvelope` | auto-release mode constants | Transparent/Balanced/Reactive constants are approved for beta; final per-mode tuning deferred post-beta. |
| Backlog | Final inter-sample-peak-safe ceiling stage | TBD | final post-chain ceiling guard | Transparent final ceiling stage covering M/S decode overshoot, clipper inter-sample peaks, and post-gain overs. Slice 7b.2 fixes the shipped M/S overshoot directly; this remains future polish for one unified final guard. |
| Backlog | Palette-injection centralization | — | `PluginEditor.cpp` / UI theme wiring | Colours are currently applied product-local across the UI files for the Slice 16b restyle. Centralize via one `UiContext` / theme override after beta to prevent drift. |
| Backlog | Full bespoke knob/slider redraw | — | UI controls / look-and-feel | Slice 16b was the light visual refinement. Deeper custom knob and slider art is deferred until after beta. |
| Backlog | HQ: consolidate dual `mdsp_ui` meter systems | `mdsp_ui/meters/MeterTypes.h`, `MeterRenderState.h` | none | `MeterTypes.h` and `MeterRenderState.h` both define `mdsp_ui::meters::MeterRenderState`, colliding in any translation unit including both. Slice 15 needed a firewall around the HQ ballistics headers; post-beta HQ cleanup should merge the old/new meter contracts. |
| Backlog | Envelope smoothing-stage SIMD | `mdsp_dsp::LimiterEnvelope` | none | The inner ramp loop is NEON-vectorized as of Slice 7.1.4.1. The per-sample smoothing cascade (s1/s2 + T/S s1s/s2s for Clean) remains scalar. If a future regression surfaces the smoothing as a hot path, vectorize. Low priority. |
| Backlog | Envelope snap-event smoother (was proposed Slice 9.6b) | `mdsp_dsp::LimiterEnvelope` | none | Demoted from Slice 9 close per Ozone evidence: only ~2 percentage-point IMD lever vs the 4–7 percent gap to in-family numbers. Revisit if a different future motivation surfaces. |
| Backlog | Optional SP-mode envelope headroom (parity with TP) | — | product `processBlock` | Slice 9 SP mode accepts small 1× SP overs from OS downsample ringing; TP mode bounds via 0.3 dB headroom. If users want bulletproof SP output, add a tiny headroom in SP too. Low priority. |

## Repository locations

- **Product repo (this repo):** `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/`
  — sibling to `AnalyzerPro/`.
- **HQ (consumed via submodule):** `third_party/melechdsp-hq/`
  — also lives locally at `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/`
  (presets prefer the sibling path for fast iteration).

## Documentation locations

- Architecture (ADRs): HQ at `docs/DECISIONS/` — ADR-0004 already, ADR-0005/0006 to come.
- Product spec: this repo at `docs/SPEC.md`.
- Progress log: this repo at `docs/PROGRESS.md`.
- Cursor slice prompts: this repo at `PROMPTS/SLICE_NN_*.md`.
- Bench artifacts (from Slice 2 onward): this repo at `docs/bench/SLICE_NN/`.

## Conventions

- Every Cursor prompt is prefixed with the mandatory
  `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt` preamble and the
  Trinity Protocol retrieval gate from `TEMPLATE_task.txt`.
- Each slice prompt names its files exhaustively. Cursor must STOP if scope
  expands.
- Parameter IDs, once introduced, are frozen. Any change requires a migration
  plan + ADR per system rules.
