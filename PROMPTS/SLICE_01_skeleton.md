# Cursor Task — MasterLimiter Slice 1: Product Repo Skeleton

Paste this entire file into Cursor as the task prompt. Do not edit it.
The MelechDSP system rules preamble at the top is MANDATORY and non-negotiable.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE (MANDATORY)
────────────────────────────────────────

You MUST obey `PROMPTS/00_SYSTEM_RULES.txt` from melechdsp-hq verbatim.
Headline rules:

- You are an IMPLEMENTATION ASSISTANT, not an architect.
- One prompt = one task = one focused diff.
- You may ONLY create/modify files explicitly listed in §4 (SCOPE) below.
  If you need to touch any other file, STOP and ask.
- Real-time audio safety rules apply to all audio-thread code (none in this slice).
- Engine/UI boundary: UI consumes snapshots only; no direct DSP access.
- APVTS is the single source of truth for parameters.
- Parameter IDs introduced here are FROZEN. They cannot be renamed without a
  migration plan + new ADR.
- Minimal diff. No speculative code. No "future" hooks.

────────────────────────────────────────
1. TRINITY PROTOCOL RETRIEVAL GATE
────────────────────────────────────────

Before writing ANY code, complete Trinity retrieval and output the log:

1. `melech_internal_server` — locate canonical paths in melechdsp-hq:
   - AnalyzerPro's `CMakeLists.txt` and `CMakePresets.json` pattern
   - `mdsp_ui::Theme` and `mdsp_ui::LookAndFeel` headers and how AnalyzerPro applies them
   - Any existing plugin-template helpers under `TEMPLATES/`
2. `juce_api_server` — verify:
   - `juce::AudioProcessorValueTreeState` parameter layout API for JUCE 8
   - `juce::AudioProcessorEditor` standard editor wiring
   - VST3 / AU / Standalone format registration via `juce_add_plugin`
3. `melech_dsp_server` — confirm there is nothing in `mdsp_dsp` to reuse for
   this slice (skeleton only — expected result: none).

Output the RETRIEVAL LOG in the format specified in `PROMPTS/TEMPLATE_task.txt`
before producing any diff. If any field is missing, output ONLY a Retrieval
Plan and STOP.

────────────────────────────────────────
2. TASK TITLE
────────────────────────────────────────

MasterLimiter Slice 1 — create the product repo skeleton at
`/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/` (sibling to
`AnalyzerPro/`). Plugin loads in Ableton Live, audio passes through unchanged,
all v1 APVTS parameters are declared (inert), placeholder UI uses
`mdsp_ui::LookAndFeel` and `mdsp_ui::Theme`.

────────────────────────────────────────
3. CONTEXT (read before implementing)
────────────────────────────────────────

- Architecture: `melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md`
- Spec: `melechdsp-hq/docs/products/MasterLimiter/SPEC.md`
- Slice plan: `melechdsp-hq/PROMPTS/MasterLimiter/PLAN.md`
- Reference product to mirror: `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro/`
  Mirror its CMake/presets/scripts wholesale. For `Source/` use a MINIMAL
  layout — only the directories listed in §4. Do NOT copy AnalyzerPro's
  analyzer/loudness/hardware/etc. directories.
- HQ consumption pattern (mirror AnalyzerPro exactly):
  - Git submodule at `third_party/melechdsp-hq` pointing to
    `https://github.com/avishali/melechdsp-hq.git`
  - `CMakePresets.json` sets `MELECHDSP_HQ_ROOT` to sibling `${sourceDir}/../melechdsp-hq`
    so local iteration prefers sibling, CI falls back to submodule.
- JUCE: external. Path comes from `JUCE_PATH` env var. AnalyzerPro uses JUCE 8.
  JUCE source exists locally at `/Users/avishaylidani/DEV/SDK/JUCE/`.

────────────────────────────────────────
4. SCOPE — files you may CREATE (everything is new)
────────────────────────────────────────

ALL paths below are under `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/`.
You may create exactly these files and no others. If any required file is
missing from this list, STOP and ask.

Repo root:
- `CMakeLists.txt`
- `CMakePresets.json`
- `README.md`
- `LICENSE`
- `.gitignore`
- `.gitattributes`
- `.editorconfig`
- `.clangd`
- `MasterLimiter.code-workspace`
- `scripts/build.sh`        (mirror AnalyzerPro's if it exists; otherwise minimal)
- `scripts/clean.sh`

Submodule (created via `git submodule add`, not as a file):
- `third_party/melechdsp-hq` → `https://github.com/avishali/melechdsp-hq.git`

Source tree (minimal layout, hybrid per S1-1 = c):
- `Source/PluginProcessor.h`
- `Source/PluginProcessor.cpp`
- `Source/PluginEditor.h`
- `Source/PluginEditor.cpp`
- `Source/parameters/ParameterIDs.h`
- `Source/parameters/Parameters.h`
- `Source/parameters/Parameters.cpp`
- `Source/ui/MainView.h`
- `Source/ui/MainView.cpp`

Documentation:
- `docs/.gitkeep`
- `PROMPTS/.gitkeep`

NON-GOALS in this slice:
- NO DSP code beyond a literal pass-through in `processBlock`.
- NO metering, no snapshots, no lock-free queues.
- NO presets system, no preset files.
- NO CI workflows (`.github/`). Deferred to ship-gate slice.
- NO marketing assets, no installer, no codesigning scripts.
- NO modifications to anything under `third_party/melechdsp-hq/` (submodule is read-only here).

────────────────────────────────────────
5. PLUGIN IDENTIFIERS (FROZEN)
────────────────────────────────────────

| CMake variable        | Value                                  |
|-----------------------|----------------------------------------|
| `PLUGIN_NAME`         | `MasterLimiter`                        |
| `PLUGIN_VERSION_MAJOR`| `0`                                    |
| `PLUGIN_VERSION_MINOR`| `1`                                    |
| `PLUGIN_VERSION_PATCH`| `0`                                    |
| `COMPANY_NAME`        | `MelechDSP`                            |
| `PLUGIN_CODE`         | `MaLm`                                 |
| `MANUFACTURER_CODE`   | `Melc`                                 |
| `PRODUCT_NAME`        | `MasterLimiter`                        |
| `IS_SYNTH`            | `FALSE`                                |
| `NEEDS_MIDI_INPUT`    | `FALSE`                                |
| `NEEDS_MIDI_OUTPUT`   | `FALSE`                                |
| `IS_MIDI_EFFECT`      | `FALSE`                                |

Bundle ID prefix: match AnalyzerPro's (read it from its CMakeLists.txt during
Trinity retrieval). Plugin bundle ID: `<prefix>.MasterLimiter`.

Dev-mode formats: **`VST3;AU;Standalone`** (no AAX in dev mode for this product).
Release formats: **`VST3;AU;Standalone`** (AAX deferred).

────────────────────────────────────────
6. APVTS PARAMETER LAYOUT (FROZEN IDS)
────────────────────────────────────────

Declare all of these in `Source/parameters/ParameterIDs.h` as `inline constexpr`
string-view IDs, and build the `AudioProcessorValueTreeState::ParameterLayout`
in `Parameters.cpp`. All parameters must be smoothed via host automation
mechanisms only in this slice (no audio-thread smoothing yet — that lands when
DSP arrives in Slice 3).

| ID                  | Type    | Range / Choices                       | Default     | Label  |
|---------------------|---------|---------------------------------------|-------------|--------|
| `input_gain_db`     | float   | −12.0 .. +24.0, step 0.01             | 0.0         | dB     |
| `ceiling_db`        | float   | −12.0 .. 0.0, step 0.01               | −1.0        | dB     |
| `release_ms`        | float   | 1.0 .. 1000.0, skew 0.3               | 100.0       | ms     |
| `release_auto`      | bool    | { Off, Auto }                         | Off         | —      |
| `lookahead_ms`      | float   | 5.0 .. 5.0 (locked in v1)             | 5.0         | ms     |
| `ceiling_mode`      | choice  | { SamplePeak, TruePeak }              | TruePeak    | —      |
| `stereo_link_pct`   | float   | 0.0 .. 100.0, step 0.1                | 100.0       | %      |
| `ms_link_pct`       | float   | 0.0 .. 100.0, step 0.1                | 100.0       | %      |
| `character`         | choice  | { Clean }                             | Clean       | —      |

Notes:
- `lookahead_ms` is declared as a float parameter with min==max so the host
  shows it but cannot be changed. Will widen in a later slice.
- `release_auto` is a separate bool rather than a sentinel value inside
  `release_ms`. Cleaner for automation.
- Use `juce::AudioParameterFloat` / `Bool` / `Choice` directly.
- Parameter version hint: `1`.

────────────────────────────────────────
7. UI REQUIREMENTS (placeholder, themed)
────────────────────────────────────────

- `PluginEditor` installs `mdsp_ui::LookAndFeel` (default theme, matching
  AnalyzerPro). Read AnalyzerPro's editor to see the exact install pattern.
- `MainView` is the editor's single child component, fills the bounds.
- Layout: header strip with text "MasterLimiter v0.1.0", below it a single
  row (or wrapped grid) of `juce::Slider` / `juce::ToggleButton` /
  `juce::ComboBox` widgets — one per APVTS parameter, attached via
  `SliderAttachment` / `ButtonAttachment` / `ComboBoxAttachment`.
- No custom painting beyond what `mdsp_ui::LookAndFeel` provides.
- Default editor size: 720 × 360. Resizable: no in this slice.
- Widgets must be visibly labeled (parameter name + units).

This UI exists to prove the plumbing — it is NOT the production UI. Production
UI lands in Slice 8 (meters) and a later polish slice.

────────────────────────────────────────
8. AUDIO PROCESSING (pass-through only)
────────────────────────────────────────

`processBlock`:
- Bypass on `isNonRealtime()` not required — pass-through is identical anyway.
- Iterate channels and samples: no modification. Equivalent to no-op but
  must still be a `for` loop that reads/writes each sample (so the symbol
  shows up in profiling traces from day one).
- `getLatencySamples()` returns `0` in this slice.
- `prepareToPlay` / `releaseResources`: empty bodies (no resources).
- `setBusesLayout`: support stereo-in / stereo-out only for v1. Reject mono
  and surround. (We'll widen if needed in a later slice.)

────────────────────────────────────────
9. LICENSE & README CONTENT
────────────────────────────────────────

`LICENSE` — proprietary, single short paragraph:

> Copyright (c) 2026 MelechDSP. All rights reserved.
>
> This software and its source code are the confidential and proprietary
> information of MelechDSP. No part of this software may be reproduced,
> distributed, modified, or used in any form without the prior written
> permission of MelechDSP.

`README.md` — minimal:

- One-line description: "MasterLimiter — MelechDSP mastering limiter."
- Status: "Pre-alpha. Slice 1 (skeleton)."
- Pointers to HQ docs:
  - `third_party/melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md`
  - `third_party/melechdsp-hq/docs/products/MasterLimiter/SPEC.md`
  - `third_party/melechdsp-hq/PROMPTS/MasterLimiter/PLAN.md`
- Local build commands matching `CMakePresets.json`.
- Note: GitHub remote will be added separately by the owner.

────────────────────────────────────────
10. GIT BOOTSTRAP
────────────────────────────────────────

Run, in order:

```
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git init -b main
git submodule add https://github.com/avishali/melechdsp-hq.git third_party/melechdsp-hq
git add .
git commit -m "Initial commit — MasterLimiter Slice 1 skeleton"
```

Do NOT push. Do NOT create a GitHub repo. The owner will do that manually
after gate approval.

────────────────────────────────────────
11. BUILD VERIFICATION (your responsibility)
────────────────────────────────────────

Before reporting done, verify locally:

```
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cmake --preset debug
cmake --build build-debug -j
```

Expected:
- Configure succeeds with no `FATAL_ERROR`.
- Build produces VST3, AU, and Standalone targets with zero new warnings
  attributable to this slice's code.
- No linker errors.

If the build fails for reasons outside your scope (e.g. mdsp_ui API drift),
STOP and ask. Do not patch HQ.

────────────────────────────────────────
12. GATE CHECKLIST (architect will verify)
────────────────────────────────────────

The slice is DONE only when ALL are true:

- [ ] Repo exists at `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/`.
- [ ] `git status` clean after the initial commit.
- [ ] HQ submodule resolves and points at the right URL.
- [ ] `cmake --preset debug` succeeds.
- [ ] `cmake --build build-debug` succeeds for VST3, AU, Standalone, no new warnings.
- [ ] Plugin loads in Ableton Live as VST3 (manual audition).
- [ ] Plugin loads in Ableton Live as AU (manual audition; unsigned dev OK).
- [ ] Standalone runs and shows the placeholder UI.
- [ ] Audio passes through unchanged (null test residual ≤ −140 dBFS — i.e. bit-exact).
- [ ] Reported plugin latency = 0 samples.
- [ ] All v1 parameters appear in Live's parameter list with correct ranges and defaults.
- [ ] Saving and reopening a Live project restores parameter state.
- [ ] Placeholder UI applies `mdsp_ui::LookAndFeel` (visually matches AnalyzerPro's theme).
- [ ] Diff scope matches §4 exactly — no stray files.

────────────────────────────────────────
13. OUTPUT FORMAT (what you return)
────────────────────────────────────────

1. RETRIEVAL LOG (§1 format).
2. Full list of files created with one-line purpose each.
3. The `git submodule add` and `git init` commands you ran, verbatim.
4. Output of `cmake --build build-debug` summary line (warnings, errors counts).
5. A self-check against §12 gate checklist — mark each item as
   "verified" / "needs human audition" / "blocked: <reason>".
6. Any open questions for the architect.

DO NOT produce a marketing summary. DO NOT add files beyond §4.
