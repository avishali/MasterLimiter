# Cursor handoff — FIX: crossover bank-swap data race (DAW crash on cutoff drag)

**Repo:** `MasterLimiter` (plugin only — the SDK `LinearPhaseCrossover` class is correct,
do NOT change it). Bug is independent of F-3d; do this on top of `fff7147`.

---

## Symptom

Dragging the DEV crossover cutoff (`dev_xover_cutoff_hz`, default 120 Hz) **crashes the
DAW**. A single discrete value change is fine; the crash needs a *drag* (rapid stream of
param changes).

## Root cause (confirmed by code read — do not re-investigate, just fix)

The linear-phase crossover uses a 2-bank double buffer. The message thread redesigns the
kernel into the "inactive" bank; the audio thread swaps banks. But:

1. `LinearPhaseCrossover::installActiveKernel()` → `installKernel()` **reallocates**
   everything the audio thread reads: `lowpass_.clear()/resize()`, `lp.setTaps()` (FFT
   partition buffers), `refDelay_.prepare()`, `lowPad_.prepare()`.
2. The staging-bank choice in `rebuildCrossoverKernels()` is a **racy snapshot**:
   ```cpp
   const int staging = 1 - activeCrossoverBank_.load(acquire);   // TOCTOU
   detectCrossover_[staging].installActiveKernel(active);        // realloc
   ```
   `activeCrossoverBank_` is flipped **asynchronously by the audio thread** in
   `trySwapCrossoverBank()`. Under a drag, the message thread can read a stale `active`,
   pick `staging` = the bank the audio thread **just switched onto**, and reallocate its
   convolver/delay buffers **while `processSample()` is reading them** → use-after-free →
   crash.

`reset()` (audio thread, in `trySwapCrossoverBank`) is allocation-free — leave it as is.

## Files

- `Source/PluginProcessor.cpp`: `prepareCrossoverBanks` (~495), `rebuildCrossoverKernels`
  (~531), `trySwapCrossoverBank` (~541). `processCore` reads the active bank at ~983-984.
- `Source/PluginProcessor.h`: the crossover atomics at ~177-179.

---

## The fix — explicit bank ownership (message thread never touches the audio thread's bank)

Replace the racy `staging = 1 - active` with a **free-bank handoff**: the message thread
writes ONLY to a bank the audio thread has explicitly published as free.

### `PluginProcessor.h` — adjust the crossover atomics
```cpp
std::atomic<int>  activeCrossoverBank_  { 0 };   // audio thread owns (writes on swap)
std::atomic<int>  crossoverFreeBank_    { 1 };   // bank the msg thread may write; audio publishes it
std::atomic<bool> crossoverSwapReady_   { false };
std::atomic<bool> crossoverRedesignPending_ { false };
int               crossoverPendingBank_ = 1;     // msg→audio: which bank holds the new kernel (plain int, written by msg before swapReady release, read by audio after swapReady acquire — the release/acquire on crossoverSwapReady_ orders it)
```

### `prepareCrossoverBanks()` — initialise ownership
After installing both banks with the initial spec:
```cpp
activeCrossoverBank_.store (0, std::memory_order_release);
crossoverFreeBank_.store   (1, std::memory_order_release);
crossoverPendingBank_ = 1;
crossoverSwapReady_.store      (false, std::memory_order_release);
crossoverRedesignPending_.store(false, std::memory_order_release);
```

### `rebuildCrossoverKernels()` (message thread) — write only the free bank
```cpp
void MasterLimiterAudioProcessor::rebuildCrossoverKernels()
{
    const int wb = crossoverFreeBank_.load (std::memory_order_acquire);   // the ONLY bank we may touch
    const auto active = readCrossoverSpecFromParams();

    detectCrossover_[wb].installActiveKernel (active);   // safe: audio is not reading wb
    applyCrossover_[wb].installActiveKernel (active);

    crossoverPendingBank_ = wb;                          // tell audio which bank to switch to
    crossoverSwapReady_.store (true, std::memory_order_release);
}
```
Note on coalescing: if several param changes arrive before the audio thread swaps,
`crossoverFreeBank_` stays the same (audio hasn't swapped), so each rebuild just rewrites
the same free bank and re-arms `swapReady`. Correct and safe — no special handling needed.

### `trySwapCrossoverBank()` (audio thread) — swap, then hand the vacated bank back
```cpp
void MasterLimiterAudioProcessor::trySwapCrossoverBank() noexcept
{
    if (! crossoverSwapReady_.exchange (false, std::memory_order_acq_rel))
        return;

    const int newBank = crossoverPendingBank_;                 // ordered by swapReady acquire
    const int oldBank = activeCrossoverBank_.load (std::memory_order_relaxed);

    detectCrossover_[newBank].reset();                         // RT-safe (clears state only)
    applyCrossover_[newBank].reset();

    activeCrossoverBank_.store (newBank, std::memory_order_release);   // audio now reads newBank
    crossoverFreeBank_.store   (oldBank, std::memory_order_release);   // hand oldBank to msg AFTER we stopped reading it
}
```

### Invariant that kills the race
The message thread writes only `crossoverFreeBank_`. The audio thread sets
`crossoverFreeBank_ = oldBank` **only after** `activeCrossoverBank_ = newBank` — i.e. after
it has stopped reading `oldBank`. So the message thread can never reallocate the bank the
audio thread is currently processing. `newBank` only becomes active again via a future swap
that the message thread arms *after* writing it.

---

## Watch-outs
- Do NOT change `LinearPhaseCrossover` (SDK). Plugin-only fix.
- Keep latency identical — `installActiveKernel` preserves fixed latency (center-padded to
  `maxNumTaps_`); the swap must not call `setLatencySamples`. Verify reported host latency
  is unchanged before/after a cutoff drag.
- `crossoverPendingBank_` is a plain `int`, intentionally — its visibility is provided by
  the release/acquire pair on `crossoverSwapReady_` (write it before the release store; read
  it after the acquire exchange). Don't add a lock.
- The same protocol covers BOTH `detectCrossover_[]` and `applyCrossover_[]` (they swap as a
  pair under one bank index) — keep them in lockstep.

## Gate
- [ ] **Repro is gone:** drag `dev_xover_cutoff_hz` rapidly across its full range (40↔ high)
      for ~30 s in a DAW under playback — no crash. Do this in the **Release** build (the
      crashing config).
- [ ] If available, build a Debug/ASAN (or TSAN) Standalone and run the same drag — no
      heap-use-after-free / data-race report on the crossover banks.
- [ ] No audible glitch beyond the expected kernel-swap transient; no zipper/dropout when
      the knob is held still.
- [ ] Host latency unchanged across a cutoff drag (report the number on/off-drag).
- [ ] Color reconstruction still ±0.0000 dB at a few static cutoffs (regression guard on
      the offline rig); full `mdsp_dsp_tests` exit 0 (sanity — no SDK change expected);
      Release build clean (Standalone/AU/VST3).

## Report back
- Confirm the drag no longer crashes (Release) + ASAN/TSAN result if you ran it.
- Confirm latency unchanged and Color recon still flat.
- Commit message: `fix: crossover bank-swap data race (DAW crash on cutoff drag)`.
- Do NOT push (push is on hold pending Quell — plugin + SDK go up together later).
