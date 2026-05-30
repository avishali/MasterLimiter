#include "GainReductionMeter.h"

#include "PluginProcessor.h"

#include <mdsp_ui/meters/MeterBallistics.h>
#include <mdsp_ui/meters/PeakHoldModel.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kMaxGrDb = 10.0f;

mdsp_ui::meters::MeterBallisticsConfig makeGrBallisticsConfig() noexcept
{
    mdsp_ui::meters::MeterBallisticsConfig config;
    config.attackMs = 1.0f;
    config.releaseMs = 300.0f;
    config.holdMs = 1000.0f;
    config.holdFalloffDbPerSec = 12.0f;
    return config;
}

mdsp_ui::meters::MeterBallisticsConfig makeClipReadoutBallisticsConfig() noexcept
{
    auto config = makeGrBallisticsConfig();
    config.clipHoldMs = 1000.0f;
    return config;
}

mdsp_ui::meters::MeterBallisticsConfig makeClipLedBallisticsConfig() noexcept
{
    auto config = makeClipReadoutBallisticsConfig();
    config.holdFalloffDbPerSec = 1.2f;
    return config;
}

float normaliseGr (float grDb) noexcept
{
    return juce::jlimit (0.0f, 1.0f, grDb / kMaxGrDb);
}

juce::Rectangle<float> makeTopDownFill (juce::Rectangle<float> bar, float grDb)
{
    const float fillH = normaliseGr (grDb) * bar.getHeight();
    return bar.withHeight (fillH);
}
} // namespace

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

namespace master_limiter_ui
{

struct ClipBallisticsState
{
    mdsp_ui::meters::MeterBallistics readout;
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
    state.readout.reset (0.0f);
    state.ledHold.reset (0.0f);
}

void processClipReadout (ClipBallisticsState& state, float clipDb, float dtSec) noexcept
{
    state.readout.process (juce::jmax (0.0f, clipDb), dtSec, makeClipReadoutBallisticsConfig());
}

float processClipLed (ClipBallisticsState& state, bool clipped, float dtSec) noexcept
{
    return state.ledHold.process (clipped ? 1.0f : 0.0f, dtSec, makeClipLedBallisticsConfig());
}

float getClipReadoutCurrent (const ClipBallisticsState& state) noexcept
{
    return state.readout.getCurrent();
}

} // namespace master_limiter_ui

GainReductionMeter::GainReductionMeter (mdsp_ui::UiContext& ui, MasterLimiterAudioProcessor& processor)
    : ui_ (ui),
      processor_ (processor),
      ballL_ (std::make_unique<mdsp_ui::meters::MeterBallistics>()),
      ballR_ (std::make_unique<mdsp_ui::meters::MeterBallistics>()),
      peakHoldL_ (std::make_unique<mdsp_ui::meters::PeakHoldModel>()),
      peakHoldR_ (std::make_unique<mdsp_ui::meters::PeakHoldModel>())
{
    setOpaque (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

GainReductionMeter::~GainReductionMeter() = default;

void GainReductionMeter::sync (float dtSec)
{
    float rawL = processor_.getCurrentGrLDb();
    float rawR = processor_.getCurrentGrRDb();
    float rawMax = processor_.getMaxGrSinceResetDb();

    if (! std::isfinite (rawL))
        rawL = 0.0f;
    if (! std::isfinite (rawR))
        rawR = 0.0f;
    if (! std::isfinite (rawMax))
        rawMax = 0.0f;

    const auto config = makeGrBallisticsConfig();
    displayGrLDb_ = ballL_->process (juce::jmax (0.0f, std::abs (rawL)), dtSec, config);
    displayGrRDb_ = ballR_->process (juce::jmax (0.0f, std::abs (rawR)), dtSec, config);
    peakHoldL_->process (displayGrLDb_, dtSec, config);
    peakHoldR_->process (displayGrRDb_, dtSec, config);
    displayCurrentGrDb_ = std::max (displayGrLDb_, displayGrRDb_);
    displayMaxGrDb_ = juce::jmax (0.0f, std::abs (rawMax));

    repaint();
}

void GainReductionMeter::resetPeakHolds() noexcept
{
    displayGrLDb_ = 0.0f;
    displayGrRDb_ = 0.0f;
    displayCurrentGrDb_ = 0.0f;
    displayMaxGrDb_ = 0.0f;
    ballL_->reset (0.0f);
    ballR_->reset (0.0f);
    peakHoldL_->reset (0.0f);
    peakHoldR_->reset (0.0f);
    processor_.resetMaxGr();
}

juce::String GainReductionMeter::formatDbBare (float grDb)
{
    if (! std::isfinite (grDb))
        grDb = 0.0f;

    return juce::String (juce::jmax (0.0f, grDb), 1);
}

void GainReductionMeter::resized()
{
    auto bounds = getLocalBounds();
    readoutBounds_ = bounds.removeFromBottom (18);
    meterBounds_ = bounds.reduced (4, 4);
}

void GainReductionMeter::mouseDown (const juce::MouseEvent& e)
{
    if (readoutBounds_.contains (e.getPosition()))
    {
        resetPeakHolds();
        repaint();
    }
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    const auto& m = ui_.metrics();

    auto fallbackBounds = getLocalBounds();
    const auto fallbackReadout = fallbackBounds.removeFromBottom (18);
    const auto fallbackMeter = fallbackBounds.reduced (4, 4);
    const auto meterArea = meterBounds_.isEmpty() ? fallbackMeter : meterBounds_;
    const auto readoutArea = readoutBounds_.isEmpty() ? fallbackReadout : readoutBounds_;

    g.setColour (theme.background.darker (0.18f));
    g.fillRoundedRectangle (meterArea.toFloat(), m.rMed);
    g.setColour (theme.grid.withAlpha (0.82f));
    g.drawRoundedRectangle (meterArea.toFloat().reduced (0.5f), m.rMed, m.strokeThin);

    auto barArea = meterArea.reduced (7, 7);
    const int gap = 3;
    const int halfW = juce::jmax (1, (barArea.getWidth() - gap) / 2);
    auto leftBar = barArea.removeFromLeft (halfW);
    barArea.removeFromLeft (gap);
    auto rightBar = barArea;

    auto drawBar = [&] (juce::Rectangle<int> bar, float grDb, float peakGrDb)
    {
        const auto slot = bar.toFloat();
        g.setColour (theme.panel.withAlpha (0.82f));
        g.fillRoundedRectangle (slot, m.rSmall);

        const auto fill = makeTopDownFill (slot.reduced (1.0f), grDb);
        if (fill.getHeight() > 0.5f)
        {
            g.setColour (theme.warning.withAlpha (0.84f));
            g.fillRoundedRectangle (fill, m.rSmall);
            g.setColour (theme.warning.brighter (0.22f).withAlpha (0.8f));
            g.drawLine (fill.getX(), fill.getBottom(), fill.getRight(), fill.getBottom(), m.strokeThin);
        }

        const auto peakSlot = slot.reduced (1.0f);
        if (peakGrDb > 0.0f && peakSlot.getHeight() > 1.0f)
        {
            const float y = peakSlot.getY() + normaliseGr (peakGrDb) * peakSlot.getHeight();
            g.setColour (theme.warning.brighter (0.35f).withAlpha (0.78f));
            g.drawLine (peakSlot.getX() + 2.0f, y, peakSlot.getRight() - 2.0f, y, m.strokeThin);
        }

        g.setColour (theme.grid.withAlpha (0.72f));
        g.drawRoundedRectangle (slot.reduced (0.5f), m.rSmall, m.strokeThin);
    };

    drawBar (leftBar, displayGrLDb_, peakHoldL_->getHeldDb());
    drawBar (rightBar, displayGrRDb_, peakHoldR_->getHeldDb());

    const auto readoutText = formatDbBare (displayCurrentGrDb_) + " / " + formatDbBare (displayMaxGrDb_);

    g.setColour (theme.textMuted.withAlpha (0.85f));
    g.setFont (type.labelFont().withHeight (11.0f));
    g.drawText (readoutText, readoutArea, juce::Justification::centred, true);

    g.setColour (theme.grid.withAlpha (0.82f));
    g.drawRoundedRectangle (readoutArea.toFloat().reduced (0.5f), m.rSmall, m.strokeThin);
}
