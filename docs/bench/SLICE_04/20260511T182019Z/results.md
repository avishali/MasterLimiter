# dsp_bench results

- Timestamp: 2026-05-11T18:18:16.795574+00:00
- Plugin: /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

| signal @ GR | metric | value | pass |
|---|---|---:|---|
| pink_noise_60s (3.0 dB GR) | null_residual_kweighted_db | -18.7991 | yes |
| pink_noise_60s (3.0 dB GR) | noise_residual_pct | 22.7833 | yes |
| pink_noise_60s (3.0 dB GR) | true_peak_overs | 166 | yes |
| pink_noise_60s (3.0 dB GR) | sample_peak_overs | 50 | yes |
| pink_noise_60s (3.0 dB GR) | aliasing_residual_db | -3.62484 | yes |
| sine_sweep_log_30s (3.0 dB GR) | null_residual_kweighted_db | -2.33186 | ❌ no |
| sine_sweep_log_30s (3.0 dB GR) | true_peak_overs | 144 | yes |
| sine_sweep_log_30s (3.0 dB GR) | sample_peak_overs | 0 | yes |
| sine_sweep_log_30s (3.0 dB GR) | aliasing_residual_db | -7.54408 | yes |
| imd_smpte (3.0 dB GR) | null_residual_kweighted_db | -27.0519 | ❌ no |
| imd_smpte (3.0 dB GR) | imd_smpte_pct | 0.338922 | yes |
| imd_smpte (3.0 dB GR) | true_peak_overs | 0 | yes |
| imd_smpte (3.0 dB GR) | sample_peak_overs | 0 | yes |
| imd_smpte (3.0 dB GR) | aliasing_residual_db | -2.02552 | yes |
| dirac_impulse (3.0 dB GR) | null_residual_kweighted_db | -39.4752 | ❌ no |
| dirac_impulse (3.0 dB GR) | true_peak_overs | 0 | yes |
| dirac_impulse (3.0 dB GR) | sample_peak_overs | 0 | yes |
| dirac_impulse (3.0 dB GR) | aliasing_residual_db | -13.8485 | yes |
| transient_train (3.0 dB GR) | null_residual_kweighted_db | -55.3818 | ❌ no |
| transient_train (3.0 dB GR) | true_peak_overs | 0 | yes |
| transient_train (3.0 dB GR) | sample_peak_overs | 0 | yes |
| transient_train (3.0 dB GR) | aliasing_residual_db | -13.4459 | yes |
| _global | latency_reported_samples | 301 | yes |
| _global | latency_alignment_lag_samples | 0 | yes |

**Overall:** PASS
