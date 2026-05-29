# Cursor Task — Slice 9: Character (Clean/Tight/Aggressive) + Clipper Drive + limiter on/off (ADR-0006)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Touches HQ (one ADR + LimiterEnvelope commit) and product repo
> (params + processBlock + UI).** Introduces TWO NEW FROZEN params
> (`limiter_active` bool, `clipper_drive_db` float) and expands the
> existing `character` choice from 1 option to **3** (Clean/Tight/
> Aggressive). Per ADR-0006. Constant latency across all
> mode/clipper combos. Branch off `main`, build, run bench regression,
> **do NOT push** — architect handles push + close.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Files in §4 only. No AnalyzerPro edits.
- **New params (FROZEN from this slice):**
  - `limiter_active` — bool, default `true`.
  - `clipper_drive_db` — float, range `0.0 .. +12.0`, step `0.01`,
    default `0.0`, label `"dB"`.
- **Existing `character` choice param**: option list expands from 1
  entry to **3** — `"Clean"`, `"Tight"`, `"Aggressive"`. Default
  index changes from 0 to **1 (Tight)**. ID stays `character`.
- No range/default changes on any *other* param.
- RT-safety: per-block reads only, no allocations or locks in
  `processBlock`. The clipper has a fast-path when
  `clipper_drive_db == 0` (the default) so default behaviour is zero
  added cost.
- Branch `slice-09-character-and-onoff` off `main`. **Do NOT commit
  the product repo** until the architect's close prompt. The HQ
  ADR + DSP commit IS made in §3.2 as a single HQ-side commit.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

avishali's Slice 11b1 audition: limiter at defaults feels like a slow
compressor, not a brickwall. Also wants more loudness without
artifacts (the modern-maximizer clipper-before-envelope stack) and a
distinct limiter on/off A/B toggle.

ADR-0006 (sibling-HQ path,
`/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/docs/DECISIONS/ADR-0006-masterlimiter-character-and-onoff.md`)
locks the design. **Read ADR-0006 in full before editing**, especially
§§Decision 2 (Character modes), 3 (Clipper Drive stage), 5 (DSP
placement), and 7 (Bench impact).

Important: HQ docs live in the **sibling checkout**, NOT under
`third_party/melechdsp-hq` (that's the older submodule pin and
**does not contain ADR-0006**). See [[juce-build-gotchas]] equivalent
in `memory/juce-build-gotchas.md`.

────────────────────────────────────────
2. TRINITY (lightweight)
────────────────────────────────────────

Read:
- The ADR at the sibling path above.
- HQ DSP: `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h`
  + `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`.
- Product: `Source/parameters/ParameterIDs.h`,
  `Source/parameters/Parameters.cpp`, `Source/PluginProcessor.{h,cpp}`,
  `Source/ui/MainView.{h,cpp}`.

Output the retrieval log including the HQ-from-sibling note.

────────────────────────────────────────
3. WHAT TO BUILD
────────────────────────────────────────

### 3.1 HQ — `mdsp_dsp::LimiterEnvelope::Mode`

Add a mode enum + setter:

```cpp
enum class Mode
{
    Clean,      // existing Slice 5 behaviour (T/S split, attack ramp = lookahead)
    Tight,      // brickwall: ~1 ms attack, T/S off
    Aggressive  // very tight: ~0.3 ms attack, T/S off
};

void setMode (Mode m) noexcept;     // safe to call per block
Mode getMode() const noexcept;
```

Implementation:
- Internal `attackSamples_` derived per mode at `prepare` time
  (using the prepared `sampleRate_`):
  - **Clean**: `attackSamples_ = lookaheadSamples_` (existing).
  - **Tight**: `attackSamples_ = max(1, round(0.001 * sampleRate_))`
    (~1 ms).
  - **Aggressive**: `attackSamples_ = max(1,
    round(0.0003 * sampleRate_))` (~0.3 ms).
- **The lookahead window stays the same in all modes** — only the
  attack ramp shape changes within that window. The pre-ramp portion
  of `ext_` stays at `1.0` (no gain reduction until the active ramp
  begins). This guarantees constant reported latency.
- T/S split:
  - **Clean**: existing `min(s2_fast, s2_slow)` combine.
  - **Tight** / **Aggressive**: collapse to single-band. Simplest:
    write `s2s = 1.0f` per sample so `min(s2, s2s) == s2` always (the
    slow cascade state still ticks but is unused).
- The `setMode` call is cheap (just sets the mode and recomputes
  `attackSamples_` from the cached sample rate); safe to call every
  block from the product processor.
- Mode changes mid-stream should NOT glitch the envelope state.
  Don't reset `stage1_/stage2_` etc on `setMode` — they're already
  near-1.0 at low GR and will adapt naturally. If a small bump is
  unavoidable, accept it; mode is a user-gesture param, not
  automation-rate.
- Attack table sizing: at `prepare`, allocate the attack table at
  `lookaheadSamples_` (max needed). When the mode resolves
  `attackSamples_` ≤ `lookaheadSamples_`, populate the first
  `attackSamples_` entries with the raised-cosine ramp; index into
  them in `process` accordingly. No runtime allocation.

### 3.2 HQ commit (separate, no push)

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# expect:
#    M docs/DECISIONS/ADR-0006-masterlimiter-character-and-onoff.md
#    M shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h
#    M shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp

git add docs/DECISIONS/ADR-0006-masterlimiter-character-and-onoff.md \
        shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h \
        shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp

git commit -m "$(cat <<'EOF'
ADR-0006 + LimiterEnvelope::Mode (Clean/Tight/Aggressive)

Locks the MasterLimiter Slice 9 character decision (3 stepped envelope
modes via Character, separate Clipper Drive pre-envelope stage at the
product layer, limiter on/off as a product-level toggle, constant
latency across all combos).

LimiterEnvelope gains setMode(Mode): Clean preserves the existing
Slice 5 behaviour (T/S split, attack ramp = lookahead window); Tight
uses a ~1ms hard attack with T/S off; Aggressive uses ~0.3ms attack
with T/S off. All modes share the same lookahead so latency is
constant. Default Mode at prepare is Clean (preserves Slice 3/4/5
bench regression). The Clipper Drive is product-side, not an envelope
mode.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Do NOT push HQ.

### 3.3 Product — branch + params

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout main && git status --short        # clean
git checkout -b slice-09-character-and-onoff
```

In `ParameterIDs.h`, add two new IDs:

```cpp
inline constexpr std::string_view limiter_active     { "limiter_active" };
inline constexpr std::string_view clipper_drive_db   { "clipper_drive_db" };
```

In `Parameters.cpp::createParameterLayout()`:

- Add the bool:
  ```cpp
  layout.add (std::make_unique<juce::AudioParameterBool> (
      pid (limiter_active, 1),
      "Limiter Active",
      true,
      juce::AudioParameterBoolAttributes()));
  ```
- Add the float:
  ```cpp
  layout.add (std::make_unique<juce::AudioParameterFloat> (
      pid (clipper_drive_db, 1),
      "Clipper Drive",
      juce::NormalisableRange<float> (0.0f, 12.0f, 0.01f),
      0.0f,
      juce::AudioParameterFloatAttributes().withLabel ("dB")));
  ```
- **Expand the existing `character` choice** from 1 option to 3:
  ```cpp
  juce::StringArray { "Clean", "Tight", "Aggressive" }
  ```
  and **change the default index from 0 to 1 (Tight)**. Same param ID,
  same version hint — only the choice list and default change.

### 3.4 Product — `processBlock` chain

Add cached pointers in `prepareToPlay`:
- `limiterActive_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("limiter_active"));`
- `characterChoice_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("character"));`
- `clipperDriveDb_ = apvts.getRawParameterValue ("clipper_drive_db");` (atomic float pointer).

In `processBlock`, after io_input has been applied and the IN meter
measured, branch like this (preferred: extract into a small helper if
cleaner):

```text
// IO-input + IN meter already done at this point.

if (! limiterActive_->get())
{
    // Limiter section bypassed: skip Drive + Clipper + envelope chain.
    // Currently-applied gain so far is io_input only. Continue to:
    currentGrDb_.store (0.0f);              // no GR while bypassed
    currentTpTrimDb_.store (0.0f);
    goto applyOutputAndMeter;                // or restructure with if/else
}

// 1. Drive
buffer.applyGain (input_gain_db_lin)         // existing

// 2. Clipper Drive (NEW) — pre-envelope, per-sample, fast-path at 0 dB
const float clipperDriveDb = clipperDriveDb_->load (std::memory_order_relaxed);
if (clipperDriveDb > 0.0f)
{
    const float driveGain = decibelsToGain (clipperDriveDb);
    for (int ch = 0; ch < nch; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[ch][i] = juce::jlimit (-1.0f, 1.0f, d[ch][i] * driveGain);
    }
}
// else: zero CPU cost, signal flows unchanged.

// 3. Character mode → envelope mode
envelope_.setMode (mapCharacterIndexToMode (characterChoice_->getIndex()));

// 4..N: existing peak detect → envelope.process → lookahead → gain mult
//       → GR store → ispTrim if TP → io_output → OUT meter + TP → loudness.
//       UNCHANGED.

applyOutputAndMeter:
// io_output + OUT meter + TP + loudness as today.
```

`mapCharacterIndexToMode`:
- index 0 → `LimiterEnvelope::Mode::Clean`
- index 1 → `LimiterEnvelope::Mode::Tight`
- index 2 → `LimiterEnvelope::Mode::Aggressive`

**Latency reporting unchanged.** Do NOT change `setLatencySamples` in
this slice — modes share lookahead, clipper has no lookahead. The
limiter-active bypass keeps the reported latency the same (we accept
a constant delay even when bypassed, to keep PDC stable). This is the
right tradeoff for a mastering tool.

Avoid `goto` if the cleanest control flow can be expressed with an
`if/else` block — your call for readability. The constraint is what
happens, not the syntax.

### 3.5 Product — UI

In `MainView`:

- **Power toggle** (`limiter_active`): small button in the Maximizer
  panel header (top of the MAXIMIZER section). Use a
  `ButtonAttachment`. When off, dim the Maximizer-panel controls or
  red-tint the title so the state is unmistakable. Keep it visually
  distinct from the app-header Bypass.
- **Character slider**: already attached to `character`. Now snaps to
  3 stops. Update the endpoint labels under the slider from
  "Clean … Fast & Loud" to **"Clean … Aggressive"**.
- **Clipper Drive knob** (`clipper_drive_db`): place a new small
  rotary in the existing Maximizer knob row (alongside Release,
  Sustain, Lookahead, Auto Rel, Stereo Lk, M/S Lk). Label "Clipper",
  readout " dB" with 2 decimals (same single-source-of-truth format
  pattern as the other knobs). Use a `SliderAttachment`. The pointer
  parks at hard-left at default `0.0` (same convention as the Gain
  knob).

No other layout/styling changes.

### 3.6 Out of scope

- Auto-Release — its own future slice.
- Bench coverage of Clipper Drive paths — defer; default `0.0`
  preserves Slice 3/4/5 regression.
- Meter panel restructure / per-channel RMS — Slice 12.
- Gain-Match Auto/Track + Learn + Bypass-match — Slice 11b2.
- Greying out inert knobs per mode — UX nice-to-have, defer.

────────────────────────────────────────
4. SCOPE — files
────────────────────────────────────────

**HQ (one HQ commit, §3.2):**
- `docs/DECISIONS/ADR-0006-masterlimiter-character-and-onoff.md`
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h`
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`

**Product repo (uncommitted on `slice-09-character-and-onoff`):**
- `Source/parameters/ParameterIDs.h` — two new IDs.
- `Source/parameters/Parameters.cpp` — bool + float adds; expanded
  `character` choice + new default index.
- `Source/PluginProcessor.h` — cached pointers.
- `Source/PluginProcessor.cpp` — chain branching (active/inactive),
  Clipper Drive stage with `clipper_drive_db == 0` fast-path,
  Character mode dispatch via `envelope_.setMode`.
- `Source/ui/MainView.h` — `ButtonAttachment` for `limiter_active` and
  `SliderAttachment` for `clipper_drive_db`.
- `Source/ui/MainView.cpp` — power-toggle button, Clipper Drive knob
  + label + format, Character endpoint label.

Do NOT touch AnalyzerPro, `CMakeLists.txt`, PluginEditor, meters/,
loudness/, LookAndFeel, or anything else.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new `Source/` warnings. The HQ build also recompiles
`LimiterEnvelope.cpp` cleanly.

### Bench regression

Drivers set `character` explicitly (Clean for Slice 3/4/5) and don't
touch the two new params (so defaults `true` / `0.0` apply, which is
the byte-identical fast-path for both). Confirm:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE09_REGRESSION_S$SLICE"
done
```

All three end `PASS N/N`. If any fails — Clean mode is not byte-
identical to pre-Slice-9, or a driver doesn't pin character to Clean —
stop and report.

### Smoke checks (architect audition)

- Default plugin feels like a brickwall (Tight character).
- Character → Clean: reverts to the Slice 5 transparent T/S feel.
- Character → Aggressive: snappier; transients pulled tighter to
  ceiling.
- **Clipper Drive at 0 dB: signal unchanged from default. Raise to
  +3, +6, +12: audibly louder/denser with progressively harder
  transient catch; output still respects `ceiling_db`.**
- Clipper Drive stacks with each Character mode (Clean + clipper =
  smooth envelope + hard transient catch; Aggressive + clipper =
  modern-maximizer maximum-loudness).
- Limiter ON / OFF toggle: when off, signal flows io_input → io_output
  unchanged by Drive / Clipper / envelope / ceiling. GR meter reads 0.
  Latency reporting stays the same (no PDC change on toggle).
- All resize, knob/fader readouts, meter latch + click-reset still
  work.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log (including the HQ-from-sibling read of ADR-0006).
2. HQ commit: SHA + `git show --stat HEAD` (the three files).
3. Product branch created off `main`; files modified, one-line purpose
   each.
4. The exact `processBlock` order in the active path (Drive → Clipper
   Drive (with default fast-path) → envelope.setMode + envelope.process
   → lookahead+gain → GR store → ispTrim if TP → io_output → OUT
   meter+TP → loudness).
5. The `attackSamples_` mapping per mode at the prepared sample rate.
6. Confirmation that latency reported to host is unchanged across all
   limiter-active / character / clipper combinations.
7. Build summary (Debug + Release, warnings).
8. Bench regression: invocation + final `PASS N/N` for Slice 3 / 4 / 5.
9. `git status --short` for both repos.
10. Confirmation: no push (product or HQ), no merge to main, no branch
    deletion.
11. Open questions.

────────────────────────────────────────
7. ARCHITECT AUDITION
────────────────────────────────────────

avishali loads Release in Ableton and runs §5 smoke checks on real
material. On approval, close prompt commits the product repo as a
single clean Slice 9 commit, then I FF main + push. After that:
Slice 12 (meter restructure) opens.
