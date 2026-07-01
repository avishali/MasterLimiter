#include "MeterGroupComponent.h"

#include <cmath>

namespace
{
constexpr float kMeterFloorDb = MasterLimiterAudioProcessor::kMeterFloorDb;

juce::String channelLabel (int channelCount, int index)
{
    if (channelCount <= 1)
        return "MONO";

    return (index == 0) ? "L" : "R";
}

bool isBelowMeterFloor (float v) noexcept
{
    return ! std::isfinite (v) || v <= kMeterFloorDb;
}

juce::String formatDbBare (float v)
{
    if (isBelowMeterFloor (v))
        return "-inf";

    return juce::String (v, 1);
}

juce::String formatPeakReadout (float currentDb)
{
    return formatDbBare (currentDb);
}

juce::String formatMaxPeakReadout (float maxDb)
{
    return formatDbBare (maxDb);
}

juce::String formatRmsReadout (float rmsDb)
{
    return formatDbBare (rmsDb);
}

juce::String formatTruePeakReadout (float tpDb)
{
    if (isBelowMeterFloor (tpDb))
        return "-inf";

    return (tpDb > 0.0f ? juce::String ("+") : juce::String()) + juce::String (tpDb, 1);
}
} // namespace

void MeterGroupComponent::GrNumericSmoother::reset() noexcept
{
    v = 0.0f;
}

void MeterGroupComponent::GrNumericSmoother::tick (float raw, float dtSec) noexcept
{
    if (! std::isfinite (raw))
        raw = 0.0f;

    raw = juce::jmax (0.0f, raw);
    const float tau = 0.1f;
    const float a = 1.0f - std::exp (-dtSec / tau);
    v += (raw - v) * a;
}

juce::String MeterGroupComponent::GrNumericSmoother::formatDb (float g) noexcept
{
    if (! std::isfinite (g))
        return "0.0 dB";

    return juce::String (g, 1) + " dB";
}

void MeterGroupComponent::DisplayLevelSmoother::reset (float peak, float rms) noexcept
{
    peakDb = std::isfinite (peak) ? peak : kMeterFloorDb;
    rmsDb = std::isfinite (rms) ? rms : kMeterFloorDb;
}

void MeterGroupComponent::DisplayLevelSmoother::tick (float peak, float rms, float dtSec) noexcept
{
    auto smooth = [] (float raw, float current, float dt) noexcept
    {
        if (! std::isfinite (raw))
            raw = kMeterFloorDb;
        if (! std::isfinite (current) || current <= kMeterFloorDb + 0.5f)
            return raw;
        if (raw >= current)
            return raw;

        constexpr float releaseDbPerSec = 60.0f;
        return juce::jmax (raw, current - releaseDbPerSec * juce::jmax (0.0f, dt));
    };

    peakDb = smooth (peak, peakDb, dtSec);
    rmsDb = smooth (rms, rmsDb, dtSec);
}

MeterGroupComponent::MeterGroupComponent (mdsp_ui::UiContext& ui,
                                          MasterLimiterAudioProcessor& processor,
                                          BusKind kind)
    : ui_ (ui),
      processor_ (processor),
      kind_ (kind)
{
    if (kind_ == BusKind::GainReduction)
    {
        meter0_ = std::make_unique<MeterComponent> (ui_, channelLabel (channelCount_, 0));
        meter1_ = std::make_unique<MeterComponent> (ui_, channelLabel (channelCount_, 1));
        meter0_->setKind (MeterComponent::Kind::GainReduction);
        meter1_->setKind (MeterComponent::Kind::GainReduction);
        meter0_->setGrSingleBarMode (true);
        meter1_->setGrSingleBarMode (true);
        meter0_->setRangeDb (0.0f, 20.0f);
        meter1_->setRangeDb (0.0f, 20.0f);
        meter0_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);
        meter1_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);
        addAndMakeVisible (*meter0_);
        addAndMakeVisible (*meter1_);
    }
    else
    {
        meter0_ = std::make_unique<MeterComponent> (ui_, channelLabel (channelCount_, 0));
        meter1_ = std::make_unique<MeterComponent> (ui_, channelLabel (channelCount_, 1));
        meter0_->setKind (MeterComponent::Kind::Level);
        meter1_->setKind (MeterComponent::Kind::Level);
        meter0_->setDrawInternalScale (false);
        meter1_->setDrawInternalScale (false);
        meter0_->setShowRms (showRms_);
        meter1_->setShowRms (showRms_);
        meter0_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);
        meter1_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);
        meter0_->setClipResetCallback (&MeterGroupComponent::peakResetThunk, this);
        meter1_->setClipResetCallback (&MeterGroupComponent::peakResetThunk, this);
        meter0_->setExternalReadoutCaptions (true);
        meter1_->setExternalReadoutCaptions (true);
        meter0_->setReadoutAlignTowardCenter (true);
        meter1_->setReadoutAlignTowardCenter (false);
        addAndMakeVisible (*meter0_);
        addAndMakeVisible (*meter1_);

        provider0_.setScaleMode (scaleMode_);
        provider1_.setScaleMode (scaleMode_);
        provider0_.setDisplayMode (DisplayMode::Rms);
        provider1_.setDisplayMode (DisplayMode::Rms);
        // holdEnabled=true latches the bar max marker until Reset Peaks; the provider's
        // 1500 ms / 12 dB·s decay branch only runs when hold is disabled.
        provider0_.setHoldEnabled (true);
        provider1_.setHoldEnabled (true);
        pushLevelRenderStates (kMeterFloorDb, kMeterFloorDb);
    }
}

void MeterGroupComponent::peakResetThunk (void* ctx) noexcept
{
    if (ctx != nullptr)
        static_cast<MeterGroupComponent*> (ctx)->handlePeakReset();
}

void MeterGroupComponent::handlePeakReset() noexcept
{
    if (kind_ == BusKind::GainReduction)
    {
        grSmooth_.reset();
        return;
    }

    const float lPeak = (kind_ == BusKind::Input) ? processor_.getInputPeakLDb() : processor_.getOutputPeakLDb();
    const float rPeak = (kind_ == BusKind::Input) ? processor_.getInputPeakRDb() : processor_.getOutputPeakRDb();
    const float lRms = (kind_ == BusKind::Input) ? processor_.getInputRmsLDb() : processor_.getOutputRmsLDb();
    const float rRms = (kind_ == BusKind::Input) ? processor_.getInputRmsRDb() : processor_.getOutputRmsRDb();
    provider0_.updateFromValues (lPeak, lRms, false, false);
    provider1_.updateFromValues (rPeak, rRms, false, false);
    provider0_.resetPeakHold();
    provider1_.resetPeakHold();
    if (kind_ == BusKind::Input)
        processor_.resetInputTruePeakHolds();
    if (kind_ == BusKind::Output)
        processor_.resetOutputTruePeakHolds();
    displaySmooth0_.reset (lPeak, lRms);
    displaySmooth1_.reset (rPeak, rRms);
    pushLevelRenderStates (kMeterFloorDb, kMeterFloorDb);
}

int MeterGroupComponent::getPreferredWidth() const noexcept
{
    const auto& m = ui_.metrics();
    const int meterW = m.meterGroupMeterW;
    const int gap = (kind_ == BusKind::Input || kind_ == BusKind::Output) ? kReadoutLabelGutterW
                                                                          : m.meterGroupGap;
    return (channelCount_ <= 1) ? meterW : (meterW * 2 + gap);
}

void MeterGroupComponent::resetPeakHolds() noexcept
{
    handlePeakReset();
}

void MeterGroupComponent::setScaleMode (ScaleMode mode) noexcept
{
    scaleMode_ = mode;
    provider0_.setScaleMode (mode);
    provider1_.setScaleMode (mode);
    const float lMax = kind_ == BusKind::Input ? processor_.getMaxInputPeakLDb() : processor_.getMaxOutputPeakLDb();
    const float rMax = kind_ == BusKind::Input ? processor_.getMaxInputPeakRDb() : processor_.getMaxOutputPeakRDb();
    pushLevelRenderStates (lMax, rMax);
    repaint();
}

void MeterGroupComponent::setShowRms (bool shouldShow) noexcept
{
    showRms_ = shouldShow;
    if (meter0_ != nullptr)
        meter0_->setShowRms (showRms_);
    if (meter1_ != nullptr)
        meter1_->setShowRms (showRms_);
    repaint();
}

juce::Rectangle<int> MeterGroupComponent::getScaleReferenceBoundsInParent() const noexcept
{
    if (meter0_ == nullptr)
        return {};

    return meter0_->getMeterArea()
        .translated (meter0_->getX(), meter0_->getY())
        .translated (getX(), getY());
}

void MeterGroupComponent::pushLevelRenderStates (float maxPeakLDb, float maxPeakRDb)
{
    provider0_.fillRenderState (renderState0_);
    provider1_.fillRenderState (renderState1_);
    renderState0_.maxPeakDb = maxPeakLDb;
    renderState1_.maxPeakDb = maxPeakRDb;

    if (meter0_ != nullptr)
        meter0_->setRenderState (renderState0_);
    if (meter1_ != nullptr)
        meter1_->setRenderState (renderState1_);
}

void MeterGroupComponent::sync (double hostSampleRate, float dtSec)
{
    juce::ignoreUnused (hostSampleRate);

    if (kind_ == BusKind::GainReduction)
    {
        const float rawGr = processor_.getCurrentGrDb();
        const float gr = (std::isfinite (rawGr) ? std::abs (rawGr) : 0.0f);
        grSmooth_.tick (gr, dtSec);
        const auto line = GrNumericSmoother::formatDb (grSmooth_.v);
        const juce::String blank;

        if (meter0_ != nullptr)
        {
            meter0_->setGrDb (gr);
            meter0_->setNumericReadoutOverride (true, line, blank);
        }
        if (meter1_ != nullptr)
        {
            meter1_->setGrDb (gr);
            meter1_->setNumericReadoutOverride (true, line, blank);
        }

        return;
    }

    const float lPeak = (kind_ == BusKind::Input) ? processor_.getInputPeakLDb() : processor_.getOutputPeakLDb();
    const float rPeak = (kind_ == BusKind::Input) ? processor_.getInputPeakRDb() : processor_.getOutputPeakRDb();
    const float lRms = (kind_ == BusKind::Input) ? processor_.getInputRmsLDb() : processor_.getOutputRmsLDb();
    const float rRms = (kind_ == BusKind::Input) ? processor_.getInputRmsRDb() : processor_.getOutputRmsRDb();
    const float lMaxDb = kind_ == BusKind::Input ? processor_.getMaxInputPeakLDb() : processor_.getMaxOutputPeakLDb();
    const float rMaxDb = kind_ == BusKind::Input ? processor_.getMaxInputPeakRDb() : processor_.getMaxOutputPeakRDb();

    displaySmooth0_.tick (lPeak, lRms, dtSec);
    displaySmooth1_.tick (rPeak, rRms, dtSec);

    constexpr float kClipThresholdDb = 0.0f;
    const bool clippedL = renderState0_.clipLatched || (std::isfinite (lPeak) && lPeak >= kClipThresholdDb);
    const bool clippedR = renderState1_.clipLatched || (std::isfinite (rPeak) && rPeak >= kClipThresholdDb);

    provider0_.updateFromValues (displaySmooth0_.peakDb, displaySmooth0_.rmsDb, clippedL, false);
    provider1_.updateFromValues (displaySmooth1_.peakDb, displaySmooth1_.rmsDb, clippedR, false);
    pushLevelRenderStates (lMaxDb, rMaxDb);

    const auto lPeakText = formatPeakReadout (displaySmooth0_.peakDb);
    const auto rPeakText = formatPeakReadout (displaySmooth1_.peakDb);
    const auto lMaxText = formatMaxPeakReadout (lMaxDb);
    const auto rMaxText = formatMaxPeakReadout (rMaxDb);
    const auto lRmsText = formatRmsReadout (lRms);
    const auto rRmsText = formatRmsReadout (rRms);
    const bool showTruePeak = kind_ == BusKind::Input || kind_ == BusKind::Output;
    const float lTruePeak = kind_ == BusKind::Input ? processor_.getMaxInputTruePeakLDb()
                           : kind_ == BusKind::Output ? processor_.getMaxOutputTruePeakLDb()
                           : kMeterFloorDb;
    const float rTruePeak = kind_ == BusKind::Input ? processor_.getMaxInputTruePeakRDb()
                           : kind_ == BusKind::Output ? processor_.getMaxOutputTruePeakRDb()
                           : kMeterFloorDb;

    tpReadoutOver_ = (lTruePeak > 0.0f) || (rTruePeak > 0.0f);

    if (meter0_ != nullptr)
    {
        meter0_->setTruePeakReadout (showTruePeak, formatTruePeakReadout (lTruePeak), lTruePeak > 0.0f);
        meter0_->setNumericReadoutOverride (true, lPeakText, lMaxText, lRmsText);
    }
    if (meter1_ != nullptr)
    {
        meter1_->setTruePeakReadout (showTruePeak, formatTruePeakReadout (rTruePeak), rTruePeak > 0.0f);
        meter1_->setNumericReadoutOverride (true, rPeakText, rMaxText, rRmsText);
    }

    if (! readoutLabelArea_.isEmpty())
        repaint (readoutLabelArea_);
}

void MeterGroupComponent::paintReadoutCaptions (juce::Graphics& g)
{
    if (readoutLabelArea_.isEmpty())
        return;

    const auto& theme = ui_.theme();
    const auto captionMuted = theme.textMuted.withAlpha (0.62f);
    const auto tpCaptionColour = tpReadoutOver_ ? theme.danger.withAlpha (0.95f) : captionMuted;

    auto bounds = readoutLabelArea_.reduced (0, 3);
    juce::Rectangle<int> tpBounds;
    tpBounds = bounds.removeFromTop (13);

    const int rowH = bounds.getHeight() / 3;
    auto spBounds = bounds.removeFromTop (rowH);
    auto maxBounds = bounds.removeFromTop (rowH);
    auto rmsBounds = bounds;

    g.setFont (juce::Font (juce::FontOptions().withHeight (8.0f)).boldened());

    auto drawCaption = [&] (juce::Rectangle<int> area, const char* text, juce::Colour colour)
    {
        g.setColour (colour);
        g.drawText (text, area, juce::Justification::centred);
    };

    drawCaption (tpBounds, "TP", tpCaptionColour);
    drawCaption (spBounds, "SP", captionMuted);
    drawCaption (maxBounds, "MAX", theme.warning.withAlpha (0.72f));
    drawCaption (rmsBounds, "RMS", captionMuted);
}

void MeterGroupComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();

    if (kind_ != BusKind::Input && kind_ != BusKind::Output)
        return;

    const auto arrowBounds = labelArea_.withSizeKeepingCentre (22, 10).toFloat();
    const float midX = arrowBounds.getCentreX();
    const float top = arrowBounds.getY() + 1.0f;
    const float bottom = arrowBounds.getBottom() - 1.0f;
    const float left = arrowBounds.getX() + 3.0f;
    const float right = arrowBounds.getRight() - 3.0f;

    juce::Path arrow;
    if (kind_ == BusKind::Input)
    {
        arrow.startNewSubPath (left, top);
        arrow.lineTo (right, top);
        arrow.lineTo (midX, bottom);
    }
    else
    {
        arrow.startNewSubPath (left, bottom);
        arrow.lineTo (right, bottom);
        arrow.lineTo (midX, top);
    }
    arrow.closeSubPath();

    g.setColour (theme.textMuted.withAlpha (0.55f));
    g.fillPath (arrow);

    const float lineY = kind_ == BusKind::Input ? top + 1.0f : bottom - 1.0f;
    const float labelLeft = static_cast<float> (labelArea_.getX());
    const float labelRight = static_cast<float> (labelArea_.getRight());
    g.setColour (theme.textMuted.withAlpha (0.36f));
    g.drawLine (labelLeft + 9.0f, lineY, left - 6.0f, lineY, 1.0f);
    g.drawLine (right + 6.0f, lineY, labelRight - 9.0f, lineY, 1.0f);
}

void MeterGroupComponent::paintOverChildren (juce::Graphics& g)
{
    if (kind_ == BusKind::Input || kind_ == BusKind::Output)
        paintReadoutCaptions (g);
}

void MeterGroupComponent::resized()
{
    const auto& m = ui_.metrics();
    auto b = getLocalBounds();

    const int labelHeight = 16;
    headerArea_ = b.removeFromTop (labelHeight);
    labelArea_ = headerArea_;
    metersArea_ = b.reduced (static_cast<int> (m.strokeThick), static_cast<int> (m.strokeThick));

    if (kind_ == BusKind::GainReduction)
    {
        const int meterW = juce::jmax (36, (metersArea_.getWidth() - m.meterGroupGap) / 2);
        auto row = metersArea_;
        const int totalW = meterW * 2 + m.meterGroupGap;
        row = row.withSizeKeepingCentre (totalW, row.getHeight());

        auto left = row.removeFromLeft (meterW);
        row.removeFromLeft (m.meterGroupGap);
        auto right = row.removeFromLeft (meterW);

        if (meter0_ != nullptr)
            meter0_->setBounds (left);
        if (meter1_ != nullptr)
            meter1_->setBounds (right);
        return;
    }

    layoutLevelMeters();
}

void MeterGroupComponent::layoutLevelMeters()
{
    const auto& m = ui_.metrics();
    const int meterW = m.meterGroupMeterW;
    const int gap = kReadoutLabelGutterW;

    readoutLabelArea_ = {};

    if (channelCount_ <= 1)
    {
        if (meter0_ != nullptr)
            meter0_->setBounds (metersArea_.withSizeKeepingCentre (meterW, metersArea_.getHeight()));
        if (meter1_ != nullptr)
            meter1_->setVisible (false);
        return;
    }

    if (meter1_ != nullptr)
        meter1_->setVisible (true);

    auto row = metersArea_;
    const int totalW = meterW * 2 + gap;
    row = row.withSizeKeepingCentre (totalW, row.getHeight());

    auto left = row.removeFromLeft (meterW);
    auto gutter = row.removeFromLeft (gap);
    auto right = row.removeFromLeft (meterW);

    if (meter0_ != nullptr)
        meter0_->setBounds (left);
    if (meter1_ != nullptr)
        meter1_->setBounds (right);

    if (meter0_ != nullptr && meter1_ != nullptr)
    {
        const auto lNum = meter0_->getNumericArea().translated (meter0_->getX(), meter0_->getY());
        readoutLabelArea_ = gutter.withY (lNum.getY()).withHeight (lNum.getHeight());
    }
}
