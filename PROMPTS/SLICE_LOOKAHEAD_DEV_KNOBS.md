# SLICE — Lookahead time DEV knobs (per-band + wideband)

**Status:** ready for Cursor · **Architect:** Claude · **Audition/decide:** avishali
**Repos:** SDK `melechdsp-hq` (LimiterEnvelope runtime lookahead) + plugin `MasterLimiter`. Commit + push BOTH (SDK first).
**Companion:** `docs/SIGNAL_FLOW.md` §1 (lookahead/latency).

---

## Goal
Add **two live DEV knobs** to tune the two voicing-relevant lookahead times independently:
- **`dev_lookahead_band_ms`** — per-band lookahead (`lookahead_` delay + low/high envelope peak-hold windows).
- **`dev_lookahead_wide_ms`** — wideband lookahead (`lookaheadWide_` delay + wide envelope peak-hold windows).

DEV-only (orange strip), like the release params. The FinalCeiling brickwall lookahead stays **fixed** (safety net — do not touch).

### Critical design constraints
1. **Delay must equal its envelope's look-ahead window.** For each stage the audio delay and the `LimiterEnvelope` peak-hold horizon are the same lookahead — they must move together or the limiter overshoots. So each knob sets *both* the delay-line `setDelaySamples` and the envelope's active lookahead window.
2. **Constant reported latency.** Lookahead = latency; changing reported latency mid-playback forces the host (Ableton) to re-run PDC (mutes/re-syncs) — unacceptable for live tuning. So: **report a fixed latency sized to the MAX lookahead**, pre-allocate to MAX, and pad the wet path by `(max − active)` so total through-latency is constant regardless of knob position.
3. Knob moves may produce a small click at the moment of change (delay-offset jump + envelope-window reset) — **acceptable for DEV**; the eventual user version will smooth it.

---

## Allowed files to touch
```
# SDK
melechdsp-hq/shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h
melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp
# Plugin
Source/parameters/ParameterIDs.h
Source/parameters/Parameters.cpp
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/ui/MainView.h
Source/ui/MainView.cpp
docs/SIGNAL_FLOW.md
docs/PROGRESS.md
PROMPTS/PLAN.md
PROMPTS/SLICE_LOOKAHEAD_DEV_KNOBS_CLOSE.md   (new, at close)
```
Do **not** touch the FinalCeilingLimiter, oversampler, or crossover.

---

## 1. SDK — `LimiterEnvelope` runtime-adjustable lookahead window
Today `prepare()` fixes `lookaheadSamples_` and sizes `ext_`/`historyPost_`/`attackTable_` to it. Split **allocation size (max)** from **active window**:

### Header
```cpp
void setActiveLookaheadSamples (int samples) noexcept;   // clamp [1, maxLookaheadSamples_]
int  getActiveLookaheadSamples() const noexcept { return lookaheadSamples_; }
...
int maxLookaheadSamples_ = 240;   // allocation ceiling, set in prepare()
```

### Impl
- In `prepare()`: set `maxLookaheadSamples_ = spec.lookaheadSamples;` and size all buffers to `maxLookaheadSamples_` (as today). Initialize `lookaheadSamples_ = maxLookaheadSamples_`.
- `setActiveLookaheadSamples(int n)`:
  ```cpp
  const int clamped = std::clamp (n, 1, maxLookaheadSamples_);
  if (clamped == lookaheadSamples_) return;
  lookaheadSamples_ = clamped;
  recomputeAttackSamples();      // attackSamples_ = min(lookaheadSamples_, modeAttackTime)
  refreshAttackTable();
  // reset the carried lookahead history so the shorter/longer window starts clean
  std::fill (historyPost_.begin(), historyPost_.end(), 1.0f);
  // (laMinDeque_/laStages_ need no resize — they already cover max; the active window just uses fewer)
  ```
- **`process()` must use `lookaheadSamples_` (active) everywhere it currently uses the lookahead count** (`la`), including: the `ext_` fill/peak-hold span, the `historyPost_` carry length, and the LookaheadFollower window-min width. Since all buffers are sized to `maxLookaheadSamples_ ≥ lookaheadSamples_`, indexing with the active count is safe. Double-check the `historyPost_` carry loop uses `la = lookaheadSamples_` (carry only the active tail).
- RT-safe: no allocation in `setActiveLookaheadSamples` or `process()`.

> Sanity: with the active window == max (the default), behavior must be byte-identical to today.

---

## 2. Plugin — constant-latency lookahead with padding (`PluginProcessor.h/.cpp`)

### 2a. Max lookahead + allocation (prepareToPlay)
```cpp
static constexpr float kMaxLookaheadMs = 12.0f;     // allocation/latency ceiling (header)
```
In `prepareToPlay`, size everything to MAX:
```cpp
const int baseMaxLookahead = (int) std::llround (kMaxLookaheadMs * 1e-3 * sampleRate);
const int osMaxLookahead   = baseMaxLookahead * osFactor;

lookahead_.prepare (2, osMaxLookahead);
lookaheadWide_.prepare (2, osMaxLookahead);
lookaheadPad_.prepare (2, 2 * osMaxLookahead);      // NEW: wet-path padding delay (OS rate)

spec.lookaheadSamples = osMaxLookahead;              // envelopes allocate to MAX
// ... envelope_.prepare(spec) etc.
```
**Reported latency uses MAX** (constant): replace the current `baseLookaheadSamples` contribution with `baseMaxLookahead` for both stages, so `baseLatencySamples_` reflects `2 × baseMaxLookahead + osLatency + finalCeiling`. Set `dryDelay_` to that constant latency as today.

### 2b. New padding delay line
Add member `mdsp_dsp::LookaheadDelay<float> lookaheadPad_;`. It delays the **wet OS signal** by the slack `(osMaxLookahead − laBand) + (osMaxLookahead − laWide)` so total wet through-latency stays constant. Apply it on `osBlock` **right before `processSamplesDown`** (after the wideband gain/ceiling write, before line ~1022):
```cpp
const int padSamples = (osMaxLookahead - laBandActive) + (osMaxLookahead - laWideActive);
lookaheadPad_.setDelaySamples (padSamples);
for (int i = 0; i < osN; ++i)
    for (int ch = 0; ch < nch; ++ch)
        osBlock.getChannelPointer((size_t) ch)[i] = lookaheadPad_.pushPop (ch, osBlock.getChannelPointer((size_t) ch)[i]);
```
(If `nch == 1`, still push a 0 through ch 1 to keep its state coherent, mirroring the existing dry-delay mono handling.)

### 2c. Read DEV knobs + drive both stages (in the configure block ~L774–823)
```cpp
const float devLaBandMs = devLookaheadBandMs_ ? devLookaheadBandMs_->load (relaxed) : kLookaheadMs;
const float devLaWideMs = devLookaheadWideMs_ ? devLookaheadWideMs_->load (relaxed) : kLookaheadMs;
const int laBandActive = juce::jlimit (1, osMaxLookahead_, (int) std::llround (devLaBandMs * 1e-3 * osSampleRate_));
const int laWideActive = juce::jlimit (1, osMaxLookahead_, (int) std::llround (devLaWideMs * 1e-3 * osSampleRate_));

lookahead_.setDelaySamples (laBandActive);
lookaheadWide_.setDelaySamples (laWideActive);
envelopeLow_.setActiveLookaheadSamples (laBandActive);
envelopeHigh_.setActiveLookaheadSamples (laBandActive);
envelope_.setActiveLookaheadSamples (laWideActive);
envelope_R_.setActiveLookaheadSamples (laWideActive);
```
(Store `osMaxLookahead_` and `osSampleRate_` as members in prepareToPlay so the block can read them.)

> Alignment check: per-band stage now delays audio by `laBandActive` and the low/high envelopes look ahead `laBandActive` — equal ✓. Same for wideband. The pad restores constant total latency ✓.

---

## 3. Params (`ParameterIDs.h`, `Parameters.cpp`)
Add two DEV params:
```cpp
inline constexpr std::string_view dev_lookahead_band_ms { "dev_lookahead_band_ms" };
inline constexpr std::string_view dev_lookahead_wide_ms { "dev_lookahead_wide_ms" };
```
Register both:
| ID | Type | Range | Default |
|---|---|---|---|
| `dev_lookahead_band_ms` | Float (ms) | 1.0 – 12.0 | 7.0 |
| `dev_lookahead_wide_ms` | Float (ms) | 1.0 – 12.0 | 7.0 |

Cache raw pointers (`devLookaheadBandMs_`, `devLookaheadWideMs_`) + jasserts, like the other dev params.

### 3a. Remove the dead `lookahead_ms` param (cleanup)
Read nowhere (verified: only its own registration references it). Delete it:
- `Parameters.cpp` — remove the `lookahead_ms` `AudioParameterFloat` registration (~L147–152).
- `ParameterIDs.h` — remove the `lookahead_ms` declaration (~L26).
Safe: no preset/state/UI references it; APVTS is keyed by string ID, so old saved states just ignore the absent node. The new LA Band/Wide knobs are the real lookahead control.

### 3b. Expose `release_sustain_ratio` as a DEV knob (hidden working param — no registration change)
This param already exists (default 4.0, range 1–10) and **is read by the DSP** (`PluginProcessor.cpp:841`, manual-release sustain split) but has **no control** — a hidden working knob locked at 4.0. It only affects audio when **Release Auto = Off**. Just add a UI control + attachment (see §4); do NOT re-register it.

---

## 4. UI (`MainView.h/.cpp`)
Add to the DEV strip (or a small "DEV LOOKAHEAD" sub-row if the release strip is full), all APVTS-attached, matching the existing dev-knob styling:
- **LA Band (ms)** → `dev_lookahead_band_ms`
- **LA Wide (ms)** → `dev_lookahead_wide_ms`
- **Sustain Ratio** → `release_sustain_ratio` (label/tooltip note: active only when Release Auto = Off)

---

## 5. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -20
cp -R build/MasterLimiter_artefacts/Release/AU/MasterLimiter.component  ~/Library/Audio/Plug-Ins/Components/
cp -R build/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3     ~/Library/Audio/Plug-Ins/VST3/
auval -v aufx MaLm Melc 2>&1 | tail -5
```
Acceptance:
1. Builds clean (SDK + plugin); AU validates.
2. Both knobs at **7.0** → behavior identical to current (max-window default path, padding = `2·(osMax − 7ms-equiv)`, latency constant).
3. Sweeping **LA Band** changes per-band attack/onset; **LA Wide** changes the wideband catch — audibly, and visibly on the **history graph** GR trace.
4. **Reported latency does NOT change** when either knob moves (check the host PDC value / no re-sync mute). Null-test against a fixed setting confirms delay-compensation is constant.
5. No overshoot: output true-peak still respects the ceiling at any knob setting (the FinalCeiling safety is untouched).
6. RT-safe: no allocations on the audio thread; CPU still ~7–10%.

Close gate: update `docs/SIGNAL_FLOW.md` (§1 lookahead now has two DEV-tunable stages, FinalCeiling still fixed; §5 remove the `lookahead_ms` row and note `release_sustain_ratio` is now a DEV knob; §8 drop the vestigial-`lookahead_ms` item — it's removed), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit + push **SDK first then plugin**; archive `PROMPTS/SLICE_LOOKAHEAD_DEV_KNOBS_CLOSE.md`.

---

## Notes
- Default 7 ms reproduces today's voicing; the constant-latency padding means at 7/7 the pad just absorbs the (12−7) ms × 2 of slack.
- Click-on-knob-move is expected (delay jump + window reset). For the future **user** version we'd crossfade the delay change and ramp the window — out of scope here.
- This finally lets the history graph show how lookahead length trades attack smoothness vs transient catch — pair it with the LA Release tuning you're already doing.
```
