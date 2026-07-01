#include "GainReductionMeter.h"

#include "PluginProcessor.h"
#include "ui/meters/ClipBallistics.h"

#include <mdsp_ui/meters/MeterBallistics.h>
#include <mdsp_ui/meters/PeakHoldModel.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kMaxGrDb = 12.0f;
constexpr float kGrScaleMarksDb[] = { 0.5f, 1.0f, 2.0f, 3.0f, 6.0f, 12.0f };
constexpr int kGrScaleGutterW = 24;
constexpr float kGrReadoutHoldSec = 0.85f;
constexpr float kGrReadoutReleaseTauSec = 0.95f;

static constexpr const char* kBandLabels[GainReductionMeter::kNumBands] = { "LO", "MID", "HI" };

mdsp_ui::meters::MeterBallisticsConfig makeGrBallisticsConfig() noexcept
{
    mdsp_ui::meters::MeterBallisticsConfig config;
    config.attackMs = 1.0f;
    config.releaseMs = 180.0f;
    config.holdMs = 1000.0f;
    config.holdFalloffDbPerSec = 12.0f;
    return config;
}

float normaliseGr (float grDb) noexcept
{
    // sqrt low-end expansion: dedicate the bottom half of the bar to 0..3 dB
    // while keeping full scale at kMaxGrDb (12 dB). 3 dB -> 50%, 6 dB -> 71%.
    const float x = juce::jlimit (0.0f, 1.0f, grDb / kMaxGrDb);
    return std::sqrt (x);
}

float grDbToY (float grDb, float plotTop, float plotBottom) noexcept
{
    const float plotH = plotBottom - plotTop;
    return plotTop + normaliseGr (grDb) * plotH;
}

juce::Rectangle<float> makeTopDownFill (juce::Rectangle<float> bar, float grDb)
{
    const float fillH = normaliseGr (grDb) * bar.getHeight();
    if (fillH <= 0.0f)
        return {};

    return { bar.getX(), bar.getY(), bar.getWidth(), fillH };
}

void tickGrReadoutSmoother (float& held,
                            float& duty,
                            int& holdTicksLeft,
                            float raw,
                            float dtSec,
                            int holdTicksAt30Hz) noexcept
{
    if (! std::isfinite (raw))
        raw = 0.0f;

    raw = juce::jmax (0.0f, raw);

    if (raw > held)
    {
        held = raw;
        duty = raw;
        holdTicksLeft = holdTicksAt30Hz;
        return;
    }

    if (holdTicksLeft > 0)
    {
        --holdTicksLeft;
        duty = held;
        return;
    }

    const float a = 1.0f - std::exp (-dtSec / kGrReadoutReleaseTauSec);
    held += (raw - held) * a;
    duty = held;
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
    const float curGr = std::isfinite (rawCur) ? std::abs (rawCur) : 0.0f;
    const int holdTicks = juce::jmax (1, static_cast<int> (std::round (kGrReadoutHoldSec / juce::jmax (1.0e-6f, dtSec))));
    tickGrReadoutSmoother (readoutHeldGrDb_, readoutCurrentGrDb_, readoutHoldTicksLeft_, curGr, dtSec, holdTicks);
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

    displayMaxGrDb_ = 0.0f;
    readoutHeldGrDb_ = 0.0f;
    readoutCurrentGrDb_ = 0.0f;
    readoutHoldTicksLeft_ = 0;
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

    auto plotOuter = meterBounds_.reduced (7, 7);
    scaleGutterArea_ = plotOuter.removeFromRight (kGrScaleGutterW);
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

    g.setColour (theme.panel.withAlpha (0.9f));
    g.fillRoundedRectangle (meterArea.toFloat(), m.rSmall);
    g.setColour (theme.background.withAlpha (0.65f));
    g.drawRoundedRectangle (meterArea.toFloat(), m.rSmall, m.strokeThin);

    auto plotOuter = meterArea.reduced (7, 7);
    scaleGutterArea_ = plotOuter.removeFromRight (kGrScaleGutterW);
    auto barArea = plotOuter;
    const int bandGap = 4;
    const int totalBandGaps = bandGap * (kNumBands - 1);
    const int bandW = juce::jmax (1, (barArea.getWidth() - totalBandGaps) / kNumBands);
    juce::Rectangle<int> grPlotArea;

    auto drawSubBar = [&] (juce::Rectangle<int> bar, float grDb, float peakGrDb, bool reserved)
    {
        const auto slot = bar.toFloat();
        g.setColour (theme.panel.withAlpha (0.82f));
        g.fillRoundedRectangle (slot, m.rSmall);

        if (! reserved)
        {
            const auto inner = slot.reduced (1.0f);
            const auto fill = makeTopDownFill (inner, grDb);
            if (fill.getHeight() > 0.0f)
            {
                g.setColour (theme.warning.withAlpha (0.84f));
                g.fillRect (fill);
                g.setColour (theme.warning.brighter (0.22f).withAlpha (0.8f));
                g.drawLine (fill.getX(), fill.getBottom(), fill.getRight(), fill.getBottom(), m.strokeThin);
            }

            const auto peakSlot = inner;
            if (peakGrDb > 0.0f && peakSlot.getHeight() > 1.0f)
            {
                const float y = grDbToY (peakGrDb, peakSlot.getY(), peakSlot.getBottom());
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
        g.setFont (type.labelFont().withHeight (9.0f));
        g.drawText (kBandLabels[b], band.removeFromTop (10), juce::Justification::centred, false);

        const bool reserved = (b == 1);
        const int subGap = 2;
        const int subW = juce::jmax (1, (band.getWidth() - subGap) / kNumChannels);
        auto leftSub = band.removeFromLeft (subW);
        const float dividerX = (float) band.getX();
        band.removeFromLeft (subGap);
        auto rightSub = band;

        drawSubBar (leftSub, displayGrBandChanDb_[b][0], peakHold_[(size_t) b][0]->getHeldDb(), reserved);
        drawSubBar (rightSub, displayGrBandChanDb_[b][1], peakHold_[(size_t) b][1]->getHeldDb(), reserved);

        if (! reserved)
            grPlotArea = grPlotArea.isEmpty() ? leftSub.getUnion (rightSub)
                                              : grPlotArea.getUnion (leftSub.getUnion (rightSub));

        if (! reserved && band.getHeight() > 12)
        {
            g.setColour (theme.grid.withAlpha (0.55f));
            g.drawVerticalLine ((int) std::round (dividerX), (float) leftSub.getY() + 2.0f, (float) leftSub.getBottom() - 2.0f);
        }
    }

    if (! grPlotArea.isEmpty() && ! scaleGutterArea_.isEmpty())
    {
        const float plotTop = (float) grPlotArea.getY();
        const float plotBottom = (float) grPlotArea.getBottom();
        const float plotRight = (float) grPlotArea.getRight();
        const auto scaleBounds = scaleGutterArea_.toFloat();
        const float scaleLeft = scaleBounds.getX();
        const float scaleRight = scaleBounds.getRight();

        g.setFont (type.labelFont().withHeight (8.0f));

        for (const float markDb : kGrScaleMarksDb)
        {
            const float y = grDbToY (markDb, plotTop, plotBottom);

            g.setColour (theme.grid.withAlpha (0.32f));
            g.drawLine ((float) grPlotArea.getX() + 1.0f, y, scaleRight, y, m.strokeThin);

            g.setColour (theme.grid.withAlpha (0.62f));
            g.drawLine (plotRight + 1.0f, y, scaleLeft + 4.0f, y, m.strokeMed);

            g.setColour (theme.textMuted.withAlpha (0.78f));
            g.drawText (juce::String (-markDb, markDb < 1.0f ? 1 : 0),
                        juce::Rectangle<float> (scaleLeft + 2.0f, y - 5.0f, scaleBounds.getWidth() - 4.0f, 10.0f),
                        juce::Justification::centredLeft,
                        false);
        }

        g.setColour (theme.textMuted.withAlpha (0.45f));
        g.drawText ("dB", juce::Rectangle<float> (scaleLeft, plotTop - 11.0f, scaleBounds.getWidth(), 10.0f),
                    juce::Justification::centredLeft, false);
    }

    const auto readoutText = formatDbBare (readoutCurrentGrDb_) + " / " + formatDbBare (displayMaxGrDb_);

    auto readoutContent = readoutArea;
    const auto totalCaption = readoutContent.removeFromLeft (34);

    g.setColour (theme.textMuted.withAlpha (0.55f));
    g.setFont (type.labelFont().withHeight (8.0f));
    g.drawText ("total", totalCaption, juce::Justification::centredLeft, true);

    g.setColour (theme.textMuted.withAlpha (0.85f));
    g.setFont (type.labelFont().withHeight (11.0f));
    g.drawText (readoutText, readoutContent, juce::Justification::centred, true);

    g.setColour (theme.grid.withAlpha (0.82f));
    g.drawRoundedRectangle (readoutArea.toFloat().reduced (0.5f), m.rSmall, m.strokeThin);
}
