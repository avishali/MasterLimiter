# dsp_bench results

- Timestamp: 2026-05-11T13:50:06.558858+00:00
- Plugin: /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-debug/MasterLimiter_artefacts/Debug/VST3/MasterLimiter.vst3

| signal @ GR | metric | value | pass |
|---|---|---:|---|
| pink_noise_60s (0.0 dB GR) | null_residual_kweighted_db | -inf | yes |
| pink_noise_60s (0.0 dB GR) | noise_residual_pct | 0 | yes |
| pink_noise_60s (0.0 dB GR) | true_peak_overs | 0 | yes |
| pink_noise_60s (0.0 dB GR) | aliasing_residual_db | 0 | yes |
| sine_sweep_log_30s (0.0 dB GR) | null_residual_kweighted_db | -inf | yes |
| sine_sweep_log_30s (0.0 dB GR) | true_peak_overs | 0 | yes |
| sine_sweep_log_30s (0.0 dB GR) | aliasing_residual_db | 0 | yes |
| imd_smpte (0.0 dB GR) | null_residual_kweighted_db | -inf | yes |
| imd_smpte (0.0 dB GR) | imd_smpte_pct | 0 | yes |
| imd_smpte (0.0 dB GR) | true_peak_overs | 0 | yes |
| imd_smpte (0.0 dB GR) | aliasing_residual_db | 0 | yes |
| dirac_impulse (0.0 dB GR) | null_residual_kweighted_db | -inf | yes |
| dirac_impulse (0.0 dB GR) | true_peak_overs | 4 | ❌ no |
| dirac_impulse (0.0 dB GR) | aliasing_residual_db | 0 | yes |
| transient_train (0.0 dB GR) | null_residual_kweighted_db | -inf | yes |
| transient_train (0.0 dB GR) | true_peak_overs | 360 | ❌ no |
| transient_train (0.0 dB GR) | aliasing_residual_db | 0 | yes |
| _global | latency_samples_measured | 0 | yes |
| _global | latency_samples_reported | 0 | yes |

**Overall:** PASS
