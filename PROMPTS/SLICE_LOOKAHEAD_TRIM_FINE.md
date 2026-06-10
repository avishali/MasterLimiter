# SLICE — Lookahead trim + fine-increment knobs

**Status:** ready for Cursor · **Architect:** Claude · **Audition/decide:** avishali
**Repo:** plugin `MasterLimiter` (SDK already supports runtime lookahead — likely no SDK change). Commit + push.
**Companion:** `docs/SIGNAL_FLOW.md` §1.

---

## Goal
Two changes to the lookahead DEV knobs (`dev_lookahead_band_ms`, `dev_lookahead_wide_ms`):
1. **Cut the max** — 12 ms is far more than needed; drop `kMaxLookaheadMs` to **6 ms**. This also lowers total reported latency (~24 ms → ~14 ms), fixing the meter-lag avishali noticed.
2. **Fine increments down to ~0** — range **0.0 – 6.0 ms** with a **0.01 ms step** so the very first transient can be dialed in. (Internally clamp to a 1-OS-sample minimum so the delay stays aligned with the envelope window.)

> Context: we proved (offline pedalboard render) the plugin's audio is clean — −119 dB passthrough, −81 dB under hard limiting. The earlier Plugin Doctor "harmonic" was a measurement artifact of the **high latency**. Trimming the max lookahead is the real fix for the latency/meter-lag.

---

## Allowed files to touch
```
Source/PluginProcessor.h            (kMaxLookaheadMs)
Source/PluginProcessor.cpp          (clamp behavior, if needed)
Source/parameters/Parameters.cpp    (param range/step/default)
docs/SIGNAL_FLOW.md
docs/PROGRESS.md
PROMPTS/PLAN.md
PROMPTS/SLICE_LOOKAHEAD_TRIM_FINE_CLOSE.md   (new, at close)
```
SDK only if a clamp tweak is needed (see §3).

---

## 1. `PluginProcessor.h`
```cpp
static constexpr float kMaxLookaheadMs = 6.0f;   // was 12.0f
```
Everything else (allocation to max, constant-latency padding) already scales off this — no other structural change. Reported latency becomes `2 × baseMaxLookahead(6 ms) + osLatency + finalCeiling` (~14 ms).

## 2. `Parameters.cpp` — both lookahead params
Change `dev_lookahead_band_ms` and `dev_lookahead_wide_ms`:
| field | new value |
|---|---|
| range | **0.0 – 6.0 ms** |
| step | **0.01 ms** (fine) |
| default | **5.0 ms** |

(Was 1.0–12.0, default 7.0.) Keep the "ms" label.

## 3. Clamp / zero handling (`PluginProcessor.cpp`)
In the configure block, the active lookahead is `round(ms · osSampleRate)`. Allow it to go small but **clamp to a 1-OS-sample minimum** so the audio delay and the envelope's active window stay equal (the SDK `setActiveLookaheadSamples` already clamps to ≥1; mirror that on the delay line):
```cpp
const int laBandActive = juce::jlimit (1, osMaxLookahead, (int) std::llround (devLaBandMs * 1e-3 * osSampleRate_));
const int laWideActive = juce::jlimit (1, osMaxLookahead, (int) std::llround (devLaWideMs * 1e-3 * osSampleRate_));
```
So "0.00 ms" on the knob = the smallest possible lookahead (1 OS sample ≈ 0.005 ms) — effectively instant transient catch; the FinalCeiling brickwall still guarantees no overshoot. The constant-latency pad fills the remaining slack to `2 × osMax`, so reported latency stays fixed regardless of knob position (no PDC churn while sweeping).

> If the SDK's `setActiveLookaheadSamples(1)` mis-behaves at the minimum (it shouldn't — it clamps to [1, max] and refreshes the attack table), note it and we'll patch the SDK; otherwise no SDK change.

## 4. Interaction with the attack knob (note, no code)
`dev_attack_ms` is clamped to the active lookahead window (`attackSamples_ ≤ lookaheadSamples_`). So at very short lookahead the attack auto-shortens too — correct and intended. Document this coupling in the close note.

---

## 5. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
cp -R build/MasterLimiter_artefacts/Release/AU/MasterLimiter.component  ~/Library/Audio/Plug-Ins/Components/
cp -R build/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3     ~/Library/Audio/Plug-Ins/VST3/
cp -R build/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3     /Library/Audio/Plug-Ins/VST3/
auval -v aufx MaLm Melc 2>&1 | tail -5
```
Acceptance:
1. Builds clean; AU validates.
2. Reported latency ≈ 14 ms (down from ~24); meter lag noticeably reduced.
3. Both knobs sweep 0.00 → 6.00 in 0.01 steps; at 0.00 the limiter catches transients near-instantly (punchier, FinalCeiling still holds the ceiling — verify output true-peak ≤ ceiling at 0.00).
4. No PDC re-sync glitch when sweeping the knobs mid-playback (constant-latency padding intact).
5. CPU still ~7–10%.

Close gate: update `docs/SIGNAL_FLOW.md` §1 (max lookahead 6 ms, knob range 0–6/0.01), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit + push; archive `PROMPTS/SLICE_LOOKAHEAD_TRIM_FINE_CLOSE.md`.

---

## Note
- Install also copies to the **system** VST3 (`/Library/Audio/Plug-Ins/VST3`, world-writable — no sudo) per avishali's current setup.
- Bigger filter/crossover work (custom FIR, movable/3-band crossover, Color-phase fix, HF phase, anti-alias high-cut) is a separate roadmap arc — see `docs/LIMITER_TYPES.md` / the filter-quality plan; not in this slice.
