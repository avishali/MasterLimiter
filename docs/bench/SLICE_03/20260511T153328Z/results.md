# dsp_bench results

- Timestamp: 2026-05-11T15:32:59.018762+00:00
- Plugin: /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

| signal @ GR | metric | value | pass |
|---|---|---:|---|
| pink_noise_60s (3.0 dB GR) | null_residual_kweighted_db | -31.2866 | yes |
| pink_noise_60s (3.0 dB GR) | noise_residual_pct | 9.6128 | yes |
| pink_noise_60s (3.0 dB GR) | true_peak_overs | 426 | ❌ no |
| pink_noise_60s (3.0 dB GR) | sample_peak_overs | 20 | yes |
| pink_noise_60s (3.0 dB GR) | aliasing_residual_db | -2.69851 | yes |
| sine_sweep_log_30s (3.0 dB GR) | null_residual_kweighted_db | -53.442 | ❌ no |
| sine_sweep_log_30s (3.0 dB GR) | true_peak_overs | 27102 | ❌ no |
| sine_sweep_log_30s (3.0 dB GR) | sample_peak_overs | 18 | yes |
| sine_sweep_log_30s (3.0 dB GR) | aliasing_residual_db | -7.52658 | yes |
| imd_smpte (3.0 dB GR) | null_residual_kweighted_db | -50.9927 | ❌ no |
| imd_smpte (3.0 dB GR) | imd_smpte_pct | 0.0811126 | yes |
| imd_smpte (3.0 dB GR) | true_peak_overs | 11200 | ❌ no |
| imd_smpte (3.0 dB GR) | sample_peak_overs | 0 | yes |
| imd_smpte (3.0 dB GR) | aliasing_residual_db | -7.47268 | yes |
| dirac_impulse (3.0 dB GR) | null_residual_kweighted_db | -inf | yes |
| dirac_impulse (3.0 dB GR) | true_peak_overs | 2 | ❌ no |
| dirac_impulse (3.0 dB GR) | sample_peak_overs | 0 | yes |
| dirac_impulse (3.0 dB GR) | aliasing_residual_db | -13.5269 | yes |
| transient_train (3.0 dB GR) | null_residual_kweighted_db | -inf | yes |
| transient_train (3.0 dB GR) | true_peak_overs | 200 | ❌ no |
| transient_train (3.0 dB GR) | sample_peak_overs | 0 | yes |
| transient_train (3.0 dB GR) | aliasing_residual_db | -12.5649 | yes |
| _global | latency_reported_samples | 240 | yes |
| _global | latency_alignment_lag_samples | 0 | yes |

**Overall:** PASS
