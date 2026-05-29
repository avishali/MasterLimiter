#include "GainReductionMeter.h"

#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kMaxGrDb = 10.0f;

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

GainReductionMeter::GainReductionMeter (mdsp_ui::UiContext& ui, MasterLimiterAudioProcessor& processor)
    : ui_ (ui),
      processor_ (processor)
{
    setOpaque (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

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

    const float tau = 0.1f;
    const float a = 1.0f - std::exp (-dtSec / tau);
    displayGrLDb_ += (juce::jmax (0.0f, std::abs (rawL)) - displayGrLDb_) * a;
    displayGrRDb_ += (juce::jmax (0.0f, std::abs (rawR)) - displayGrRDb_) * a;
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
        processor_.resetMaxGr();
        displayMaxGrDb_ = 0.0f;
        repaint (readoutBounds_);
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

    auto drawBar = [&] (juce::Rectangle<int> bar, float grDb)
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

        g.setColour (theme.grid.withAlpha (0.72f));
        g.drawRoundedRectangle (slot.reduced (0.5f), m.rSmall, m.strokeThin);
    };

    drawBar (leftBar, displayGrLDb_);
    drawBar (rightBar, displayGrRDb_);

    const auto readoutText = formatDbBare (displayCurrentGrDb_) + " / " + formatDbBare (displayMaxGrDb_);

    g.setColour (theme.textMuted.withAlpha (0.85f));
    g.setFont (type.labelFont().withHeight (11.0f));
    g.drawText (readoutText, readoutArea, juce::Justification::centred, true);

    g.setColour (theme.grid.withAlpha (0.82f));
    g.drawRoundedRectangle (readoutArea.toFloat().reduced (0.5f), m.rSmall, m.strokeThin);
}
