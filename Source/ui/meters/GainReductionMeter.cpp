#include "GainReductionMeter.h"

#include "PluginProcessor.h"

#include <cmath>

namespace
{
constexpr float kMaxGrDb = 10.0f;

float normaliseGr (float grDb) noexcept
{
    return juce::jlimit (0.0f, 1.0f, grDb / kMaxGrDb);
}
} // namespace

GainReductionMeter::GainReductionMeter (mdsp_ui::UiContext& ui, MasterLimiterAudioProcessor& processor)
    : ui_ (ui),
      processor_ (processor)
{
    setOpaque (false);
}

void GainReductionMeter::sync (float dtSec)
{
    float raw = processor_.getCurrentGrDb();
    if (! std::isfinite (raw))
        raw = 0.0f;

    grDb_ = juce::jmax (0.0f, std::abs (raw));

    const float tau = 0.1f;
    const float a = 1.0f - std::exp (-dtSec / tau);
    displayGrDb_ += (grDb_ - displayGrDb_) * a;

    repaint();
}

void GainReductionMeter::resetPeakHolds() noexcept
{
    grDb_ = 0.0f;
    displayGrDb_ = 0.0f;
}

juce::String GainReductionMeter::formatDb (float grDb)
{
    if (! std::isfinite (grDb))
        return "0.0 dB";

    return juce::String (juce::jmax (0.0f, grDb), 1) + " dB";
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    const auto& m = ui_.metrics();

    auto bounds = getLocalBounds();

    auto labelArea = bounds.removeFromTop (18);
    auto valueArea = bounds.removeFromBottom (24);
    auto meterArea = bounds.reduced (4, 4);

    g.setColour (theme.textMuted.withAlpha (0.75f));
    g.setFont (type.labelFont().withHeight (11.0f).boldened());
    g.drawText ("GR", labelArea, juce::Justification::centred, true);

    g.setColour (theme.textMuted.withAlpha (0.85f));
    g.setFont (type.labelFont().withHeight (11.0f));
    g.drawText (formatDb (displayGrDb_), valueArea, juce::Justification::centred, true);

    const float x = static_cast<float> (meterArea.getX());
    const float y = static_cast<float> (meterArea.getY());
    const float w = static_cast<float> (meterArea.getWidth());
    const float h = static_cast<float> (meterArea.getHeight());

    g.setColour (theme.background.darker (0.18f));
    g.fillRoundedRectangle (meterArea.toFloat(), m.rMed);
    g.setColour (theme.grid.withAlpha (0.82f));
    g.drawRoundedRectangle (meterArea.toFloat().reduced (0.5f), m.rMed, m.strokeThin);

    g.setFont (type.labelFont().withHeight (9.0f));
    for (const auto tick : { 0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f })
    {
        const float yy = y + normaliseGr (tick) * h;
        g.setColour (theme.text.withAlpha (tick == 0.0f ? 0.42f : 0.27f));
        g.drawHorizontalLine (juce::roundToInt (yy), x, x + w);

        const auto label = tick == 0.0f ? juce::String ("0") : "-" + juce::String (tick, 0);
        g.setColour (theme.textMuted.withAlpha (0.48f));
        g.drawText (label,
                    juce::Rectangle<float> (x, yy - 5.0f, w, 10.0f),
                    juce::Justification::centred);
    }

    const float barW = w - 18.0f;
    const float barX = x + 9.0f;
    const float fillH = normaliseGr (displayGrDb_) * h;

    const auto slot = juce::Rectangle<float> (barX, y, barW, h).reduced (1.0f, 1.0f);
    g.setColour (theme.panel.withAlpha (0.8f));
    g.fillRoundedRectangle (slot, m.rSmall);

    if (fillH > 0.5f)
    {
        const auto fill = juce::Rectangle<float> (barX + 1.0f, y + 1.0f, barW - 2.0f, fillH);
        g.setColour (theme.warning.withAlpha (0.84f));
        g.fillRoundedRectangle (fill, m.rSmall);
        g.setColour (theme.warning.brighter (0.22f).withAlpha (0.8f));
        g.drawLine (fill.getX(), fill.getBottom(), fill.getRight(), fill.getBottom(), m.strokeThin);
    }

    g.setColour (theme.grid.withAlpha (0.75f));
    g.drawRoundedRectangle (slot, m.rSmall, m.strokeThin);
}
