# SLICE — Lookahead Envelope-Follower Release (A/B mode)

**Status:** ready for Cursor · **Owner build/audition:** avishali · **Architect:** Claude
**Repos:** SDK `melechdsp-hq` (master) + plugin `MasterLimiter` (main) · commit + push BOTH after the slice.

---

## Goal
Add a **new auto-release engine** to `LimiterEnvelope` — a *lookahead envelope-follower* — gated behind a **dev toggle** so avishali can A/B it live against the current adaptive-sigma release. **Attack is unchanged.** Only the release/recovery half is new. Default toggle = **Adaptive**, so existing behavior is byte-identical until switched.

### Why
The current auto-release (`LimiterEnvelope::process()`, autoRelease branch) blends a fast/slow alpha by `sigma` — **the rate-switching itself is the audible artifact**. The new engine removes rate-switching:
1. **Recovery is gated by the minimum gain visible within the lookahead window** → it only recovers in genuine gaps. Program-dependence comes from the signal, not a guessed time constant.
2. **Recovery slope is a fixed-time N-pole cascade** → the same smooth S-curve every release.

Because the window-min gating already prevents pumping, the **release time wants to be SHORT** (much shorter than the adaptive slow-ceiling of 1200/600/250 ms). So the release time is exposed as a **direct dev knob** (`dev_la_release_ms`) for tuning — default low. We map it to the Auto-mode selector later, *after* avishali finds the sweet spot.

---

## Allowed files to touch
```
# SDK (sibling repo — the one the build actually compiles)
/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h
/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp
# Plugin
/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/Source/parameters/ParameterIDs.h
/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/Source/parameters/Parameters.cpp
/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/Source/PluginProcessor.h
/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/Source/PluginProcessor.cpp
/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/Source/PluginEditor.h
/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/Source/PluginEditor.cpp
# Docs (close gate)
/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/docs/PROGRESS.md
/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/PROMPTS/PLAN.md
```
Do **not** touch `processCore` signal flow, the oversampler, crossover, FinalCeilingLimiter, or the frozen product param IDs.

---

## 1. SDK — `LimiterEnvelope.h`
Add (mirror the existing setter style):
```cpp
enum class ReleaseEngine { AdaptiveSigma, LookaheadFollower };
void setReleaseEngine (ReleaseEngine engine) noexcept;   // default AdaptiveSigma
void setLookaheadReleaseMs (float releaseMs) noexcept;    // direct recovery time
void setLookaheadReleasePoles (int poles) noexcept;       // clamp 2..4
```
Private state:
```cpp
ReleaseEngine releaseEngine_ = ReleaseEngine::AdaptiveSigma;
float lookaheadReleaseMs_ = 80.0f;
int   lookaheadPoles_ = 3;
float laReleaseAlpha_ = 1.0f;                 // recomputed from ms, poles, SR
std::array<float, 4> laStages_ { { 1.0f, 1.0f, 1.0f, 1.0f } };   // cascade state (use lookaheadPoles_)
// Sliding-window-min (Lemire monotonic deque) over ext_, width = lookaheadSamples_:
std::vector<float> laMinOut_;                 // preallocate maxBlockSize_ in prepare()
std::vector<int>   laMinDeque_;               // index ring, preallocate (lookaheadSamples_ + 2)
```
Add `void recomputeLookaheadRelease() noexcept;`. Call it from `prepare()`, `setLookaheadReleaseMs`, `setLookaheadReleasePoles`, and on SR change. In `reset()`: set `laStages_` to 1.0 and clear the deque state.

**Alpha** (normalized by pole count so perceived time stays ≈constant as poles change — poles becomes a pure *smoothness* control, not a time control):
```cpp
const float tauTotalSamp = std::max (1.0f, lookaheadReleaseMs_ * 1.0e-3f * (float) sampleRate_);
const float tauPerPole   = tauTotalSamp / (float) lookaheadPoles_;
laReleaseAlpha_ = std::exp (-1.0f / tauPerPole);
```

---

## 2. SDK — `LimiterEnvelope.cpp`
**Leave the entire `ext_` build / attack peak-hold loop (the `attackTable_` cosine min-scan, ~L231–315) untouched.** It already produces `ext_[j]` = per-sample target gain coefficient (≤1) with smooth lookahead attack.

In `process()`, branch the release section on `releaseEngine_`:
- `AdaptiveSigma` → existing autoRelease loop (~L324–361), **unchanged**.
- `LookaheadFollower` → new loop below.

### 2a. Sliding-window minimum over the lookahead horizon
For each output sample `j ∈ [0, n)`, compute `relCeil[j] = min(ext_[j .. j+la])` where `la = lookaheadSamples_`. The future tail already lives in `ext_[n .. n+la-1]`. Use **Lemire's monotonic-deque** algorithm against the preallocated `laMinDeque_` index ring — **O(n), zero allocation, RT-safe**. Reference shape:
```cpp
// window covers [j, j+la]; ext_ valid over [0, n+la)
int head = 0, tail = 0;           // indices into laMinDeque_ used as a ring/wedge
auto push = [&](int i) {
    while (tail > head && ext_[(size_t) laMinDeque_[tail-1]] >= ext_[(size_t) i]) --tail;
    laMinDeque_[tail++] = i;
};
// prime with the first window [0, la]
for (int i = 0; i <= la && i < n + la; ++i) push (i);
for (int j = 0; j < n; ++j) {
    while (laMinDeque_[head] < j) ++head;                 // drop indices that fell out of [j, j+la]
    laMinOut_[(size_t) j] = ext_[(size_t) laMinDeque_[head]];
    const int next = j + la + 1;
    if (next < n + la) push (next);
}
```
(Size `laMinDeque_` to `n + la + 2` worst case — preallocate `maxBlockSize_ + lookaheadSamples_ + 2`. Adapt the exact bookkeeping; the requirement is correct windowed-min, no allocation, no UB at block edges.)

### 2b. Release loop
```cpp
const int poles = std::clamp (lookaheadPoles_, 2, 4);
float maxGr = 0.0f;
float g = laStages_[(size_t) (poles - 1)];     // current smoothed gain
for (int j = 0; j < n; ++j)
{
    const float instant = ext_[(size_t) j];     // attack target (already cosine-smoothed)
    if (instant < g - 1.0e-12f)
    {
        // ATTACK: follow down now; reset cascade so the next release starts from here
        for (int p = 0; p < poles; ++p) laStages_[(size_t) p] = instant;
        g = instant;
    }
    else
    {
        // RELEASE: rise toward the window-min ceiling through the fixed N-pole cascade.
        // relCeil[j] <= instant, so we never recover into a peak that is already visible.
        float x = laMinOut_[(size_t) j];
        for (int p = 0; p < poles; ++p)
        {
            laStages_[(size_t) p] = laReleaseAlpha_ * laStages_[(size_t) p] + (1.0f - laReleaseAlpha_) * x;
            x = laStages_[(size_t) p];
        }
        g = x;
    }
    g = std::clamp (g, 1.0e-12f, 1.0f);
    gainOut[j] = g;
    const float gr = 20.0f * std::log10 (std::max (1.0e-12f, 1.0f / g));
    if (gr > maxGr) maxGr = gr;
}
lastBlockMaxGrDb_ = maxGr;
// then update historyPost_ from ext_[n + k] exactly as the existing path does
```

---

## 3. Plugin params
`ParameterIDs.h` — add three (dev block, **not** frozen product IDs):
```cpp
inline constexpr std::string_view dev_release_engine   { "dev_release_engine" };
inline constexpr std::string_view dev_la_release_ms    { "dev_la_release_ms" };
inline constexpr std::string_view dev_la_release_poles { "dev_la_release_poles" };
```
`Parameters.cpp` — register beside the existing dev params:
| ID | Type | Range | Default |
|---|---|---|---|
| `dev_release_engine` | Choice `{"Adaptive","Lookahead"}` | — | index 0 (Adaptive) |
| `dev_la_release_ms` | Float (ms) | **5.0 – 400.0** (skew toward low end, e.g. skew ~0.4) | **80.0** |
| `dev_la_release_poles` | Choice `{"2","3","4"}` | — | index 1 (=3) |

---

## 4. Plugin wiring — `PluginProcessor.h/.cpp`
- Cache raw pointers like the other dev params (`devReleaseEngine_`, `devLaReleaseMs_`, `devLaReleasePoles_`) + matching `jassert`s (~L105–108).
- In `processCore`'s `configureEnvelope` block (~L774–796), read the three values and apply them. **Release time is the direct dev knob** (no Auto-mode derivation for now). The existing per-band low scale (`lowReleaseScale`, def 3×) still multiplies the low band's release time so band character carries:
```cpp
const int   laReleaseEngineIdx = devReleaseEngine_ ? (int) devReleaseEngine_->load() : 0;
const float laReleaseMs        = devLaReleaseMs_   ? devLaReleaseMs_->load()         : 80.0f;
const int   laReleasePoles     = 2 + (devLaReleasePoles_ ? (int) devLaReleasePoles_->load() : 1);
const auto  laEngine = (laReleaseEngineIdx == 1)
    ? mdsp_dsp::LimiterEnvelope::ReleaseEngine::LookaheadFollower
    : mdsp_dsp::LimiterEnvelope::ReleaseEngine::AdaptiveSigma;
```
Add to the `configureEnvelope` lambda (it already receives `autoReleaseScale` per band — reuse it to scale the LA time):
```cpp
envelope.setReleaseEngine (laEngine);
envelope.setLookaheadReleaseMs (laReleaseMs * autoReleaseScale);   // low band inherits the 3x
envelope.setLookaheadReleasePoles (laReleasePoles);
```
Applies to all four envelopes (`envelope_`, `envelope_R_`, `envelopeLow_`, `envelopeHigh_`) via the existing four `configureEnvelope(...)` calls.

---

## 5. UI — `PluginEditor.cpp/.h`
Extend the existing **DEV RELEASE** strip with three controls attached to the new params:
- **Engine** toggle/combo — `Adaptive | Lookahead`
- **LA Release** knob (ms) — the time to sweep
- **Poles** selector — `2 | 3 | 4`

Match the existing dev-strip layout/aesthetic. Leave the legacy sigma knobs visible (they drive Adaptive mode for A/B).

---

## 6. Build & verify (Release)
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -20
cp -R build/MasterLimiter_artefacts/Release/AU/MasterLimiter.component  ~/Library/Audio/Plug-Ins/Components/
cp -R build/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3     ~/Library/Audio/Plug-Ins/VST3/
```
Acceptance:
1. Builds clean (Release).
2. Engine = **Adaptive** → bit-identical to current behavior (toggle is inert until switched).
3. Engine = **Lookahead** → no zipper/clicks; smooth recovery; sweeping **LA Release** changes recovery speed; **Poles** changes smoothness without changing perceived time.
4. **RT-safe**: no allocations in `process()` (deque + cascade all use preallocated state).

---

## 7. Close gate
1. Update `docs/PROGRESS.md` + `PROMPTS/PLAN.md` (new release engine, 3 dev params, A/B + tuning plan).
2. Commit + push **SDK `melechdsp-hq` first**, then plugin `MasterLimiter`. Never leave a slice unpushed.
3. Archive this prompt → `PROMPTS/SLICE_LOOKAHEAD_RELEASE_CLOSE.md` per convention.

---

## After Cursor lands it — audition loop (avishali)
On the acoustic mix at **Color 100, Auto Transparent**:
1. A/B the **Engine** toggle (Adaptive vs Lookahead) — is the rate-switching grit gone?
2. With Lookahead on, sweep **LA Release** from low up (start ~20–40 ms; the gating means short times should already sound clean — confirm where it starts to pump vs where it dulls).
3. Try **Poles** 2 / 3 / 4 for smoothness.
4. Report the winning **LA Release ms + Poles** → Claude bakes constants, maps time onto the Auto-mode selector, and strips dev params for 0.4.
