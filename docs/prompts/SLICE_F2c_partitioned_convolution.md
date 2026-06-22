# Cursor Build Prompt — Slice F-2c: Uniform-Partitioned FFT Convolution (make the crossover RT-viable)

**Context:** `docs/CUSTOM_FILTERS.md`; F-2b shipped the linear-phase crossover but it pegs CPU at 100% in
real time. Root cause (measured): at the 4× OS rate (192 kHz) the worst-case kernel is **~14,264 taps**
→ **~5.5 GMAC/s** of scalar direct convolution (detect+apply × 2 ch, symmetry-folded). Direct conv is
O(N) per sample; **uniform-partitioned FFT convolution is ~O(log N) amortized → ~100× faster**, which
takes the crossover from 100% CPU to single digits. The DSP is correct (offline rig proved it); this slice
is purely the convolution *engine*, swapped in under the existing crossover so behaviour is unchanged.

SDK work in `melechdsp-hq/shared/mdsp_dsp/` (sibling repo, source of truth). `mdsp_dsp` already links
`juce::juce_dsp`, so **use `juce::dsp::FFT`** (don't hand-roll an FFT).

> Close F-2c by pasting raw `git status`, the test output, and the latency numbers. The CPU win is
> validated in a DAW by avishali — your gate is correctness + latency + build.

---

## 1. New class `PartitionedConvolver` (`include/mdsp_dsp/filters/PartitionedConvolver.h` + `src/filters/PartitionedConvolver.cpp`)

Uniform-partitioned (block) convolution, **overlap-save**, with a frequency-domain delay line (FDL).
Per-channel. Computes the exact same linear convolution as `LinearPhaseFIR` (same taps), just cheaply.

Algorithm (B = block size, power of two; FFT size = 2B):
- **prepare(taps, blockSize, numChannels):**
  - Partition the length-N kernel into `P = ceil(N / B)` segments of B taps (zero-pad the last).
  - For each segment p: zero-pad to 2B, forward-FFT → store `H[p]` (frequency domain). Precomputed once.
  - Allocate per channel: an input accumulator (B), an overlap buffer (B carried from previous block),
    an FDL of P complex spectra (2B each), an output queue, scratch FFT buffers. **All allocation here.**
- **processSample(ch, x) -> float (RT-safe, no allocation):**
  - Push x into the channel's input accumulator and pop one sample from its output queue (return it).
  - When B input samples have accumulated, run one block:
    - Build the 2B time buffer `[overlap (prev B) | current B]`; forward-FFT → `X`.
    - Shift the FDL and insert `X` at slot 0.
    - `Y = Σ_p H[p] · FDL[p]` (complex multiply-accumulate over the P partitions).
    - Inverse-FFT `Y`; the **last B samples** are the output block (overlap-save) → push to output queue.
    - Save the current B input as the next `overlap`.
- **reset():** zero accumulators, overlap, FDL, output queue (keep `H[p]`).
- **getLatencySamples() const:** the exact input→output delay. For overlap-save uniform partitioning
  this is the kernel's own group delay PLUS the block latency B. Return the true total so callers can
  align. (Derive it precisely and assert it in the test against a measured impulse delay.)

Use `juce::dsp::FFT` (order = log2(2B)). Keep `float` audio I/O; complex math in `float` is fine here
(K-weight is the only place that needed double). No locks, no allocation in `processSample`.

Pick **B = 128** (OS-rate samples ≈ 0.67 ms host) as the default — negligible vs the ~37 ms kernel.

## 2. Wire into `LinearPhaseCrossover`

Replace the `LinearPhaseFIR` lowpass member with `PartitionedConvolver` (one per channel), OR add a
`bool usePartitioned` and keep both. Critical coupling — get this exactly right:

- The reference delay (`refDelay_`) MUST equal the convolver's **actual** latency, not `(N-1)/2`:
  ```cpp
  refDelay_.setDelaySamples (lowpass_.getLatencySamples());   // NOT (taps.size()-1)/2
  ```
  Then `high = delay(x, L) - low` with `L = getLatencySamples()` keeps `low + high == delay(x, L)`
  EXACTLY regardless of block latency. The existing F-2a complementary-sum + flat-reconstruction tests
  must still pass unchanged — re-run them.
- `prepareFixedLatency` / `installActiveKernel` / `centerPadKernel` still apply: center-pad to Nmax so
  latency is fixed across DEV specs. (Partitioning of a fixed-Nmax kernel keeps a fixed partition count.)

## 3. Plugin — should be near-zero change

The crossover's `processSample(ch, x, low, high, xDelayed)` API is unchanged, so
`PluginProcessor.cpp` (detect loop :1150, apply loop :1189) needs no edit. BUT:
- `crossoverOsLatencySamples_ = detectCrossover_[0].getLatencySamples()` (:504) now includes block
  latency B — confirm the PDC math (:312/:323) and `dryDelay_`/`lookaheadPad_` still add up, and that
  the band-lookahead alignment from F-2b still holds (detect and apply share the SAME convolver latency,
  so the M-cancellation argument is unaffected — re-verify with `tools/analysis/transient_lookahead.py`).

## 4. Gate (paste raw output)

1. **Correctness:** new test `tests/Test_PartitionedConvolver.cpp` — partitioned output matches a direct
   `LinearPhaseFIR` convolution of the same taps to **< 1e-5** on a broadband signal (align by the
   reported latency). Impulse response delay == `getLatencySamples()`.
2. **Crossover invariants:** F-2a `Test_LinearPhaseCrossover` still green (complementary sum, flat
   reconstruction at every blend) with the partitioned lowpass.
3. Full `mdsp_dsp_tests` exit 0; MasterLimiter Standalone/AU/VST3 build clean.
4. Report `getLatencySamples()` (OS + host) for the worst-case Nmax so we confirm PDC.
5. **CPU** is validated by avishali in a DAW (target: single-digit % at Color 50, OS 4×). Note the
   expected partition count `P = ceil(Nmax / B)` so we have a complexity estimate.

## Out of scope
Multirate low-band split (separate latency-reduction slice), oversampler port (F-3), removing DEV params.
Don't change the kernel design or the crossover topology — only the convolution engine underneath.
