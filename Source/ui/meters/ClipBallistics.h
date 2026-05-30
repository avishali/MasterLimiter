#pragma once

#include <memory>

namespace master_limiter_ui
{
struct ClipBallisticsState;
struct ClipBallisticsDeleter
{
    void operator() (ClipBallisticsState* state) const noexcept;
};

using ClipBallisticsPtr = std::unique_ptr<ClipBallisticsState, ClipBallisticsDeleter>;

ClipBallisticsPtr makeClipBallisticsState();
void resetClipBallistics (ClipBallisticsState& state) noexcept;
void processClipReadout (ClipBallisticsState& state, float clipDb, float dtSec) noexcept;
float processClipLed (ClipBallisticsState& state, bool clipped, float dtSec) noexcept;
float getClipReadoutCurrent (const ClipBallisticsState& state) noexcept;
} // namespace master_limiter_ui
