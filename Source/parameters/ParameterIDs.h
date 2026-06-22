#pragma once

#include <string_view>

namespace param
{

inline constexpr std::string_view input_gain_db { "input_gain_db" };
inline constexpr std::string_view limiter_active { "limiter_active" };
inline constexpr std::string_view plugin_bypass { "plugin_bypass" };
inline constexpr std::string_view clipper_drive_db { "clipper_drive_db" };
inline constexpr std::string_view clipper_mode { "clipper_mode" };
inline constexpr std::string_view ceiling_db { "ceiling_db" };
inline constexpr std::string_view gain_ceiling_link { "gain_ceiling_link" };
inline constexpr std::string_view gain_match_auto { "gain_match_auto" };
inline constexpr std::string_view io_input_l_db { "io_input_l_db" };
inline constexpr std::string_view io_input_r_db { "io_input_r_db" };
inline constexpr std::string_view io_input_link { "io_input_link" };
inline constexpr std::string_view io_output_l_db { "io_output_l_db" };
inline constexpr std::string_view io_output_r_db { "io_output_r_db" };
inline constexpr std::string_view io_output_link { "io_output_link" };
inline constexpr std::string_view release_ms { "release_ms" };
inline constexpr std::string_view release_sustain_ratio { "release_sustain_ratio" };
inline constexpr std::string_view release_auto { "release_auto" };
inline constexpr std::string_view auto_release_mode { "auto_release_mode" };
inline constexpr std::string_view ceiling_mode { "ceiling_mode" };
inline constexpr std::string_view stereo_mode { "stereo_mode" };
inline constexpr std::string_view stereo_link_pct { "stereo_link_pct" };
inline constexpr std::string_view ms_link_pct { "ms_link_pct" };
inline constexpr std::string_view band_color { "band_color" };
inline constexpr std::string_view character { "character" };
inline constexpr std::string_view clipper_active { "clipper_active" };
inline constexpr std::string_view dev_low_band_release_scale { "dev_low_band_release_scale" };
inline constexpr std::string_view dev_high_band_release_scale { "dev_high_band_release_scale" };
inline constexpr std::string_view dev_sigma_attack_ms { "dev_sigma_attack_ms" };
inline constexpr std::string_view dev_sigma_decay_scale { "dev_sigma_decay_scale" };
inline constexpr std::string_view dev_attack_ms { "dev_attack_ms" };
inline constexpr std::string_view dev_attack_mode { "dev_attack_mode" };
inline constexpr std::string_view dev_real_attack_ms { "dev_real_attack_ms" };
inline constexpr std::string_view dev_release_engine { "dev_release_engine" };
inline constexpr std::string_view dev_la_release_ms { "dev_la_release_ms" };
inline constexpr std::string_view dev_la_release_poles { "dev_la_release_poles" };
inline constexpr std::string_view dev_lookahead_band_ms { "dev_lookahead_band_ms" };
inline constexpr std::string_view dev_lookahead_wide_ms { "dev_lookahead_wide_ms" };
inline constexpr std::string_view dev_xover_cutoff_hz { "dev_xover_cutoff_hz" };
inline constexpr std::string_view dev_xover_transition_hz { "dev_xover_transition_hz" };
inline constexpr std::string_view dev_xover_atten_db { "dev_xover_atten_db" };

} // namespace param
