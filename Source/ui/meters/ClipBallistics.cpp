#include "ClipBallistics.h"

#include <mdsp_ui/meters/MeterTypes.h>
#include <mdsp_ui/meters/PeakHoldModel.h>

#include <algorithm>

namespace
{
mdsp_ui::meters::MeterBallisticsConfig makeClipReadoutBallisticsConfig() noexcept
{
    mdsp_ui::meters::MeterBallisticsConfig config;
    config.holdMs = 850.0f;
    config.holdFalloffDbPerSec = 12.0f;
    return config;
}

mdsp_ui::meters::MeterBallisticsConfig makeClipLedBallisticsConfig() noexcept
{
    mdsp_ui::meters::MeterBallisticsConfig config;
    config.holdMs = 80.0f;
    config.holdFalloffDbPerSec = 24.0f;
    return config;
}
} // namespace

namespace master_limiter_ui
{

struct ClipBallisticsState
{
    mdsp_ui::meters::PeakHoldModel readoutHold;
    mdsp_ui::meters::PeakHoldModel ledHold;
};

void ClipBallisticsDeleter::operator() (ClipBallisticsState* state) const noexcept
{
    delete state;
}

ClipBallisticsPtr makeClipBallisticsState()
{
    return ClipBallisticsPtr { new ClipBallisticsState };
}

void resetClipBallistics (ClipBallisticsState& state) noexcept
{
    state.readoutHold.reset (0.0f);
    state.ledHold.reset (0.0f);
}

void processClipReadout (ClipBallisticsState& state, float clipDb, float dtSec) noexcept
{
    state.readoutHold.process (std::max (0.0f, clipDb), dtSec, makeClipReadoutBallisticsConfig());
}

float processClipLed (ClipBallisticsState& state, bool clipped, float dtSec) noexcept
{
    return state.ledHold.process (clipped ? 1.0f : 0.0f, dtSec, makeClipLedBallisticsConfig());
}

float getClipReadoutCurrent (const ClipBallisticsState& state) noexcept
{
    return state.readoutHold.getHeldDb();
}

} // namespace master_limiter_ui
