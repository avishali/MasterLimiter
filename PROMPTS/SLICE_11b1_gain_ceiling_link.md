# Cursor Task — Slice 11b1: Gain ⇄ Ceiling structural link (control coupling only)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. Introduces ONE NEW FROZEN bool param
> (`gain_ceiling_link`), wires the existing UI link button to it, and
> implements an inverse parameter-coupling listener. NO DSP audio
> processing changes — this is pure parameter linkage at the
> APVTS/UI layer.** Per ADR-0007. Auto/Track, Learn, and Bypass-with-
> match are **Slice 11b2** (one cohesive LUFS-driven feature set). Branch
> off `main`, build, run bench regression (must pass — defaults are
> no-ops), **do NOT push** — architect handles push + close.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Files in §3 only. No HQ edits, no AnalyzerPro edits.
- One new FROZEN parameter ID introduced here: **`gain_ceiling_link`**.
  Use the exact name + default in §4.
- No new audio DSP, no other param range/default/ID changes.
- RT-safety: avoid heavy or blocking work on the audio thread. The
  coupling is **UI/control-layer** — listener-driven, runs on the
  message thread.
- Branch `slice-11b1-gain-ceiling-link` off `main`. **Do NOT commit the
  product repo** — architect's close prompt handles that.

────────────────────────────────────────
1. WHY (from ADR-0007)
────────────────────────────────────────

ADR-0007 defines two independent Gain-Match methods. **Method 2 — Gain ⇄
Ceiling Link — is a structural, dB-for-dB control coupling**: raising
`input_gain_db` by *X* dB lowers `ceiling_db` by *X* dB (and vice versa),
clamped to each param's range. No measurement, no DSP — just a parameter
listener pair. Output level "stays the same" in the dB-math sense (drive
up + ceiling down by the same amount = output peak unchanged), so you
hear *density change* not "louder," which is the maximizer A/B feel.

This is the smaller, faster half of Slice 11b — keeping it in its own
slice means LUFS work (Auto/Track + Learn + Bypass-match) stays clean in
11b2.

────────────────────────────────────────
2. TRINITY (lightweight)
────────────────────────────────────────

Read:
- `third_party/melechdsp-hq/docs/DECISIONS/ADR-0007-masterlimiter-io-gain-and-gain-match.md` — esp. §"Decision" item 2 and the parameter table.
- `Source/parameters/ParameterIDs.h`, `Source/parameters/Parameters.cpp`
  (add the new bool, do not touch other params).
- `Source/PluginProcessor.{h,cpp}` (where the listener should live).
- `Source/ui/MainView.{h,cpp}` (existing "Gain / Ceiling Link" button
  is currently a placeholder — wire it).

Output the retrieval log.

────────────────────────────────────────
3. SCOPE — files
────────────────────────────────────────

**MODIFY:**
- `Source/parameters/ParameterIDs.h` — add the new ID.
- `Source/parameters/Parameters.cpp` — declare the new bool param.
- `Source/PluginProcessor.h` — declare the listener members + helper.
- `Source/PluginProcessor.cpp` — implement listener wiring, the
  inverse-coupling logic, and the feedback-loop guard.
- `Source/ui/MainView.h` — `ButtonAttachment` for the link toggle.
- `Source/ui/MainView.cpp` — attach the existing link button to the
  param.

Do NOT touch any other file (no PluginEditor, no CMake, no meters, no
LookAndFeel, no HQ, no AnalyzerPro).

NON-GOALS in 11b1 (locked for 11b2):
- No `gain_match_auto` param, no Auto/Track LUFS loop.
- No Learn Input Gain logic, no learned-reference state.
- No Bypass changes.
- No `LoudnessAnalyzer` use beyond what's already in `processBlock`.

────────────────────────────────────────
4. WHAT TO BUILD
────────────────────────────────────────

### 4.1 The frozen param

In `ParameterIDs.h`, add alongside the existing IDs:

```cpp
inline constexpr std::string_view gain_ceiling_link { "gain_ceiling_link" };
```

In `Parameters.cpp::createParameterLayout()`, add:

```cpp
layout.add (std::make_unique<juce::AudioParameterBool> (
    pid (gain_ceiling_link, 1),
    "Gain/Ceiling Link",
    false,                       // default OFF — bench / no-effect at defaults
    juce::AudioParameterBoolAttributes()));
```

(Watch the pedalboard label-vs-ID gotcha — empty/default label is safe.)

### 4.2 Inverse coupling — semantics

When **`gain_ceiling_link` is true**:
- A user moving **`input_gain_db`** by Δ (sign + magnitude) immediately
  moves **`ceiling_db`** by **−Δ**, clamped to ceiling's range `[−24, 0]`.
- A user moving **`ceiling_db`** by Δ immediately moves
  **`input_gain_db`** by **−Δ**, clamped to drive's range `[0, +24]`.
- The values at the moment the link is **toggled on** are the **baseline**
  — no automatic compensation jump on toggle. Further moves couple from
  the captured baseline.
- If a coupled move would clamp at either range edge, the *driving*
  param still moves to where the user dragged it; the *coupled* param
  sticks at its clamp. (This is the usual "lose the link" edge behaviour
  and is fine.)

When **`gain_ceiling_link` is false**: independent, no coupling.

### 4.3 Implementation

- Use `juce::AudioProcessorValueTreeState::addParameterListener` for both
  `input_gain_db` and `ceiling_db`. The processor implements
  `juce::AudioProcessorValueTreeState::Listener::parameterChanged
  (const juce::String& paramID, float newValue)`.
- **Cache** raw `std::atomic<float>*` pointers (or
  `AudioParameterFloat*`) for both params + the bool param at prepare
  time so the listener never allocates.
- **Last-known values**: track the previous value of each coupled param
  (two `float` members on the processor) to compute Δ on each change.
- **Feedback-loop guard**: use a single `std::atomic<bool>
  couplingInProgress_` (or a non-atomic bool if you can guarantee the
  listener runs single-threaded on the message thread in this host
  context — JUCE's APVTS listeners typically do). When the listener
  applies the inverse update, set the guard, write the partner param,
  clear the guard. The listener re-entry from the partner write checks
  the guard and short-circuits.
- **Threading**: `addParameterListener` callbacks may fire on either
  the message thread or the audio thread depending on the source of the
  change (UI vs. automation). Make the listener body **non-allocating
  and RT-safe**: the inverse update is a simple param-value write,
  which JUCE guarantees is safe to call from any thread. If for any
  reason you defer (e.g. via `MessageManager::callAsync`), do **not**
  block the audio thread.
- **Writing the partner value**: use
  `apvts.getParameter (id)->setValueNotifyingHost (normalised)` (or the
  equivalent on the cached `AudioParameterFloat*`). Convert the new
  real value to normalised via the parameter's `convertTo0to1`.
- **On link toggle (false→true)**: do nothing — capture the current
  values as the new baseline by updating the last-known cache. This
  prevents the immediate Δ from being misread as a user move.

### 4.4 UI

In `MainView`, the existing "Gain / Ceiling Link" button (currently a
placeholder) gets a `juce::AudioProcessorValueTreeState::ButtonAttachment`
to `gain_ceiling_link`. The button's clicking-toggles-state behaviour
is already correct from the placeholder; just add the attachment and
remove any placeholder click-handler that no longer makes sense.

No restyling. No layout changes (that's a later UI graphics pass per
avishali).

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout main
git status --short        # must be clean
git checkout -b slice-11b1-gain-ceiling-link
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new `Source/` warnings.

### Bench regression (must PASS unchanged)

`gain_ceiling_link` defaults OFF → no behavior change at defaults. Bench
drivers don't touch this param. Bench Slice 3/4/5 must be byte-identical:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE11b1_REGRESSION_S$SLICE"
done
```

All three end `PASS N/N`. If any fails: you've touched DSP — STOP.

### Smoke checks for the architect audition
- Link OFF: moving Gain doesn't move Ceiling, and vice versa.
- Toggle Link ON: nothing visibly changes (values stay).
- Link ON: push **Gain +3 dB** → **Ceiling drops by 3 dB**. The output
  peak level reads the same as before the change (density/character
  changes, not loudness).
- Link ON: drag Ceiling up 2 dB → Gain drops by 2 dB (clamped at 0 if
  it would go negative).
- Drag Gain to its max (+24): Ceiling pegs at its min (−24); release the
  drag, ceiling stays at clamp (graceful, no feedback storm).
- Toggle Link OFF: independent again from current positions.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Files modified, one-line purpose each.
3. The new param ID + default (`gain_ceiling_link`, bool, default `false`).
4. A short description of where the listener lives, how the feedback
   guard works, and where the last-known values are stored.
5. Build summary (Debug + Release, warnings).
6. Bench regression: invocation + `PASS N/N` for Slice 3 / 4 / 5.
7. `git status --short` (working tree dirty on the branch, no commit).
8. Open questions.

────────────────────────────────────────
7. ARCHITECT AUDITION (after Cursor reports)
────────────────────────────────────────

avishali loads Release in Ableton and runs the §5 smoke checks. On
approval, the close prompt collapses to a single clean Slice 11b1
commit, FF main, push. Then Slice 11b2 (Auto/Track + Learn + Bypass-
with-match) opens.
