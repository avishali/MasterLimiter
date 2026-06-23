# Cursor handoff — Anti-crackle slice 1: DEBOUNCE the heavy DEV controls

**Repo:** `MasterLimiter` plugin only. On top of `23b059c`. SDK unchanged.

## Goal

The heavy DEV controls crackle when dragged because the change is applied continuously
mid-stream:
- **Crossover** (`dev_xover_cutoff_hz` / `dev_xover_transition_hz` / `dev_xover_atten_db`):
  every change rebuilds the FIR kernel + swaps banks.
- **Lookahead** (`dev_lookahead_band_ms` / `dev_lookahead_wide_ms`): `processCore` reads the
  RAW param every block (`PluginProcessor.cpp:1122-1124`) and `setDelaySamples` jumps the
  delay read-position sample-to-sample.

User explicitly wants **no immediate real-time response** — so **debounce**: while the user
is moving a heavy control, change NOTHING in the audio path; commit ONCE, ~120 ms after the
control settles. This removes the during-drag crackle entirely. (A short equal-power
crossfade to remove the single on-settle transition is a SEPARATE follow-up slice — out of
scope here.)

This is independent of the crossover lock (`23b059c`) — keep that; debounce just reduces how
often the swap fires.

## Design — message-thread idle-debounce timer, thread-safe arming

`parameterChanged` may be called from the **audio thread** (host automation), so do NOT call
`startTimer()` there. Arm via the existing thread-safe `triggerAsyncUpdate()`; own the timer
on the message thread.

### `PluginProcessor.h`
- Add `private juce::Timer` to the base list (currently `AudioProcessor`,
  `APVTS::Listener`, `AsyncUpdater`).
- Add:
  ```cpp
  void timerCallback() override;

  std::atomic<bool>          heavyCrossoverDirty_ { false };
  std::atomic<bool>          heavyLookaheadDirty_ { false };
  std::atomic<juce::uint32>  lastHeavyChangeMs_   { 0 };
  std::atomic<float>         committedLookaheadBandMs_ { 0.0f };
  std::atomic<float>         committedLookaheadWideMs_ { 0.0f };

  static constexpr int kHeavyDebounceMs = 120;  // commit this long after the control settles
  static constexpr int kHeavyPollMs     = 30;   // timer poll interval
  ```
  (`heavyCrossoverDirty_` supersedes the old `crossoverRedesignPending_` flow — remove that
  member and its uses.)

### Register lookahead listeners (constructor ~131, destructor ~142)
The lookahead params currently have NO listener. Add them so changes arm the debounce:
```cpp
apvts.addParameterListener (param::dev_lookahead_band_ms.data(), this);
apvts.addParameterListener (param::dev_lookahead_wide_ms.data(), this);
// …and matching removeParameterListener in the destructor.
```

### `parameterChanged` (any thread) — arm, don't apply
Replace the crossover branch (currently sets `crossoverRedesignPending_` +
`triggerAsyncUpdate`) and add a lookahead branch:
```cpp
if (parameterID == param::dev_xover_cutoff_hz.data()
    || parameterID == param::dev_xover_transition_hz.data()
    || parameterID == param::dev_xover_atten_db.data())
{
    heavyCrossoverDirty_.store (true, std::memory_order_release);
    lastHeavyChangeMs_.store (juce::Time::getMillisecondCounter(), std::memory_order_release);
    triggerAsyncUpdate();   // thread-safe; bounces to the message thread to manage the timer
    return;
}
if (parameterID == param::dev_lookahead_band_ms.data()
    || parameterID == param::dev_lookahead_wide_ms.data())
{
    heavyLookaheadDirty_.store (true, std::memory_order_release);
    lastHeavyChangeMs_.store (juce::Time::getMillisecondCounter(), std::memory_order_release);
    triggerAsyncUpdate();
    return;
}
```
`juce::Time::getMillisecondCounter()` is cheap/RT-safe (no alloc/lock).

### `handleAsyncUpdate` (message thread) — ensure the timer is running
Replace the old `if (crossoverRedesignPending_.exchange(false)) rebuildCrossoverKernels();`
with:
```cpp
if ((heavyCrossoverDirty_.load (std::memory_order_acquire)
     || heavyLookaheadDirty_.load (std::memory_order_acquire))
    && ! isTimerRunning())
    startTimer (kHeavyPollMs);

commitLearnedRef();   // keep existing
```

### `timerCallback` (message thread) — commit once settled
```cpp
void MasterLimiterAudioProcessor::timerCallback()
{
    const auto nowMs = juce::Time::getMillisecondCounter();
    if (nowMs - lastHeavyChangeMs_.load (std::memory_order_acquire)
        < (juce::uint32) kHeavyDebounceMs)
        return;   // still settling — keep polling

    if (heavyCrossoverDirty_.exchange (false, std::memory_order_acq_rel))
        rebuildCrossoverKernels();   // lock-safe (23b059c)

    if (heavyLookaheadDirty_.exchange (false, std::memory_order_acq_rel))
    {
        committedLookaheadBandMs_.store (devLookaheadBandMs_->load (std::memory_order_relaxed), std::memory_order_release);
        committedLookaheadWideMs_.store (devLookaheadWideMs_->load (std::memory_order_relaxed), std::memory_order_release);
    }

    stopTimer();
}
```

### `processCore` — read the COMMITTED lookahead, not the raw param
Lines ~1122-1124: replace the raw `devLookaheadBandMs_->load()` / `devLookaheadWideMs_->load()`
reads with:
```cpp
const float devLaBandMs = committedLookaheadBandMs_.load (std::memory_order_acquire);
const float devLaWideMs = committedLookaheadWideMs_.load (std::memory_order_acquire);
```
(Keep the existing null-guards / `jlimit` clamping that follow.)

### `prepareToPlay` — seed committed values
After the lookahead param pointers are bound (~264-265), initialise the committed atomics so
audio uses the correct lookahead before any change:
```cpp
committedLookaheadBandMs_.store (devLookaheadBandMs_ ? devLookaheadBandMs_->load() : (float) kDevLookaheadBandDefault, std::memory_order_release);
committedLookaheadWideMs_.store (devLookaheadWideMs_ ? devLookaheadWideMs_->load() : (float) kDevLookaheadWideDefault, std::memory_order_release);
```
(use whatever the existing default constants are).

### Teardown
`stopTimer()` in the destructor (and `releaseResources` if present) so it can't fire after
teardown.

## Watch-outs
- `parameterChanged` runs on arbitrary threads → all cross-thread state here is `std::atomic`
  (done above). The timer state machine is message-thread only.
- Do NOT debounce the light controls (attack/release-engine/sigma/scales/sustain) — they
  don't crackle; leave them immediate.
- Latency must NOT change (lookahead already uses fixed reserved latency + `lookaheadPad_`
  compensation; committing a new value just moves the internal tap — same as today, only less
  often). Verify reported host latency is unchanged.
- **Known limitation (acceptable for DEV controls):** continuous host automation of a heavy
  param won't commit until the automation pauses (idle-debounce). Fine for voicing knobs.
  If undesirable later, add an optional "force-commit if dirty > 250 ms" — OUT OF SCOPE now.
- The on-settle transition (one soft click when the value commits) is expected — the crossfade
  follow-up slice removes it. Do not try to solve it here.

## Gate
- [ ] **Live Release DAW:** drag each heavy param (cutoff, transition, atten, lookahead band,
      lookahead wide) slowly and fast under playback — **no crackle DURING the drag** (audio
      holds the pre-drag setting). On release, the new setting applies ~120 ms later with at
      most one soft transition. State host + that each param was exercised.
- [ ] Crossover still does not crash on drag (regression guard on `23b059c`).
- [ ] Host latency unchanged before/after committing any heavy param.
- [ ] Color reconstruction still ±0.0000 dB at static settings; full `mdsp_dsp_tests` exit 0;
      Release build clean (AU/VST3/Standalone).
- [ ] Light DEV controls still respond immediately (sanity: attack/release knobs unaffected).

## Report back
- Confirm no during-drag crackle for all five heavy params (host + how exercised) and the
  ~120 ms settle feel.
- Confirm latency unchanged + Color recon flat + no crash.
- Commit message: `feat: debounce heavy DEV controls (crossover + lookahead) to kill drag crackle`.
- Do NOT push (Quell hold; SDK+plugin go up together later).
