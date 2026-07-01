# SLICE — DEV: clamp/brickwall on-off toggles + activity metering (diagnostic)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` only (product-only; no SDK/submodule). All DEV/temporary.
**Companion:** `docs/SIGNAL_FLOW.md` §2.14 (M/S safety clamp), §2.16 / §4.4 (FinalCeiling).
**Why:** avishali is diagnosing the ~3 dB RMS density M/S shows vs Stereo (peaks pinned at ceiling; pre-existing, not from A2). Root suspect = the **M/S-only per-sample decoded clamp** (`msSafetyGain`). This slice makes the two peak-control stages **switchable and metered** so their contribution can be heard and seen. Nothing here is permanent — it's an audition instrument.

There are two independent stages:
- **`msSafetyGain`** — per-sample hard clamp on decoded L/R, **M/S mode only** ([PluginProcessor.cpp:1634–1640](Source/PluginProcessor.cpp:1634)). The density suspect.
- **`FinalCeilingLimiter`** — the always-on, all-modes final brickwall ([:1712](Source/PluginProcessor.cpp:1712)).

---

## Allowed files to touch
```
Source/parameters/ParameterIDs.h  Source/parameters/Parameters.cpp
Source/PluginProcessor.h / PluginProcessor.cpp
Source/ui/DevControlsComponent.h / .cpp
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_CLAMP_DEV_TOGGLES_CLOSE.md   (new, at close)
```
**Non-goals / STOP:** no change to the *math* of either stage when its toggle is On (default = current behavior, bit-identical); no new frozen params (DEV bools only); no SDK edit; no latency change.

---

## 1. DEV params (`ParameterIDs.h`, `Parameters.cpp`)
| ID | Type | Default | Effect |
|---|---|---|---|
| `dev_ms_safety_clamp` | Bool | **On (true)** | Off → skip the M/S decoded clamp; overs pass to the FinalCeiling (still ceiling-safe). |
| `dev_final_ceiling` | Bool | **On (true)** | Off → skip `finalCeiling_.process()`. **Overs escape — audition only.** |

Cache raw pointers + jassert, like the other `dev_*` params.

## 2. Processor — gate + meter each stage (`PluginProcessor.{h,cpp}`)

### 2a. `msSafetyGain` (M/S apply loop, [:1633–1646](Source/PluginProcessor.cpp:1633))
Gate the clamp and track its depth:
```cpp
const bool msClampOn = devMsSafetyClamp_ == nullptr || devMsSafetyClamp_->load() >= 0.5f;
float minMsSafety = 1.0f;   // before the osN loop
...
float msSafetyGain = 1.0f;
if (msClampOn && decodedPeak > thresholdLin)
{
    msSafetyGain = thresholdLin / decodedPeak;
    outL[i] *= msSafetyGain;
    outR[i] *= msSafetyGain;
}
minMsSafety = std::min (minMsSafety, msSafetyGain);
```
`totalGainL/R` keep multiplying by `msSafetyGain` (unchanged; it's 1.0 when off). After the loop publish `currentMsClampDb_ = jmax(0, -gainToDecibels(minMsSafety))` and latch `maxMsClampDb_`. In the **Stereo / non-M-S** path store `0.0f` (the clamp doesn't run there).

### 2b. FinalCeiling (`[:1712]`)
```cpp
float prePeak = bufferAbsMax (buffer);            // scalar max|sample| over the block, both channels
if (devFinalCeiling_ == nullptr || devFinalCeiling_->load() >= 0.5f)
    finalCeiling_.process (buffer);
float postPeak = bufferAbsMax (buffer);
const float fcGrDb = (prePeak > 1.0e-6f && postPeak > 1.0e-6f)
    ? juce::jmax (0.0f, juce::Decibels::gainToDecibels (prePeak) - juce::Decibels::gainToDecibels (postPeak))
    : 0.0f;
currentFinalCeilingDb_.store (fcGrDb, ...);  // + latch maxFinalCeilingDb_
```
> Block-peak based → approximate GR readout, adequate for a DEV instrument. When the toggle is Off, `postPeak == prePeak` so the readout is 0 (and overs pass — that's the audition).

### 2c. Atomics + getters + reset
Add `currentMsClampDb_/maxMsClampDb_`, `currentFinalCeilingDb_/maxFinalCeilingDb_` (mirror the `currentClipDb_/maxClipSinceResetDb_` pattern) with getters; extend the peak-reset path to clear the maxes.

## 3. UI (`DevControlsComponent`)
New **"PEAK CONTROL (DEV)"** section:
- Toggle **M/S Safety Clamp** (`dev_ms_safety_clamp`) + a **current / max dB** readout of `getCurrentMsClampDb()/getMaxMsClampDb()` — so you *see* how hard it's clamping (only moves in M/S mode).
- Toggle **Final Ceiling** (`dev_final_ceiling`) + a **current / max dB** readout. Tooltip on this one: *"OFF lets peaks exceed the ceiling — audition only."*
- Readouts refresh on the existing 30 Hz meter sync (add to `syncMetersFromProcessor` if the DEV panel isn't already polled), or a small local timer in the DEV component.

---

## 4. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
```
**Acceptance (Claude verifies 1–4; avishali auditions 5):**
1. Build clean; AU validates; no latency change; **both toggles On (default) = bit-identical to current**.
2. M/S Safety Clamp **On**: the M/S clamp readout shows non-zero dB on wide/heavy material (Stereo = 0). **Off**: readout 0, M/S density audibly drops toward Stereo, **output true-peak still ≤ ceiling** (FinalCeiling catches it).
3. Final Ceiling readout shows the block peak reduction; **Off** → overs above ceiling appear on the output TP meter (expected; audition-only).
4. Toggles are RT-safe (relaxed-atomic reads, no allocation); resets clear the max readouts.
5. **Audition:** A/B M/S with the clamp on vs off — does removing the hard clamp bring M/S loudness/character in line with Stereo? This informs the real fix (fold decoded-L/R peak into the smooth wideband detection).

**Close gate:** update `docs/SIGNAL_FLOW.md` §2.14/§4.4 (toggles + readouts), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit plugin-only; archive CLOSE prompt.

---

## Notes for the architect (context, not for Cursor)
- This is the diagnostic that precedes the real **M/S-detection fix**: if clamp-off brings M/S in line, the fix is to make the smooth wideband envelope limit the *decoded* peak (fold `max(|mid+side|,|mid−side|)` into detection) so the hard clamp rarely fires — smoother M/S, loudness-neutral vs Stereo. Bake the toggles away then.
- `bufferAbsMax` is a tiny local helper (scalar max over channels/samples) — reuse any existing peak-scan util if present.
