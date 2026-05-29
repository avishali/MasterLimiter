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
inline constexpr std::string_view lookahead_ms { "lookahead_ms" };
inline constexpr std::string_view ceiling_mode { "ceiling_mode" };
inline constexpr std::string_view stereo_link_pct { "stereo_link_pct" };
inline constexpr std::string_view ms_link_pct { "ms_link_pct" };
inline constexpr std::string_view character { "character" };

} // namespace param
