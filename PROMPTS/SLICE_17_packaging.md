# Cursor Task — Slice 17: beta packaging (presets, default-state, smoothing, version)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product packaging slice for the beta.** (1) Default/init-state audit,
> (2) param-smoothing audit, (3) **factory presets via an in-UI header menu**
> (~5 presets), (4) version stamp → `v0.3.0 (beta)`. Mostly product UI +
> Parameters + a small preset manager. No HQ, no DSP-algorithm change. Branch
> off `main`. **Do NOT push.**

> **Ordering:** assumes Slice 16b has closed and is on `main`. If not, STOP.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Product edits: `Source/parameters/Parameters.cpp` (defaults only — keep all
  IDs/ranges), `Source/PluginProcessor.{h,cpp}` (smoothing + apply-preset),
  `Source/ui/MainView.{cpp,h}` (header preset menu), a new
  `Source/ui/PresetManager.{h,cpp}` (or similar), `CMakeLists.txt` (version).
- **Do NOT change parameter IDs or ranges**; only default *values* may change
  (§1). No HQ, no DSP-math change.
- Branch `slice-17-packaging` off `main`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. DEFAULT / INIT STATE AUDIT
────────────────────────────────────────

The on-insert defaults must equal the **"Default"** preset (§3). Confirm/set in
`Parameters.cpp`:
- `ceiling_db` = **−1.0** (already), `ceiling_mode` = **SamplePeak** (already),
  `limiter_active` = **true**, `character` = **Clean** (set if currently other),
  `input_gain_db` = 0, `clipper_drive_db` = 0, `clipper_mode` = Hard,
  `release_ms` = 30, `release_auto` = false, `auto_release_mode` = Transparent,
  `stereo_mode` = Stereo, `stereo_link_pct` = 100, `ms_link_pct` = 100,
  `band_color` = 50, `gain_match_auto` = false, I/O trims = 0, links = off.
- Report any default you changed. Re-run Slice 3/4/5 (the bench sets its own
  values, so defaults shouldn't affect it — confirm PASS unchanged).

────────────────────────────────────────
2. PARAM-SMOOTHING AUDIT
────────────────────────────────────────

Sweep each value control with audio running and listen for zipper noise.
Drive, Ceiling, Color, I/O gains are smoothed already; verify and **add
smoothing where a fast drag/automation audibly zips** (e.g. Clipper drive,
Output/ceiling, any gain applied per-block without a ramp). Use
`juce::LinearSmoothedValue` / per-block ramps, RT-safe. Report which controls
got smoothing added.

────────────────────────────────────────
3. FACTORY PRESETS — in-UI header menu (~5)
────────────────────────────────────────

- Add a small **PresetManager** (product) holding the preset definitions
  (name + the parameter→value map) and an `applyPreset(index)` that sets the
  APVTS parameters (via `setValueNotifyingHost` / the param pointers). Presets
  set the **processing params** below; do NOT touch `plugin_bypass`, the
  gain-match Learn/ref, or override I/O trims unless listed.
- Add a **styled preset dropdown in the header** (between the title and the
  Bypass button), matching the new LookAndFeel. Selecting a preset applies it;
  on load it shows "Default". (Optional nicety: append "*" if the user edits a
  param after selecting — skip if it complicates.)

**Preset values (starting points — avishali tunes by ear after):**

| Preset | Drive | Ceiling | Mode | Clipper drv | Clip mode | Character | Release | Auto | Auto mode | Stereo | Link | Color | 
|--------|------:|--------:|------|------------:|-----------|-----------|--------:|------|-----------|--------|-----:|------:|
| Default            | 0 | −1.0 | SP | 0  | Hard | Clean      | 30  | off | Transparent | Stereo | 100 | 50 |
| Transparent Master | 3 | −1.0 | TP | 0  | Hard | Clean      | 30  | on  | Transparent | Stereo | 100 | 50 |
| Loud & Aggressive  | 9 | −1.0 | TP | −3 | Soft | Aggressive | 30  | on  | Reactive    | Stereo | 100 | 25 |
| Gentle Glue        | 2 | −1.0 | SP | 0  | Hard | Tight      | 120 | off | Transparent | Stereo | 100 | 70 |
| Clipper Punch      | 4 | −1.0 | SP | −4 | Hard | Tight      | 30  | off | Transparent | Stereo | 100 | 40 |

(All: limiter on, M/S link 100, gain-match off, I/O 0. `Default` == the init
state from §1.)

────────────────────────────────────────
4. VERSION STAMP → v0.3.0 (beta)
────────────────────────────────────────

- `CMakeLists.txt`: set `PLUGIN_VERSION_MINOR` 1 → **3** (→ version `0.3.0`).
- `Source/ui/MainView.h` line ~140: `headerMode_` text `"v0.2 - Maximizer"` →
  **`"v0.3.0 (beta) - Maximizer"`**.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout -b slice-17-packaging
cmake --build build-debug -j && cmake --build build-release -j
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/SLICE17_S$SLICE"; done
```
- Zero new `Source/` warnings. Slice 3/4/5 PASS.
- In host: header shows `v0.3.0 (beta)`; preset dropdown lists the 5, selecting
  each applies sensibly + no clicks; fresh insert = Default; no zipper on any
  control sweep.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log. 2. Diffs: defaults changed, smoothing added, PresetManager +
   header menu, version. 3. Build summary + Slice 3/4/5 PASS. 4. Confirm no
   param ID/range changes. 5. `git status --short`. 6. Open questions (preset
   value tweaks, menu placement).

────────────────────────────────────────
7. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali auditions: preset menu placement/look, each preset's sound (tune the
values by ear), no zipper, version shows. Iterate preset values. On approval →
architect closes Slice 17, then drafts the **user manual**, then we cut the
**beta build**. **Do not self-close.**
