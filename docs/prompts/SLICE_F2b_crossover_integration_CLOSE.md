# SLICE CLOSE — F-2b: Wire LinearPhaseCrossover into plugin (DEV-auditionable)

## Shipped

### SDK (`melechdsp-hq`)
- `LinearPhaseCrossover::prepareFixedLatency()` — worst-case Nmax/Mmax for stable PDC
- `installActiveKernel()` — center-pads shorter kernels into Nmax
- `centerPadKernel()` static helper

### Plugin (`MasterLimiter`)
- Replaced `juce::dsp::LinkwitzRileyFilter` with `mdsp_dsp::LinearPhaseCrossover` (detect + apply, double-buffered)
- Phase-coherent Color blend: linked term uses `xDelayed` from `processSample`
- Detection alignment: band lookahead reduced by crossover OS latency
- Fixed host PDC: `crossoverOsLatencyHostSamples_` added to `baseLatencySamples_`
- DEV params: `dev_xover_cutoff_hz`, `dev_xover_transition_hz`, `dev_xover_atten_db`
- DEV UI: CROSSOVER section in docked panel
- Docs: `SIGNAL_FLOW.md` §2.9/§2.12, §4.1, §6, §8.2

## Gate checklist
- [x] `mdsp_dsp_tests` full suite exit 0
- [x] MasterLimiter Standalone/AU/VST3 build clean
- [x] Offline Color sweep — low-band delta vs Color 0: ±0.000 dB at 0/25/50/75/100 (pedalboard AU rig)
- [ ] CPU % at Color 50, OS 4× — profile in DAW (escalate to F-2c if RT budget exceeded)

## Follow-ups
- Bake auditioned xover spec + remove DEV xover params before 0.4
- F-2c FFT-partition crossover if CPU demands at 192 kHz OS
