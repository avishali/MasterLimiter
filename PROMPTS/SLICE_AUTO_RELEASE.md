# Cursor Task — Slice (Auto-release): program-dependent release, 3 modes

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Implements the deferred auto-release (ADR-0006 earmark).** Activates the
> dead "Auto" button: when ON, release becomes program-dependent (adapts
> fast/slow to the material) and the manual Release knob greys out. A new
> `auto_release_mode` Choice selects **Transparent / Balanced / Reactive**.
> HQ `LimiterEnvelope` DSP + product param/wiring/UI + an architect-authored
> ADR. Default OFF → bench bit-identical. **Do NOT push** — architect closes.

> **Ordering:** branch off product `main` AFTER the M/S slice (7b) has closed
> (both touch MainView's control clusters). If 7b is still open, STOP and say so.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- HQ edit: `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h` +
  `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` (auto-release in the
  envelope). Edit in the **sibling** `melechdsp-hq` on a branch
  `slice-auto-release` off `master` (the product build consumes HQ from the
  sibling). Leave `MCP` untouched. The architect provides the ADR doc.
- Product edits: `Source/parameters/ParameterIDs.h`, `Parameters.cpp`,
  `Source/PluginProcessor.{h,cpp}`, `Source/ui/MainView.{cpp,h}`.
- **New frozen param** `auto_release_mode` (permanent once shipped).
- RT-safety: per-sample release adaptation must be allocation-free and cheap —
  **no per-sample `exp`** (precompute fast/slow coefficients; interpolate).
- Branch product `slice-auto-release` off `main`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `LimiterEnvelope.{h,cpp}`: `setReleaseMs`, `setReleaseSustainRatio`,
  `recomputeAlpha()` (computes `alpha_` fast + `alphaSlow_` slow from
  `releaseMs_` × `releaseSustainRatio_`), the per-sample release smoothing in
  `process()` (`stage1_/stage2_` + the dormant `stage1Slow_/stage2Slow_`), and
  `getLastBlockMaxGrDb()`.
- `Source/PluginProcessor.cpp`: where `envelope_`/`envelope_R_`/`envelopeLow_`/
  `envelopeHigh_` get `setReleaseMs`/`setReleaseSustainRatio`/`setMode` per
  block; the cached `release_ms` read.
- `Parameters.cpp`/`ParameterIDs.h`: `release_ms`, `release_auto` (exists),
  the Choice pattern (`character`, `clipper_mode`).
- `Source/ui/MainView.cpp`: `btnReleaseAuto_` (+ its `release_auto`
  attachment ~323), `sldRelease_`/the Release knob, greying patterns.

Output the retrieval log.

────────────────────────────────────────
2. PARAMETER (new frozen ID)
────────────────────────────────────────

Add ONE new frozen `AudioParameterChoice`:
- **ID:** `auto_release_mode`
- **Display name:** `"Auto Release"`
- **Choices:** `["Transparent", "Balanced", "Reactive"]`
- **Default index:** `0` (Transparent)

`release_auto` already exists (bool, default `false`). Keep it.

────────────────────────────────────────
3. DSP — program-dependent release in LimiterEnvelope
────────────────────────────────────────

Add to `LimiterEnvelope`:
- `void setAutoRelease (bool enabled) noexcept;`
- `enum class AutoReleaseMode { Transparent, Balanced, Reactive };`
  `void setAutoReleaseMode (AutoReleaseMode) noexcept;`

### Algorithm (cheap, no per-sample exp)
When auto is **off**: behaviour is **exactly as today** (use `alpha_` from
`releaseMs_`). The shipped default path must be bit-identical.

When auto is **on**, ignore the manual `releaseMs_` and instead, per mode,
precompute two release coefficients (only when SR/mode changes):
- `fastAlpha` from `fastMs`, `slowAlpha` from `slowMs` (same `exp(-1/(ms*sr))`
  form as `recomputeAlpha`). Starting values (tune in audition):
  - **Transparent:** fast ≈ 80 ms, slow ≈ 1200 ms (strong anti-pump).
  - **Balanced:**    fast ≈ 50 ms, slow ≈ 600 ms.
  - **Reactive:**    fast ≈ 20 ms, slow ≈ 250 ms (punchy recovery).

Per sample, compute a **sustain factor** `sigma ∈ [0,1]` — high when reduction
is sustained, low on isolated transients — and interpolate the **coefficient
directly** (not the ms):
```
relAlpha = fastAlpha + sigma * (slowAlpha - fastAlpha);   // sustained -> slower release
```
`sigma`: a slow one-pole on the current reduction depth (e.g. smooth
`1 - currentGain` with a ~150–300 ms time constant; deeper/longer reduction →
sigma → 1). The mode may also scale the sigma smoothing time if useful. Use
`relAlpha` in place of `alpha_` in the per-sample release update. (You may
repurpose the dormant `alphaSlow_`/`stage*Slow_` members rather than adding new
ones — your call; keep it clean.)

Net feel: transients recover fast (punch retained); sustained limiting releases
slowly (no pumping). Modes shift the fast/slow bounds.

### Product wiring
Per block, for **all** envelopes (`envelope_`, `envelope_R_`, `envelopeLow_`,
`envelopeHigh_`): read `release_auto` → `setAutoRelease(...)`; read
`auto_release_mode` → `setAutoReleaseMode(...)`. When auto is off, keep the
existing `setReleaseMs(release_ms)`.

────────────────────────────────────────
4. UI
────────────────────────────────────────

- `btnReleaseAuto_` (exists) keeps its `release_auto` attachment — now
  functional.
- Add the **Auto Release mode** selector (3-way segmented / dropdown) bound to
  `auto_release_mode`, near the Release controls (architect/avishali audit
  placement; match the LookAndFeel).
- **Greying:** Auto OFF → Release knob active, mode selector greyed. Auto ON →
  Release knob **greyed/disabled**, mode selector active. Wire the `release_auto`
  toggle to update `setEnabled` on both + repaint.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq && git checkout -b slice-auto-release
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter && git checkout -b slice-auto-release
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new warnings.
- **Default (Auto OFF) bit-identical:** Slice 3/4/5 quick PASS unchanged.
```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/SLICE_AR_S$SLICE"; done
```
- Behavioural sanity (report, not gated): with Auto ON on a drum loop, each
  mode differs (Transparent smoothest/least pumping; Reactive punchiest); no
  zipper when toggling Auto or switching modes live (smooth the coefficient
  transition / crossfade if needed); Release knob greys when Auto on.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. HQ diff (LimiterEnvelope auto-release API + algorithm).
3. Product diff (param, per-envelope wiring, UI selector + greying).
4. Build summary (Debug + Release, warnings).
5. Slice 3/4/5 quick: PASS unchanged (Auto OFF bit-identical).
6. Behavioural sanity notes (3 modes; live-toggle click test).
7. `git status --short` product (slice branch) + HQ sibling (slice branch).
8. Open questions (mode names/placement before freeze; fast/slow ms per mode;
   sigma time constant; any click on live toggle).

────────────────────────────────────────
7. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

Architect authors the auto-release ADR (3-mode program-dependent release;
reuses dual-rate coefficients). avishali auditions the three modes + the
Auto toggle/greying + default-unchanged. Tune the fast/slow ms + sigma per
mode by ear. On approval → architect closes (product + HQ + ADR + submodule
bump + PROGRESS/PLAN + prompt archive). **Do not self-close, do not write the
ADR.**
