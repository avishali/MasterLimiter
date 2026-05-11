#pragma once

#include <string_view>

namespace param
{

inline constexpr std::string_view input_gain_db { "input_gain_db" };
inline constexpr std::string_view ceiling_db { "ceiling_db" };
inline constexpr std::string_view release_ms { "release_ms" };
inline constexpr std::string_view release_auto { "release_auto" };
inline constexpr std::string_view lookahead_ms { "lookahead_ms" };
inline constexpr std::string_view ceiling_mode { "ceiling_mode" };
inline constexpr std::string_view stereo_link_pct { "stereo_link_pct" };
inline constexpr std::string_view ms_link_pct { "ms_link_pct" };
inline constexpr std::string_view character { "character" };

} // namespace param
