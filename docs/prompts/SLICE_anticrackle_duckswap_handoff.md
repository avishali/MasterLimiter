# Cursor handoff — Anti-crackle slice 2: DUCK-AND-SWAP the crossover commit

**Repo:** `MasterLimiter` plugin only. On top of `9ae248a`. SDK unchanged.

## Why / what

Gesture-commit (`9ae248a`) reduced atten to one commit per drag, but **each commit still
clicks** — the hard crossover bank swap is a discontinuity (kernel change + `reset()` clears
the conv state AND the full-band `xDelayed` delay line). User chose **duck-and-swap**: hide the
swap under a short output dip instead of a click.

**Key constraint (drives the design):** the crossover FIR is long (up to ~14k taps at the 4×
rate). After `reset()`, the new bank's output — including `xDelayed` — is invalid until its
delay lines refill (≈ `crossoverOsLatencySamples_`, ~37 ms). So:

- **fade OUT** the multiband output over ~5 ms,
- **swap** at the gain minimum (≈0, so no audible discontinuity),
- **fade IN** over the **refill length** (~`crossoverOsLatencySamples_`) so the new bank's
  warm-up is masked by the rising gain.

Single bank, ~zero extra steady CPU. Audible cost: a brief dip/morph on each commit (only on
gesture-end, so rare). Keep the `23b059c` lock for the swap itself.

## Where

`processCore`, the multiband section: `trySwapCrossoverBank()` at ~1083, `xoBank` latched at
~1084, detect loop ~1268-1291, **apply loop ~1300-1321** (writes `bandLimitedBuf_`). The duck
gain multiplies the apply-loop output; the swap moves into the apply-loop state machine.

## Design — block-latched bank, per-sample duck gain, swap at the fade-out floor

The current swap is instant in `trySwapCrossoverBank`. Replace it with a 3-phase state machine
driven inside the **apply loop** (so the gain and the swap stay sample-accurate together).
`xoBank` stays latched per block (as today); the actual `activeCrossoverBank_` flip happens
mid-loop at gain≈0, and takes audible effect on the NEXT block's latch — fine, because the
swap point is muted.

### `PluginProcessor.h` — state (audio-thread owned)
```cpp
int   xoDuckPhase_     = 0;     // 0 = Idle, 1 = FadeOut, 2 = FadeIn
int   xoDuckPos_       = 0;     // OS samples elapsed in the current phase
int   xoFadeOutSamples_ = 1;    // ~5 ms at OS rate (set in prepare)
int   xoFadeInSamples_  = 1;    // = crossover refill (set in prepare)
```

### `prepareCrossoverBanks` — size the fades (where `crossoverOsLatencySamples_` is computed)
```cpp
xoFadeOutSamples_ = juce::jmax (1, (int) std::llround (0.005 * osSampleRate));   // ~5 ms
xoFadeInSamples_  = juce::jmax (xoFadeOutSamples_, crossoverOsLatencySamples_);  // mask the refill
xoDuckPhase_ = 0;
xoDuckPos_   = 0;
```

### Remove the instant swap
Delete the `trySwapCrossoverBank()` call at ~1083 (the swap now lives in the apply loop). Keep
the function but it's no longer the swap path — or inline its locked-swap body into the helper
below. Keep `xoBank` latched at ~1084 from `activeCrossoverBank_` as today.

### Apply loop (~1300-1321) — duck gain + state machine
Compute a per-sample duck gain, multiply the `bandLimitedBuf_` write by it, and advance the
phase. Equal-power (cosine) ramps:
```cpp
// helper (file-local): raised-cosine 1->0 (out) and 0->1 (in)
auto fadeOutGain = [] (int pos, int len) { return 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * (float) pos / (float) len)); };
auto fadeInGain  = [] (int pos, int len) { return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (float) pos / (float) len)); };
```
Per sample `i` in the apply loop:
```cpp
float duck = 1.0f;
if (xoDuckPhase_ == 0)            // Idle
{
    if (crossoverSwapReady_.load (std::memory_order_acquire))   // a commit is pending
    { xoDuckPhase_ = 1; xoDuckPos_ = 0; }
}
if (xoDuckPhase_ == 1)           // FadeOut
{
    duck = fadeOutGain (xoDuckPos_, xoFadeOutSamples_);
    if (++xoDuckPos_ >= xoFadeOutSamples_)
    {
        duck = 0.0f;
        // gain ~0 here: do the locked swap (23b059c discipline)
        if (tryLockCrossoverBank())
        {
            if (crossoverSwapReady_.exchange (false, std::memory_order_acq_rel))
            {
                const int nb = crossoverPendingBank_;
                detectCrossover_[nb].reset();
                applyCrossover_[nb].reset();
                activeCrossoverBank_.store (nb, std::memory_order_release);
            }
            unlockCrossoverBank();
            xoDuckPhase_ = 2; xoDuckPos_ = 0;
        }
        // if lock not acquired (msg installing), hold at 0 and retry next sample
    }
}
else if (xoDuckPhase_ == 2)      // FadeIn
{
    duck = fadeInGain (xoDuckPos_, xoFadeInSamples_);
    if (++xoDuckPos_ >= xoFadeInSamples_) { xoDuckPhase_ = 0; duck = 1.0f; }
}

// apply to this sample's multiband output:
bandLimitedBuf_.getWritePointer (ch)[i] = duck * ( /* existing expression */ );
```
Note: `xoBank` (latched at block start) is used for `applyCrossover_[xoBank]`/`detectCrossover_[xoBank]`
throughout the block; the `activeCrossoverBank_` flip above only takes audible effect on the next
block's latch. That's correct because the flip happens at `duck≈0`.

### Two channels / consistency
The state machine advances **once per sample `i`, not per channel** — advance `xoDuckPos_` /
phase on the inner-channel-loop exit (or guard it to run only for `ch == 0`), and apply the same
`duck` to all channels of sample `i`. Don't advance it nch times.

## Watch-outs
- The swap still uses the `crossoverBankLock_` try-lock + `crossoverPendingBank_` from `23b059c`
  — do NOT regress that. If the lock isn't acquired (message thread mid-install), stay at duck 0
  and retry next sample; do not spin on the audio thread.
- Only start a new duck from **Idle**. A `crossoverSwapReady_` that arrives mid-transition stays
  sticky and triggers the next duck when the current one returns to Idle (serializes commits).
- Detect crossover is reset in the same swap; its band levels refill during FadeIn. The apply
  output is ducked, so the envelope/GR settling is masked. (If a pumping artifact shows up on
  FadeIn, a follow-up can freeze the envelopes during the transition — out of scope now.)
- Latency unchanged (duck is gain only). Lookahead commits are unaffected (no kernel/refill) —
  leave them as the gesture/timer commit; this slice is crossover-only.
- `xoFadeInSamples_` = full refill is the safe value. It can be shortened later if the ~37 ms
  dip feels too long (trades a little cleanliness) — tune by ear, don't shorten below ~½ refill.

## Gate
- [ ] **Live Release DAW:** step/drag atten (and cutoff/transition) under playback — **no
      click on commit**; instead a brief smooth dip/morph (~40 ms) as it settles. State host.
- [ ] Crossover still doesn't crash on drag (`23b059c`); rapid commits (fast repeated releases)
      stay clean and don't wedge (state returns to Idle).
- [ ] Host latency unchanged; Color reconstruction still ±0.0000 dB at static settings;
      `mdsp_dsp_tests` exit 0; Release build clean (AU/VST3/Standalone).
- [ ] Steady-state CPU unchanged vs `9ae248a` (single bank; duck is a multiply).

## Report back
- Confirm commits are click-free (brief dip instead) for atten/cutoff/transition, host + how
  exercised, and the dip feels acceptable (or report if ~37 ms is too long → we tune `xoFadeInSamples_`).
- Commit message: `feat: duck-and-swap crossover commit (dip instead of click)`.
- Do NOT push (Quell hold).
