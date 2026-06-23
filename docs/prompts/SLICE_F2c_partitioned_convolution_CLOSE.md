# SLICE CLOSE — F-2c: Uniform-partitioned FFT convolution (crossover RT engine)

## Shipped (SDK only — `melechdsp-hq`)

### New
- `PartitionedConvolver` — overlap-save uniform-partitioned FFT conv (`B=128`, `juce::dsp::FFT`)
- `Test_PartitionedConvolver.cpp` — matches `LinearPhaseFIR` to < 1e-5, impulse peak at `getLatencySamples()`

### Wired
- `LinearPhaseCrossover` lowpass engine: `LinearPhaseFIR` → `PartitionedConvolver`
- `refDelay_` uses convolver `getLatencySamples()` (group delay + B)
- Plugin unchanged — same `processSample(ch,x,low,high,xDelayed)` API; PDC auto-updates via `getLatencySamples()`

## Gate
- [x] `mdsp_dsp_tests "Partitioned"` — 3/3 pass
- [x] `mdsp_dsp_tests "Crossover"` — 5/5 pass (complementary sum, Color flatness, band split, latency, symmetry)
- [x] Full `mdsp_dsp_tests` exit 0
- [x] MasterLimiter Standalone/AU/VST3 build clean
- [ ] CPU @ Color 50, OS 4× — DAW profile by avishali (target: single-digit %)

## Latency (worst-case DEV spec @ 192 kHz OS)
| Quantity | Value |
|---|---|
| Nmax | 14,267 taps |
| Partitions P = ceil(Nmax/128) | **112** |
| OS latency `getLatencySamples()` | **7,261** (~37.8 ms @ 192 kHz) |
| Host PDC (÷4) | **1,816** samples (~37.8 ms @ 48 kHz) |

Block latency B=128 adds +128 OS samples (+32 host) vs F-2b direct conv — negligible vs ~7.1k group delay.

## Follow-up
- DAW CPU validation; escalate multirate low-band split only if still over budget
