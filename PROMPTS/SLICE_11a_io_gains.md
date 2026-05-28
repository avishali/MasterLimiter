# Cursor Task — Slice 11a: I/O gain stages (Input L/R + Output L/R) + Drive re-range + Ceiling 0..−24 (ADR-0007)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Touches HQ (one ADR commit) and product repo (params + DSP + UI wiring).**
> This slice introduces **NEW FROZEN parameter IDs** and changes the range of
> two existing frozen params, per ADR-0007. Gain-Match logic (Auto/Track +
> Gain⇄Ceiling Link + Learn) is **deferred to Slice 11b**. Branch off `main`,
> build, run bench regression, **do NOT push** — architect handles push +
> close.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Files in §3 only. No AnalyzerPro edits.
- **New parameter IDs introduced here are FROZEN from this slice onward.**
  Use the exact names + ranges + defaults in §4.
- The two existing range changes (`input_gain_db`, `ceiling_db`) are
  documented in ADR-0007 — no state-versioning needed (no shipped sessions).
- RT-safety: per-block param reads, no allocation in `processBlock`, no
  locks. Use atomic snapshots that already exist where applicable.
- **No Gain-Match logic in this slice** — `gain_match_auto`,
  `gain_ceiling_link`, and Learn are Slice 11b. Don't introduce those IDs
  yet.
- Branch `slice-11a-io-gains` off `main`. **Do NOT commit the product repo
  until the architect's close prompt.** The HQ ADR commit is a separate
  step in §3.1 — that one DOES commit (HQ-side only).

────────────────────────────────────────
1. WHY
────────────────────────────────────────

Slice 10 shipped the Maximizer UI shell with all I/O-side controls and the
Gain knob as inert placeholders. ADR-0007 (already on disk at
`third_party/melechdsp-hq/docs/DECISIONS/ADR-0007-masterlimiter-io-gain-and-gain-match.md`)
locks the gain-staging model. This slice wires it: independent L/R I/O
Input and Output trims, the Gain knob becomes a real positive-only drive,
the ceiling gains downward headroom, and meters measure at the right
points.

Read ADR-0007 in full before editing — it's the authoritative spec.

────────────────────────────────────────
2. TRINITY (lightweight)
────────────────────────────────────────

Read:
- `third_party/melechdsp-hq/docs/DECISIONS/ADR-0007-masterlimiter-io-gain-and-gain-match.md`
- `Source/parameters/ParameterIDs.h`, `Source/parameters/Parameters.cpp`
- `Source/PluginProcessor.{h,cpp}` (whole `processBlock`)
- `Source/ui/MainView.{h,cpp}` (existing attachments + placeholder controls)

Output the retrieval log.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

### 3.1 Commit ADR-0007 to HQ (separate HQ commit, no push)

The ADR file is already on disk in the sibling HQ checkout. Commit it on
HQ's `main` (no branch, no push):

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short   # expect: ?? docs/DECISIONS/ADR-0007-masterlimiter-io-gain-and-gain-match.md
git add docs/DECISIONS/ADR-0007-masterlimiter-io-gain-and-gain-match.md
git commit -m "$(cat <<'EOF'
ADR-0007: MasterLimiter I/O gain staging + dual Gain-Match

Locks the control model that Slice 10 mocked: separate I/O Input/Output L/R
trims, positive-only Maximizer Gain (drive), Ceiling 0..-24, and two
independent Gain-Match methods (Auto/Track via LoudnessAnalyzer +
Gain<->Ceiling structural link). 11a wires the gain staging; 11b implements
Gain-Match + Learn Input Gain.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Do **NOT** push HQ. Return to the product repo for the rest of the work.

### 3.2 Product repo: branch off `main`

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout main
git status --short   # must be clean
git checkout -b slice-11a-io-gains
```

### 3.3 Add new frozen params and change two ranges

In `Source/parameters/ParameterIDs.h` — add these constants alongside the
existing ones (exact IDs, do not rename):

```cpp
constexpr std::string_view io_input_l_db   = "io_input_l_db";
constexpr std::string_view io_input_r_db   = "io_input_r_db";
constexpr std::string_view io_input_link   = "io_input_link";
constexpr std::string_view io_output_l_db  = "io_output_l_db";
constexpr std::string_view io_output_r_db  = "io_output_r_db";
constexpr std::string_view io_output_link  = "io_output_link";
```

In `Source/parameters/Parameters.cpp::createParameterLayout()`:

- **Re-range** `input_gain_db`: `NormalisableRange<float>(0.0f, 24.0f, 0.01f)`,
  default `0.0f`, label `"dB"`. (Was `−12..+24`.)
- **Re-range** `ceiling_db`: `NormalisableRange<float>(-24.0f, 0.0f, 0.01f)`,
  default `-1.0f`, label `"dB"`. (Was `−12..0`.)
- **Add four `AudioParameterFloat`** (version hint 1):
  - `io_input_l_db`,  range `−24..+24`, step `0.01`, default `0.0`, label `"dB"`.
  - `io_input_r_db`,  range `−24..+24`, step `0.01`, default `0.0`, label `"dB"`.
  - `io_output_l_db`, range `−24..+24`, step `0.01`, default `0.0`, label `"dB"`.
  - `io_output_r_db`, range `−24..+24`, step `0.01`, default `0.0`, label `"dB"`.
- **Add two `AudioParameterBool`** (version hint 1):
  - `io_input_link`,  default `true`.
  - `io_output_link`, default `true`.

Watch the pedalboard label-vs-ID gotcha (Slice 5 notes): use plain `"dB"`
labels and no spurious suffixes — bench drivers expect bare IDs.

### 3.4 Wire DSP in `PluginProcessor`

In `prepareToPlay`, cache raw `std::atomic<float>*` / `AudioParameterBool*`
pointers for the new params (or read via `apvts.getRawParameterValue` per
block — match the existing pattern).

In `processBlock`, restructure the signal chain to this exact order (per
ADR-0007):

```text
1.  Apply per-channel I/O Input gain:
       ch 0 *= dBtoGain(io_input_l_db)
       ch 1 *= dBtoGain(io_input_r_db)
2.  Measure INPUT peak L / R   --> inputPeakL/RDb_  (POST-io_input, PRE-drive)
3.  Apply Drive (mono) input_gain_db to all channels  (existing applyGain).
4.  Peak detector + envelope (existing).
5.  Lookahead pushPop + gain multiplication (existing).
6.  Update currentGrDb_ (existing).
7.  TP path (ispTrim_) when ceiling_mode == 1 (existing).
8.  Apply per-channel I/O Output gain:
       ch 0 *= dBtoGain(io_output_l_db)
       ch 1 *= dBtoGain(io_output_r_db)
9.  Measure OUTPUT peak L / R + outputTpDb_  (POST-io_output).
10. loudness_.process(buffer)  (POST-io_output, so LUFS reflects final out).
```

- All gain multiplications use `juce::Decibels::decibelsToGain (dB)` —
  no `std::pow` calls; precompute once per block.
- `io_input_link` and `io_output_link` are **UI-side** coupling; the DSP
  applies the L and R params independently every block regardless of link
  state. Do **not** combine them in DSP.
- Output Gain is post-ceiling — it CAN push output above the ceiling. That
  is intended (ADR-0007). The OUTPUT/TP meters measure post-Output-Gain so
  the user can see overage.

### 3.5 Wire UI in `MainView`

- **Gain knob** — replace the placeholder with a real `SliderAttachment` to
  `input_gain_db` (range now `0..+24`). Pointer still parks at hard-left at
  default `0.0` (the existing LookAndFeel handles this correctly).
- **Output Level knob** — already attached to `ceiling_db`; the new range
  `0..−24` flows through automatically. Confirm the slider's displayed
  range follows the param. Apply slider precision (e.g. 2 decimals, `dB`
  suffix — same single-source-of-truth rule as Slice 10.4).
- **I/O Input L / R faders** — `SliderAttachment` each to `io_input_l_db`
  and `io_input_r_db`.
- **I/O Output L / R faders** — `SliderAttachment` each to `io_output_l_db`
  and `io_output_r_db`.
- **L/R link buttons** — `ButtonAttachment` each to `io_input_link` and
  `io_output_link`. When the link is **on**, add a small UI-side handler
  (e.g. a `Slider::Listener` or a parameter-value-changed lambda) that
  mirrors moves from one channel to the other (write to the partner's
  param). Avoid feedback loops (guard with a recursion flag or sentinel).
  When link is **off**, no mirroring.
- Slider precision/suffix: configure on each slider then **clear the
  APVTS-attachment-installed text formatter** (the Slice 10.4 pattern) so
  per-slider settings apply.

### 3.6 Out of scope (DO NOT do in 11a)

- Gain-Match Auto/Track logic and the `gain_match_auto` param — **11b**.
- Gain⇄Ceiling Link control coupling and the `gain_ceiling_link` param —
  **11b**.
- Learn Input Gain functionality and the learned-reference state — **11b**.
- Bypass wiring — later.
- Any visual restyle — done in Slice 10.

────────────────────────────────────────
4. SCOPE — files
────────────────────────────────────────

**HQ commit (§3.1):**
- `third_party/melechdsp-hq/docs/DECISIONS/ADR-0007-masterlimiter-io-gain-and-gain-match.md`

**Product repo (uncommitted on `slice-11a-io-gains`):**
- `Source/parameters/ParameterIDs.h` — new IDs.
- `Source/parameters/Parameters.cpp` — 4 new floats + 2 new bools, 2 range
  changes on existing params.
- `Source/PluginProcessor.h` — cache pointers for new params (and any
  helper accessors needed for UI link handling).
- `Source/PluginProcessor.cpp` — restructured `processBlock` chain (§3.4)
  + updated meter measurement points.
- `Source/ui/MainView.h` — new attachment members for the new params.
- `Source/ui/MainView.cpp` — wire attachments, L/R link mirroring handler,
  slider precision.

Do NOT touch AnalyzerPro, `CMakeLists.txt` (no new files expected),
PluginEditor, MeterComponent/MeterGroupComponent, GR meter, LoudnessNumericPanel,
LookAndFeel, or any other file unless necessary and justified.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```
Zero new `Source/` warnings.

### Bench regression (DSP must remain equivalent at defaults)

I/O Input/Output params default to 0.0 dB (unity). `input_gain_db` default
unchanged at 0.0. `ceiling_db` default unchanged at −1.0. So the bench
must PASS unchanged:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE11a_REGRESSION_S$SLICE"
done
```

All three end `PASS N/N`. If any fails:
- If the bench driver passed a value outside the new `input_gain_db` range
  (`0..+24`), the bench needs an update (likely a driver-side clamp). Stop
  and report — don't widen the param range to accommodate.
- Otherwise, DSP got touched incorrectly — stop and report.

### Smoke checks for the architect audition

- Move I/O Input L fader → only L channel level changes (link off);
  with link on, R follows L 1:1.
- Same for Output L/R + link.
- Push Gain up: limiting engages, GR meter responds, output stays at
  ceiling.
- Push Output Gain up past 0 dB: OUT meter and TP readout exceed 0 dBFS
  (Ozone-like — confirms post-Output-Gain measurement).
- Ceiling drag goes all the way to −24 now.
- Resize still clean. All knob/fader readouts formatted correctly (single
  suffix, sensible decimals).

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log (including ADR-0007 read).
2. HQ-side: commit SHA + `git show --stat HEAD` for the ADR commit. NO push
   confirmation.
3. Product-side: branch `slice-11a-io-gains` created off `main`; files
   modified, one-line purpose each.
4. The exact `processBlock` chain order (§3.4 numbered list) as
   implemented, with file:line refs.
5. Param diff vs `main`: the 6 new params (ID + range + default) and the 2
   re-ranged ones.
6. Build summary (Debug + Release, warnings).
7. Bench regression: invocation + final `PASS N/N` for Slice 3 / 4 / 5.
8. `git status --short` (working tree dirty on the product branch — that is
   expected, do NOT commit the product repo).
9. Open questions.

────────────────────────────────────────
7. ARCHITECT AUDITION (after Cursor reports)
────────────────────────────────────────

avishali loads Release in Ableton and runs the smoke checks above. On look-
and-behaviour approval, the close prompt commits the product repo as a
single clean Slice 11a commit and fast-forwards `main`. Then Slice 11b
(Gain-Match + Learn) opens.
