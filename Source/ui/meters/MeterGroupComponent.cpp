#include "MeterGroupComponent.h"

#include <cmath>

namespace
{
juce::String titleFor (MeterGroupComponent::BusKind k)
{
    switch (k)
    {
        case MeterGroupComponent::BusKind::Output: return "OUT";
        case MeterGroupComponent::BusKind::Input: return "IN";
        case MeterGroupComponent::BusKind::GainReduction: return "GR";
    }
    return {};
}

juce::String channelLabel (int channelCount, int index)
{
    if (channelCount <= 1)
        return "MONO";

    return (index == 0) ? "L" : "R";
}

juce::String formatDbBare (float v)
{
    if (! std::isfinite (v) || v <= -100.0f)
        return "-inf";

    return juce::String (v, 1);
}
} // namespace

void MeterGroupComponent::PeakNumericSmoother::reset() noexcept
{
    held = duty = -200.0f;
    holdTicksLeft = 0;
}

void MeterGroupComponent::PeakNumericSmoother::tick (float raw, float dtSec, int holdTicksAt30Hz) noexcept
{
    if (! std::isfinite (raw))
        raw = -200.0f;

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

    const float tau = 0.3f;
    const float a = 1.0f - std::exp (-dtSec / tau);
    held += (raw - held) * a;
    duty = held;
}

juce::String MeterGroupComponent::PeakNumericSmoother::formatDb (float v) noexcept
{
    if (! std::isfinite (v) || v <= -100.0f)
        return "-inf";

    return juce::String (v, 1) + " dB";
}

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
        meter0_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);
        meter1_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);
        meter0_->setClipResetCallback (&MeterGroupComponent::peakResetThunk, this);
        meter1_->setClipResetCallback (&MeterGroupComponent::peakResetThunk, this);
        addAndMakeVisible (*meter0_);
        addAndMakeVisible (*meter1_);

        provider0_.setScaleMode (ScaleMode::Top24Db);
        provider1_.setScaleMode (ScaleMode::Top24Db);
        provider0_.setDisplayMode (DisplayMode::Peak);
        provider1_.setDisplayMode (DisplayMode::Peak);
        provider0_.setHoldEnabled (true);
        provider1_.setHoldEnabled (true);
        pushLevelRenderStates();
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
    provider0_.updateFromValues (lPeak, lPeak, false, false);
    provider1_.updateFromValues (rPeak, rPeak, false, false);
    provider0_.resetPeakHold();
    provider1_.resetPeakHold();
    peakSmooth0_.reset();
    peakSmooth1_.reset();
    maxPeakLDb_ = -200.0f;
    maxPeakRDb_ = -200.0f;
    pushLevelRenderStates();
}

int MeterGroupComponent::getPreferredWidth() const noexcept
{
    const auto& m = ui_.metrics();
    const int meterW = m.meterGroupMeterW;
    const int gap = m.meterGroupGap;
    return (channelCount_ <= 1) ? meterW : (meterW * 2 + gap);
}

void MeterGroupComponent::resetPeakHolds() noexcept
{
    handlePeakReset();
}

void MeterGroupComponent::pushLevelRenderStates()
{
    provider0_.fillRenderState (renderState0_);
    provider1_.fillRenderState (renderState1_);

    if (meter0_ != nullptr)
        meter0_->setRenderState (renderState0_);
    if (meter1_ != nullptr)
        meter1_->setRenderState (renderState1_);
}

void MeterGroupComponent::sync (double hostSampleRate, float dtSec)
{
    juce::ignoreUnused (hostSampleRate);

    const int holdTicks = juce::jmax (1, static_cast<int> (std::round (1.0f / juce::jmax (1.0e-6f, dtSec))));

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

    peakSmooth0_.tick (lPeak, dtSec, holdTicks);
    peakSmooth1_.tick (rPeak, dtSec, holdTicks);

    if (std::isfinite (lPeak))
        maxPeakLDb_ = juce::jmax (maxPeakLDb_, lPeak);
    if (std::isfinite (rPeak))
        maxPeakRDb_ = juce::jmax (maxPeakRDb_, rPeak);

    constexpr float kClipThresholdDb = 0.0f;  // digital full-scale
    const bool clippedL = renderState0_.clipLatched || (std::isfinite (lPeak) && lPeak >= kClipThresholdDb);
    const bool clippedR = renderState1_.clipLatched || (std::isfinite (rPeak) && rPeak >= kClipThresholdDb);

    provider0_.updateFromValues (lPeak, lPeak, clippedL, false);
    provider1_.updateFromValues (rPeak, rPeak, clippedR, false);
    pushLevelRenderStates();

    const auto lCurrentText = formatDbBare (peakSmooth0_.duty);
    const auto rCurrentText = formatDbBare (peakSmooth1_.duty);
    const auto lMaxText = formatDbBare (maxPeakLDb_);
    const auto rMaxText = formatDbBare (maxPeakRDb_);

    if (meter0_ != nullptr)
        meter0_->setNumericReadoutOverride (true, lCurrentText, lMaxText);
    if (meter1_ != nullptr)
        meter1_->setNumericReadoutOverride (true, rCurrentText, rMaxText);
}

void MeterGroupComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    g.setColour (theme.textMuted.withAlpha (0.7f));
    g.setFont (type.labelFont());
    g.drawText (titleFor (kind_), labelArea_, juce::Justification::centred);
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
    const int gap = m.meterGroupGap;

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
    row.removeFromLeft (gap);
    auto right = row.removeFromLeft (meterW);

    if (meter0_ != nullptr)
        meter0_->setBounds (left);
    if (meter1_ != nullptr)
        meter1_->setBounds (right);
}
