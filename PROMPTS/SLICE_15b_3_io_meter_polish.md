# Cursor Task — Slice 15b.3: I/O meter polish (release, -inf floor, RMS overlay, RMS readout)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Small product-only refinement on the uncommitted Slice 15b branch** (before
> the 15b close). Four I/O-meter fixes from avishali's audition. No audio-path
> change, no params, no ADR, no HQ change. **Do NOT commit, do NOT push.**

> Continue product branch `slice-15b-io-meter-features`. HQ sibling
> `slice-15b-meter-scale` (Top48Db) stays as-is.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product edits only: `Source/ui/meters/MeterComponent.cpp` (+`.h` if needed),
  `Source/ui/meters/MeterGroupComponent.cpp`, `Source/PluginProcessor.cpp`
  (level floor only — measurement, no audio change).
- Do NOT touch the GR meter, params, ADRs, or HQ.
- The PluginProcessor change is the level **measurement floor** only; it must
  NOT alter any audio sample (Slice 3/4/5 unchanged).
- **Do NOT commit, do NOT push.** Leave `MCP` untouched.

────────────────────────────────────────
1. RELEASE — too slow now, speed it up
────────────────────────────────────────

`MeterGroupComponent.cpp` line ~116: `constexpr float releaseDbPerSec = 20.0f;`
was too slow. Change to **`40.0f`** (faster fall on stop while still gliding,
not snapping). Single tunable constant — report it so avishali can dial.

────────────────────────────────────────
2. -inf FLOOR — bars must empty at silence
────────────────────────────────────────

Today the engine floors I/O peak/RMS at **−100 dB** (`gainToDecibels(x, -100.0f)`),
so at silence the bar sits at ~16 % height (the −100 position in the −120…+6
FullRange) even though the readout already shows "-inf".

- In `PluginProcessor.cpp`, lower the I/O peak **and** RMS measurement floor
  from `-100.0f` to **`-120.0f`** (match the meter's FullRange bottom) at the
  input and output measurement points — so silence drives the bar to **empty**.
  Measurement only; no audio change.
- Keep the readout "-inf" behaviour (`formatDbBare` returns "-inf" for
  `v <= -100`). Result: at silence the bar empties **and** the readout shows
  "-inf". Verify the bar reaches zero height at digital silence in all range
  modes (Full / 48 / 24 / 12 / 6).

────────────────────────────────────────
3. RMS = OVERLAY on the peak bar (don't recolour the whole bar)
────────────────────────────────────────

Today (`MeterComponent::paintLevel` ~328/348/353) the RMS toggle switches the
bar fill to the RMS level and recolours it (`fillNorm = showRms_ ? rmsNorm :
peakNorm`). Change so the **peak bar is always drawn**, and RMS is an **added
overlay** when enabled:

- **Always** draw the peak fill from bottom to `peakNorm` in the **peak colour**
  (the light blue from 15b.2) + the existing peak cap. This is unconditional —
  toggling RMS must NOT change the peak bar's level or colour.
- **When `showRms_`**: additionally draw the **RMS fill** from bottom to
  `rmsNorm` in a **distinct RMS colour** (e.g. `theme.accent`), layered over the
  lower portion of the peak bar (RMS ≤ peak, so it sits below the peak cap).
  Use enough contrast/opacity that both are readable: light-blue peak above the
  RMS level, RMS colour below it.
- Remove the `fillNorm = showRms_ ? rmsNorm : peakNorm` switch and the
  show-dependent recolour. The peak-hold cap stays as-is.
- In `MeterGroupComponent`, the `setShowRms`/`DisplayMode::Rms` wiring (~165) is
  no longer needed to swap the bar — keep the provider feeding both peak and
  rms; `showRms_` now only controls whether the RMS overlay draws (§4 keeps the
  numeric always on).

────────────────────────────────────────
4. RMS READOUT — always visible; toggle only the bar
────────────────────────────────────────

`MeterComponent::paintLevel` ~523 skips the RMS numeric when `! showRms_`.
Change so the **RMS numeric readout is always drawn** (both peak and RMS
numbers visible regardless of the toggle). `showRms_` controls **only the RMS
bar overlay** (§3), not the numeric. So: RMS toggle OFF → peak bar only, but
peak + RMS numbers both shown; RMS toggle ON → adds the RMS bar overlay.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings.
- No audio change → Slice 3/4/5 quick unchanged (sanity).
- In a host:
  - Bars fall faster on stop (~40 dB/s) and **empty fully** at silence; readout
    shows "-inf".
  - RMS toggle OFF: single light-blue peak bar, but **both** peak and RMS
    numbers visible.
  - RMS toggle ON: same peak bar + an RMS-colour overlay up to the RMS level
    (peak bar unchanged in level/colour).

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Diffs: release rate; measurement floor −100→−120 (peak + RMS, in + out);
   RMS overlay rework; RMS numeric always-on.
3. Build summary (Debug + Release, warnings).
4. Slice 3/4/5 quick: unchanged (proves measurement-only).
5. `git status --short` (product slice-15b branch; HQ unchanged).
6. Open questions (release feel at 40 dB/s, RMS overlay colour/contrast).

────────────────────────────────────────
7. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali re-auditions the four fixes. On approval → architect writes the
Slice 15b consolidated close (the existing `SLICE_15b_CLOSE.md` still applies;
it commits all the accumulated 15b work). **Do not self-close.**
