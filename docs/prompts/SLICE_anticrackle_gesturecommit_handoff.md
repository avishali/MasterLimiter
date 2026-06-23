# Cursor handoff — Anti-crackle slice 1.5: GESTURE-based commit for heavy DEV controls

**Repo:** `MasterLimiter` plugin only. On top of `4c4109f` (the time-based debounce). SDK unchanged.

## Why

The 120 ms time-based debounce (`4c4109f`) only coalesces changes that arrive faster than
120 ms apart. The **atten** control is coarse (range 48–72 dB, 1 dB steps = 24 steps), so it's
moved in deliberate, spaced steps >120 ms apart → the timer commits *between* steps → one
kernel rebuild + bank swap → **a click on every step**. (Atten's click is the loudest because
stop-band attenuation swings the FIR tap count the most.) Cutoff/transition feel clean only
because they're fine-grained and dragged continuously.

Fix: commit on **gesture end** (slider mouse-up) regardless of step spacing. While the user is
dragging ANY heavy param, hold the audio path; commit once when the gesture ends. Keep the
time-based debounce as the fallback for host automation / typed values (which don't fire UI
gestures).

This does NOT remove the single click on release — that's the crossfade slice (slice 2, next).
It turns "click every step" into "one click per drag".

## Design

JUCE's `SliderAttachment` calls `beginChangeGesture()` / `endChangeGesture()` around a drag.
Those reach the processor via `juce::AudioProcessorParameter::Listener::parameterGestureChanged`
(distinct from the APVTS `parameterChanged` value callback we already use). Register that
listener on the 5 heavy params; commit on gesture end; suppress the timer while a gesture is
active.

⚠️ `parameterGestureChanged` may not be on the message thread for all hosts → route the commit
through `triggerAsyncUpdate()` (message-thread) rather than calling `rebuildCrossoverKernels()`
(which spins the bank lock) directly from the callback.

### `PluginProcessor.h`
- Add base `private juce::AudioProcessorParameter::Listener`.
- Declare:
  ```cpp
  void parameterValueChanged (int, float) override {}            // unused; APVTS handles values
  void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;
  void commitHeavyControls();                                    // message thread only

  std::atomic<int>  heavyGestureActive_       { 0 };            // nesting count of active heavy gestures
  std::atomic<bool> heavyGestureCommitPending_ { false };
  ```

### Register / unregister (constructor near the existing `addParameterListener` block; destructor)
```cpp
for (auto id : { param::dev_xover_cutoff_hz, param::dev_xover_transition_hz, param::dev_xover_atten_db,
                 param::dev_lookahead_band_ms, param::dev_lookahead_wide_ms })
    if (auto* p = apvts.getParameter (id.data())) p->addListener (this);
// …matching p->removeListener(this) in the destructor, BEFORE other teardown.
```

### Extract the commit (reuse from the timer)
Pull the current `timerCallback` body into a helper so both paths share it:
```cpp
void MasterLimiterAudioProcessor::commitHeavyControls()   // message thread only
{
    if (heavyCrossoverDirty_.exchange (false, std::memory_order_acq_rel))
        rebuildCrossoverKernels();

    if (heavyLookaheadDirty_.exchange (false, std::memory_order_acq_rel))
    {
        committedLookaheadBandMs_.store (devLookaheadBandMs_ != nullptr ? devLookaheadBandMs_->load (std::memory_order_relaxed) : kLookaheadMs, std::memory_order_release);
        committedLookaheadWideMs_.store (devLookaheadWideMs_ != nullptr ? devLookaheadWideMs_->load (std::memory_order_relaxed) : kLookaheadMs, std::memory_order_release);
    }
}
```

### `parameterGestureChanged`
```cpp
void MasterLimiterAudioProcessor::parameterGestureChanged (int, bool gestureIsStarting)
{
    if (gestureIsStarting)
    {
        heavyGestureActive_.fetch_add (1, std::memory_order_acq_rel);
    }
    else
    {
        const int prev = heavyGestureActive_.fetch_sub (1, std::memory_order_acq_rel);
        if (prev <= 1)   // last heavy gesture just ended
        {
            heavyGestureActive_.store (0, std::memory_order_release);   // clamp (defensive vs unbalanced begin/end)
            heavyGestureCommitPending_.store (true, std::memory_order_release);
            triggerAsyncUpdate();
        }
    }
}
```

### `timerCallback` — suppress while a gesture is active; use the helper
```cpp
void MasterLimiterAudioProcessor::timerCallback()
{
    if (heavyGestureActive_.load (std::memory_order_acquire) > 0)
        return;   // a drag is in progress — gesture-end will commit

    const auto nowMs = juce::Time::getMillisecondCounter();
    if (nowMs - lastHeavyChangeMs_.load (std::memory_order_acquire) < (juce::uint32) kHeavyDebounceMs)
        return;

    commitHeavyControls();
    stopTimer();
}
```

### `handleAsyncUpdate` — gesture-end commit + don't start the timer during a gesture
```cpp
if (heavyGestureCommitPending_.exchange (false, std::memory_order_acq_rel))
{
    commitHeavyControls();
    stopTimer();
}
else if ((heavyCrossoverDirty_.load (std::memory_order_acquire)
          || heavyLookaheadDirty_.load (std::memory_order_acquire))
         && heavyGestureActive_.load (std::memory_order_acquire) == 0
         && ! isTimerRunning())
{
    startTimer (kHeavyPollMs);
}

commitLearnedRef();   // keep
```

## Behaviour after this
- **UI drag (any speed / step spacing):** zero commits during the drag; one commit on
  mouse-up. → atten "click every step" becomes one click per drag.
- **Host automation playback / typed value (no gesture):** unchanged — time-based 120 ms
  debounce commits after the value settles.

## Watch-outs
- `parameterValueChanged` (the AudioProcessorParameter::Listener one) is called on the audio
  thread — keep it a no-op; value arming stays in the existing APVTS `parameterChanged`.
- Don't call `rebuildCrossoverKernels()` directly from `parameterGestureChanged` (it spins the
  bank lock; the callback thread isn't guaranteed message-thread). Route via `triggerAsyncUpdate`.
- Guard the gesture counter against unbalanced begin/end (the `prev <= 1` + clamp above).
- Latency unchanged; light controls still immediate.

## Gate
- [ ] **Live Release DAW:** step the **atten** slider one notch at a time (slowly) under
      playback — **no click per step**; audio holds until you release, then one soft transition.
      Repeat for cutoff, transition, lookahead band/wide (slow stepping AND fast drag).
- [ ] Host automation of a heavy param still applies (time-based path) — a recorded/played
      automation move updates the sound after it settles.
- [ ] Crossover still doesn't crash on drag (`23b059c` regression); host latency unchanged;
      Color recon ±0.0000 at static settings; `mdsp_dsp_tests` exit 0; Release build clean.

## Report back
- Confirm atten slow-stepping no longer clicks per step (one click per gesture remains, expected
  — crossfade slice removes it). Host + how exercised.
- Commit message: `feat: gesture-commit heavy DEV controls (hold until slider release)`.
- Do NOT push (Quell hold).
