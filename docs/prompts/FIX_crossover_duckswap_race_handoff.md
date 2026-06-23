# Cursor handoff — FIX: duck-and-swap reopened the crossover install/process race (DAW crash)

**Repo:** `MasterLimiter` plugin only. On top of `9262889`. SDK unchanged.

## Crash (confirmed from the .ips — do not re-investigate, just fix)

`EXC_BAD_ACCESS` / SIGSEGV at NULL on the **AudioCalc** (audio) thread, top frame **`libvDSP`
FFT** called from `processCore` → the crossover `PartitionedConvolver`. Happened while
stepping the DEV atten slider (rapid gesture-commits) on the `9262889` build.

**Root cause — a regression from `9262889`.** The crossover's crash-safety relies on one
invariant (from `23b059c`):
> audio processes bank `active`; the message thread installs ONLY into bank `1 - active`.
> They never touch the same bank.

`9262889` moved the swap INTO the per-sample apply loop, flipping `activeCrossoverBank_` to
`nb` **mid-block**. But `xoBank` is latched at the top of the block, so the rest of that block
keeps running `applyCrossover_[xoBank]` on the **old** bank. Window:
1. Mid-block swap sets `active = nb`; audio keeps doing vDSP FFTs on the **old** bank for the
   rest of the block.
2. Message thread sees `active == nb`, so its next gesture-commit `rebuildCrossoverKernels`
   targets `1 - nb = old` and `installKernel` **frees/reallocs the old bank's vDSP FFT setup**…
3. …while audio is mid-FFT on that old bank → **NULL deref**.

The lock (`crossoverBankLock_`) doesn't help: it serializes install-vs-swap, but `processSample`
is lock-free and was reading a bank that no longer matched `active`.

## Fix: move the swap back to the block top (block-aligned), keep the per-sample duck

Within any block, the bank being processed MUST equal `activeCrossoverBank_`. So do the actual
bank swap once per block, at the top (before `xoBank` is latched), gated on the duck having
already reached the fade-out floor. The per-sample loop only ramps the duck gain — it must NOT
flip `activeCrossoverBank_`.

This also removes the buffer-size-dependent "cold-bank step" (the fade-in now runs entirely on
the new bank).

### `processCore` — block top (where the swap used to live, ~line 1077, before `xoBank` latch)
```cpp
// Block-aligned crossover swap: only once the duck has faded to the floor.
if (xoDuckPhase_ == 1 /*FadeOut*/ && xoDuckPos_ >= xoFadeOutSamples_)
{
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
        xoDuckPhase_ = 2 /*FadeIn*/;
        xoDuckPos_   = 0;
    }
    // lock not acquired (msg installing) → stay FadeOut, duck held at 0, retry next block
}

const int xoBank = activeCrossoverBank_.load (std::memory_order_acquire);   // latched AFTER any swap
```

### Apply loop (~1295-1360) — per-sample duck ONLY; remove the in-loop swap
The state machine in the apply loop must no longer reset banks or flip `activeCrossoverBank_`.
Keep only the gain ramp + phase advance:
```cpp
float duck = 1.0f;
if (xoDuckPhase_ == 0)            // Idle
{
    if (crossoverSwapReady_.load (std::memory_order_acquire))
    { xoDuckPhase_ = 1; xoDuckPos_ = 0; }
}
if (xoDuckPhase_ == 1)           // FadeOut (ramp to floor; the SWAP is done at block top)
{
    if (xoDuckPos_ < xoFadeOutSamples_) { duck = xoFadeOutGain (xoDuckPos_, xoFadeOutSamples_); ++xoDuckPos_; }
    else                                  duck = 0.0f;   // hold at floor until block-top swap flips us to FadeIn
}
else if (xoDuckPhase_ == 2)      // FadeIn (now on the NEW bank — block-top already swapped)
{
    duck = xoFadeInGain (xoDuckPos_, xoFadeInSamples_);
    if (++xoDuckPos_ >= xoFadeInSamples_) { xoDuckPhase_ = 0; duck = 1.0f; }
}
// apply `duck` to bandLimitedBuf_ as in 9262889
```

### Net effect / timing
- Fade-out ramps down (may span ≥1 block at small buffers); once it hits the floor it **holds
  at 0**. At the **next block top**, the swap fires (under the lock), `active` flips, `xoBank`
  latches to the new bank, phase → FadeIn. Adds at most ~1 block of held-floor silence — fine.
- Within every block `xoBank == active`, so the message thread (targeting `1 - active`) can
  never install the bank the audio thread is processing. Invariant restored → no race.

## Watch-outs
- The per-sample loop must NOT call `reset()` / `installActiveKernel` / store `activeCrossoverBank_`.
  All bank mutation happens at the block top, under `crossoverBankLock_` try-lock (audio never spins).
- Keep `crossoverPendingBank_` / `crossoverSwapReady_` semantics from `23b059c`. A commit arriving
  during FadeOut/FadeIn just updates pending + swapReady and is picked up at the next eligible
  block top (serialized; no overlap).
- `detectCrossover_` and `applyCrossover_` swap together as a pair (same `nb`), and both are now
  used consistently for the whole block (detect loop + apply loop both read the post-swap `xoBank`).
- Latency unchanged; lookahead path untouched.

## Gate (the real one is live-DAW)
- [ ] **Live Release DAW:** rapidly step AND drag atten, cutoff, transition under playback for
      60–90 s — **no crash**, brief dip/morph on commit (no click). This is the gate; the offline
      rig cannot reproduce the message-thread timing that caused the SIGSEGV.
- [ ] Rapid repeated gesture-commits (fast slider releases) stay clean and return to Idle; no
      wedge, no stuck mute.
- [ ] Host latency unchanged; Color recon ±0.0000 at static settings; `mdsp_dsp_tests` exit 0;
      Release build clean (AU/VST3/Standalone).

## Report back
- Confirm the SIGSEGV is gone under aggressive atten stepping/dragging in a live DAW (host +
  duration), and commits are still click-free (dip).
- Commit message: `fix: block-align crossover duck-swap (mid-block flip raced the convolver FFT)`.
- Do NOT push (Quell hold).
