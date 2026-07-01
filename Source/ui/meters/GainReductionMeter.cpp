#include "GainReductionMeter.h"

#include "PluginProcessor.h"
#include "ui/meters/ClipBallistics.h"

#include <mdsp_ui/meters/MeterBallistics.h>
#include <mdsp_ui/meters/PeakHoldModel.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kMaxGrDb = 10.0f;

static constexpr const char* kBandLabels[GainReductionMeter::kNumBands] = { "LO", "MID", "HI" };

mdsp_ui::meters::MeterBallisticsConfig makeGrBallisticsConfig() noexcept
{
    mdsp_ui::meters::MeterBallisticsConfig config;
    config.attackMs = 1.0f;
    config.releaseMs = 50.0f;
    config.holdMs = 1000.0f;
    config.holdFalloffDbPerSec = 12.0f;
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

GainReductionMeter::GainReductionMeter (mdsp_ui::UiContext& ui, MasterLimiterAudioProcessor& processor)
    : ui_ (ui),
      processor_ (processor)
{
    for (int b = 0; b < kNumBands; ++b)
    {
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            ball_[(size_t) b][(size_t) ch] = std::make_unique<mdsp_ui::meters::MeterBallistics>();
            peakHold_[(size_t) b][(size_t) ch] = std::make_unique<mdsp_ui::meters::PeakHoldModel>();
        }
    }

    setOpaque (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

GainReductionMeter::~GainReductionMeter() = default;

void GainReductionMeter::sync (float dtSec)
{
    const float raw[kNumBands][kNumChannels] = {
        { processor_.getCurrentGrLowLDb(), processor_.getCurrentGrLowRDb() },
        { processor_.getCurrentGrMidLDb(), processor_.getCurrentGrMidRDb() },
        { processor_.getCurrentGrHighLDb(), processor_.getCurrentGrHighRDb() }
    };
    const auto config = makeGrBallisticsConfig();

    for (int b = 0; b < kNumBands; ++b)
    {
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            float v = std::isfinite (raw[b][ch]) ? std::abs (raw[b][ch]) : 0.0f;
            displayGrBandChanDb_[b][ch] = ball_[(size_t) b][(size_t) ch]->process (juce::jmax (0.0f, v), dtSec, config);
            peakHold_[(size_t) b][(size_t) ch]->process (displayGrBandChanDb_[b][ch], dtSec, config);
        }
    }

    const float rawCur = processor_.getCurrentGrDb();
    const float rawMax = processor_.getMaxGrSinceResetDb();
    displayCurrentGrDb_ = std::isfinite (rawCur) ? std::abs (rawCur) : 0.0f;
    displayMaxGrDb_ = std::isfinite (rawMax) ? std::abs (rawMax) : 0.0f;

    repaint();
}

void GainReductionMeter::resetPeakHolds() noexcept
{
    for (int b = 0; b < kNumBands; ++b)
    {
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            displayGrBandChanDb_[b][ch] = 0.0f;
            ball_[(size_t) b][(size_t) ch]->reset (0.0f);
            peakHold_[(size_t) b][(size_t) ch]->reset (0.0f);
        }
    }

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
    const int bandGap = 2;
    const int totalBandGaps = bandGap * (kNumBands - 1);
    const int bandW = juce::jmax (1, (barArea.getWidth() - totalBandGaps) / kNumBands);

    auto drawSubBar = [&] (juce::Rectangle<int> bar, float grDb, float peakGrDb, bool reserved)
    {
        const auto slot = bar.toFloat();
        g.setColour (theme.panel.withAlpha (0.82f));
        g.fillRoundedRectangle (slot, m.rSmall);

        if (! reserved)
        {
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
                g.drawLine (peakSlot.getX() + 1.0f, y, peakSlot.getRight() - 1.0f, y, m.strokeThin);
            }
        }
        else
        {
            g.setColour (theme.textMuted.withAlpha (0.45f));
            g.setFont (type.labelFont().withHeight (10.0f));
            g.drawText ("-", slot, juce::Justification::centred, false);
        }

        g.setColour (theme.grid.withAlpha (0.72f));
        g.drawRoundedRectangle (slot.reduced (0.5f), m.rSmall, m.strokeThin);
    };

    for (int b = 0; b < kNumBands; ++b)
    {
        if (b > 0)
            barArea.removeFromLeft (bandGap);
        auto band = barArea.removeFromLeft (bandW);

        g.setColour (theme.textMuted.withAlpha (0.85f));
        g.setFont (type.labelFont().withHeight (8.0f));
        g.drawText (kBandLabels[b], band.removeFromTop (9), juce::Justification::centred, false);

        const bool reserved = (b == 1);
        const int subGap = 1;
        const int subW = juce::jmax (1, (band.getWidth() - subGap) / kNumChannels);
        auto leftSub = band.removeFromLeft (subW);
        band.removeFromLeft (subGap);
        auto rightSub = band;

        drawSubBar (leftSub, displayGrBandChanDb_[b][0], peakHold_[(size_t) b][0]->getHeldDb(), reserved);
        drawSubBar (rightSub, displayGrBandChanDb_[b][1], peakHold_[(size_t) b][1]->getHeldDb(), reserved);
    }

    const auto readoutText = formatDbBare (displayCurrentGrDb_) + " / " + formatDbBare (displayMaxGrDb_);

    g.setColour (theme.textMuted.withAlpha (0.85f));
    g.setFont (type.labelFont().withHeight (11.0f));
    g.drawText (readoutText, readoutArea, juce::Justification::centred, true);

    g.setColour (theme.grid.withAlpha (0.82f));
    g.drawRoundedRectangle (readoutArea.toFloat().reduced (0.5f), m.rSmall, m.strokeThin);
}
