#include "MainView.h"
#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"
#include "ui/meters/ClipBallistics.h"

#include <cmath>

namespace
{
juce::String pid (std::string_view sv)
{
    return { sv.data(), static_cast<size_t> (sv.size()) };
}

juce::String formatDb1 (double v)
{
    return juce::String (v, 1) + " dB";
}

juce::String formatSignedDb1 (float v)
{
    if (! std::isfinite (v))
        v = 0.0f;

    return juce::String (v >= 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
}

juce::String formatPositiveBare (float v)
{
    if (! std::isfinite (v))
        v = 0.0f;

    return juce::String (juce::jmax (0.0f, v), 1);
}

juce::String formatClipReadout (float currentDb, float maxDb)
{
    return "Clip " + formatPositiveBare (currentDb) + " / " + formatPositiveBare (maxDb);
}

juce::String scaleLabel (MeterGroupComponent::ScaleMode mode)
{
    switch (mode)
    {
        case MeterGroupComponent::ScaleMode::FullRange: return "Full";
        case MeterGroupComponent::ScaleMode::Top48Db: return "48";
        case MeterGroupComponent::ScaleMode::Top24Db: return "24";
        case MeterGroupComponent::ScaleMode::Top12Db: return "12";
        case MeterGroupComponent::ScaleMode::Top6Db: return "6";
    }
    return "Full";
}

MeterGroupComponent::ScaleMode scaleFromIndex (int index) noexcept
{
    switch (juce::jlimit (0, 4, index))
    {
        case 1: return MeterGroupComponent::ScaleMode::Top48Db;
        case 2: return MeterGroupComponent::ScaleMode::Top24Db;
        case 3: return MeterGroupComponent::ScaleMode::Top12Db;
        case 4: return MeterGroupComponent::ScaleMode::Top6Db;
        default: return MeterGroupComponent::ScaleMode::FullRange;
    }
}

int scaleIndex (MeterGroupComponent::ScaleMode mode) noexcept
{
    switch (mode)
    {
        case MeterGroupComponent::ScaleMode::FullRange: return 0;
        case MeterGroupComponent::ScaleMode::Top48Db: return 1;
        case MeterGroupComponent::ScaleMode::Top24Db: return 2;
        case MeterGroupComponent::ScaleMode::Top12Db: return 3;
        case MeterGroupComponent::ScaleMode::Top6Db: return 4;
    }
    return 0;
}

void appendScaleTicks (juce::Array<float>& ticks, MeterGroupComponent::ScaleMode mode)
{
    switch (mode)
    {
        case MeterGroupComponent::ScaleMode::FullRange:
            ticks.addArray ({ 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f, -72.0f, -96.0f, -120.0f });
            break;
        case MeterGroupComponent::ScaleMode::Top48Db:
            ticks.addArray ({ 0.0f, -12.0f, -24.0f, -36.0f, -48.0f });
            break;
        case MeterGroupComponent::ScaleMode::Top24Db:
            ticks.addArray ({ 0.0f, -3.0f, -6.0f, -9.0f, -12.0f, -15.0f, -18.0f, -24.0f });
            break;
        case MeterGroupComponent::ScaleMode::Top12Db:
            ticks.addArray ({ 0.0f, -3.0f, -6.0f, -9.0f, -12.0f });
            break;
        case MeterGroupComponent::ScaleMode::Top6Db:
            ticks.addArray ({ 0.0f, -1.0f, -2.0f, -3.0f, -4.0f, -5.0f, -6.0f });
            break;
    }
}

juce::String tickLabel (float db)
{
    if (std::abs (db) < 0.001f)
        return "0";
    if (db > 0.0f)
        return "+" + juce::String (db, 0);
    return juce::String (db, 0);
}

} // namespace

void MainView::CompensationBar::setColours (juce::Colour negative, juce::Colour positive, juce::Colour track) noexcept
{
    negative_ = negative;
    positive_ = positive;
    track_ = track;
}

void MainView::CompensationBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
        return;

    const auto track = bounds.withHeight (4.0f).withCentre (bounds.getCentre());
    g.setColour (track_);
    g.fillRoundedRectangle (track, 2.0f);

    const float v = processor_ != nullptr ? juce::jlimit (-12.0f, 12.0f, processor_->getCompGainDb()) : 0.0f;
    const float centre = track.getCentreX();
    const float half = track.getWidth() * 0.5f;
    const float pixels = half * (std::abs (v) / 12.0f);

    if (pixels > 0.5f)
    {
        auto fill = track;
        if (v < 0.0f)
            fill.setBounds (centre - pixels, track.getY(), pixels, track.getHeight());
        else
            fill.setBounds (centre, track.getY(), pixels, track.getHeight());

        g.setColour (v < 0.0f ? negative_ : positive_);
        g.fillRoundedRectangle (fill, 2.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float> { centre - 0.5f, track.getY() - 1.0f, 1.0f, track.getHeight() + 2.0f });
}

void MainView::TpReadoutSmoother::reset() noexcept
{
    held = duty = -200.0f;
    holdTicksLeft = 0;
}

void MainView::TpReadoutSmoother::tick (float raw, float dtSec, int holdTicksAt30Hz) noexcept
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

juce::String MainView::TpReadoutSmoother::formatDb (float v) noexcept
{
    if (! std::isfinite (v) || v <= -100.0f)
        return "-inf dB";

    return juce::String (v, 1) + " dB";
}

juce::Rectangle<int> MainView::ValueSlider::getValueLabelBounds() const
{
    auto bounds = getLocalBounds();

    if (valueLabelMode_ == ValueLabelMode::Below)
        return bounds.removeFromBottom (20).reduced (1, 1);

    return bounds.withSizeKeepingCentre (juce::jmin (72, juce::jmax (44, bounds.getWidth() - 18)), 24);
}

void MainView::ValueSlider::paint (juce::Graphics& g)
{
    juce::Slider::paint (g);

    if (valueLabelMode_ != ValueLabelMode::Below)
        return;

    const auto valueArea = getValueLabelBounds();
    g.setColour (findColour (juce::Slider::textBoxTextColourId, true).withAlpha (isEnabled() ? 0.82f : 0.38f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (getTextFromValue (getValue()), valueArea, juce::Justification::centred, true);
}

void MainView::ValueSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown()
        && e.getNumberOfClicks() >= 2
        && getValueLabelBounds().contains (e.getPosition())
        && onValueEditRequest != nullptr)
    {
        onValueEditRequest (*this);
        return;
    }

    juce::Slider::mouseDown (e);
}

//==============================================================================
MainView::MainView (mdsp_ui::UiContext& uiContext, MasterLimiterAudioProcessor& processor)
    : ui_ (uiContext),
      processor_ (processor),
      apvts_ (processor.getAPVTS()),
      tooltipWindow_ (this, 650),
      meterIn_ (ui_, processor, MeterGroupComponent::BusKind::Input),
      meterGr_ (ui_, processor),
      meterOut_ (ui_, processor, MeterGroupComponent::BusKind::Output),
      lufsPanel_ (ui_, processor),
      clipBallistics_ (master_limiter_ui::makeClipBallisticsState())
{
    const auto& theme = ui_.theme();

    header_.setJustificationType (juce::Justification::centredLeft);
    header_.setFont (ui_.type().titleFont().withHeight (20.0f).boldened());
    header_.setColour (juce::Label::textColourId, theme.accent.brighter (0.5f));
    addAndMakeVisible (header_);

    headerMode_.setJustificationType (juce::Justification::centredLeft);
    headerMode_.setFont (ui_.type().labelFont().withHeight (10.0f));
    headerMode_.setColour (juce::Label::textColourId, theme.textMuted);
    addAndMakeVisible (headerMode_);

    auto setupLabel = [&] (juce::Label& l)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setFont (ui_.type().labelFont().withHeight (11.0f));
        l.setColour (juce::Label::textColourId, theme.textMuted);
        addAndMakeVisible (l);
    };

    setupLabel (lblGainDrive_);
    setupLabel (lblGainDriveRange_);
    setupLabel (lblClipperDrive_);
    setupLabel (lblClipperReadout_);
    setupLabel (lblCeiling_);
    setupLabel (lblRelease_);
    setupLabel (lblReleaseAuto_);
    setupLabel (lblCeilingMode_);
    setupLabel (lblStereoLink_);
    setupLabel (lblBandColor_);
    setupLabel (lblCharacter_);
    setupLabel (lblGainMatchNote_);
    setupLabel (lblLearnInputLufs_);
    setupLabel (lblIoInputTrim_);
    setupLabel (lblIoInputReadout_);
    setupLabel (lblIoOutputTrim_);
    setupLabel (lblIoOutputReadout_);
    setupLabel (lblMeterScaleRange_);

    styleRotary (sldGainDrive_);
    styleRotary (sldClipperDrive_);
    styleRotary (sldCeiling_);
    styleRotary (sldRelease_);
    styleRotary (sldStereoLink_);
    styleRotary (sldBandColor_);
    styleHorizontalPlaceholder (sldCharacter_);
    styleIoTrimFader (sldIoInputTrimL_);
    styleIoTrimFader (sldIoInputTrimR_);
    styleIoTrimFader (sldIoOutputTrimL_);
    styleIoTrimFader (sldIoOutputTrimR_);

    sldGainDrive_.setRange (0.0, 24.0, 0.1);
    sldGainDrive_.setNumDecimalPlacesToDisplay (2);
    sldGainDrive_.setTextValueSuffix (" dB");
    sldGainDrive_.setValue (0.0, juce::dontSendNotification);

    addAndMakeVisible (sldGainDrive_);
    addAndMakeVisible (sldClipperDrive_);
    btnClipperMode_.setClickingTogglesState (false);
    addAndMakeVisible (btnClipperMode_);
    addAndMakeVisible (sldCeiling_);
    addAndMakeVisible (sldRelease_);
    btnReleaseAuto_.setClickingTogglesState (true);
    addAndMakeVisible (btnReleaseAuto_);
    cmbAutoReleaseMode_.addItemList (juce::StringArray { "Transparent", "Balanced", "Reactive" }, 1);
    cmbAutoReleaseMode_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (cmbAutoReleaseMode_);
    btnCeilingMode_.setClickingTogglesState (true);
    addAndMakeVisible (btnCeilingMode_);
    addAndMakeVisible (sldCharacter_);
    addAndMakeVisible (sldStereoLink_);
    btnStereoMode_.setClickingTogglesState (false);
    addAndMakeVisible (btnStereoMode_);
    addAndMakeVisible (sldBandColor_);
    lblClipperReadout_.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    lblClipperReadout_.addMouseListener (this, false);

    btnGainCeilingLink_.setClickingTogglesState (true);
    btnLimiterActive_.setClickingTogglesState (true);
    btnGainMatchAutoTrack_.setClickingTogglesState (true);
    btnIoInputLink_.setClickingTogglesState (true);
    btnIoInputLink_.setButtonText ("L/R");
    btnIoInputLink_.setToggleState (true, juce::dontSendNotification);
    btnIoOutputLink_.setClickingTogglesState (true);
    btnIoOutputLink_.setButtonText ("L/R");
    btnIoOutputLink_.setToggleState (true, juce::dontSendNotification);
    btnBypass_.setClickingTogglesState (true);
    btnMeterRms_.setClickingTogglesState (true);
    btnMeterRms_.setToggleState (false, juce::dontSendNotification);
    compGainBar_.setProcessor (&processor_);
    compGainBar_.setColours (theme.warning.withAlpha (0.85f),
                             theme.accent.withAlpha (0.9f),
                             theme.grid.withAlpha (0.65f));
    addAndMakeVisible (btnGainCeilingLink_);
    addAndMakeVisible (btnLimiterActive_);
    addAndMakeVisible (btnGainMatchAutoTrack_);
    addAndMakeVisible (compGainBar_);
    addAndMakeVisible (btnLearnInputGain_);
    addAndMakeVisible (sldIoInputTrimL_);
    addAndMakeVisible (sldIoInputTrimR_);
    addAndMakeVisible (btnIoInputLink_);
    addAndMakeVisible (sldIoOutputTrimL_);
    addAndMakeVisible (sldIoOutputTrimR_);
    addAndMakeVisible (btnIoOutputLink_);
    addAndMakeVisible (btnBypass_);
    addAndMakeVisible (btnMeterScaleMinus_);
    addAndMakeVisible (btnMeterScalePlus_);
    addAndMakeVisible (btnMeterRms_);

    attGainDrive_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::input_gain_db), sldGainDrive_);
    attLimiterActive_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::limiter_active), btnLimiterActive_);
    attPluginBypass_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::plugin_bypass), btnBypass_);
    attClipperDrive_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::clipper_drive_db), sldClipperDrive_);
    if (auto* clipperModeParam = apvts_.getParameter (pid (param::clipper_mode)))
    {
        attClipperMode_ = std::make_unique<juce::ParameterAttachment> (*clipperModeParam,
                                                                       [this] (float value)
                                                                       {
                                                                           updateClipperModeButton ((int) std::lround (value));
                                                                       },
                                                                       nullptr);
        attClipperMode_->sendInitialUpdate();
    }
    attCeiling_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::ceiling_db), sldCeiling_);
    attGainCeilingLink_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::gain_ceiling_link), btnGainCeilingLink_);
    attRelease_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::release_ms), sldRelease_);
    attReleaseAuto_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::release_auto), btnReleaseAuto_);
    attAutoReleaseMode_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::auto_release_mode), cmbAutoReleaseMode_);
    attBandColor_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::band_color), sldBandColor_);
    attCharacter_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::character), sldCharacter_);
    attGainMatchAutoTrack_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::gain_match_auto), btnGainMatchAutoTrack_);
    attIoInputTrimL_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::io_input_l_db), sldIoInputTrimL_);
    attIoInputTrimR_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::io_input_r_db), sldIoInputTrimR_);
    attIoInputLink_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::io_input_link), btnIoInputLink_);
    attIoOutputTrimL_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::io_output_l_db), sldIoOutputTrimL_);
    attIoOutputTrimR_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::io_output_r_db), sldIoOutputTrimR_);
    attIoOutputLink_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::io_output_link), btnIoOutputLink_);

    auto useSliderTextboxFormat = [] (juce::Slider& slider, int numDecimals, const juce::String& suffix)
    {
        slider.textFromValueFunction = nullptr;
        slider.setNumDecimalPlacesToDisplay (numDecimals);
        slider.setTextValueSuffix (suffix);
    };

    useSliderTextboxFormat (sldGainDrive_, 2, " dB");
    useSliderTextboxFormat (sldClipperDrive_, 2, " dB");
    useSliderTextboxFormat (sldCeiling_, 2, " dB");
    useSliderTextboxFormat (sldRelease_, 0, " ms");
    useSliderTextboxFormat (sldStereoLink_, 0, juce::String());
    useSliderTextboxFormat (sldBandColor_, 0, juce::String());
    sldStereoLink_.textFromValueFunction = [] (double v) { return juce::String (v, 0) + "%"; };
    sldBandColor_.textFromValueFunction = [] (double v) { return juce::String (v, 0) + "%"; };
    useSliderTextboxFormat (sldIoInputTrimL_, 2, " dB");
    useSliderTextboxFormat (sldIoInputTrimR_, 2, " dB");
    useSliderTextboxFormat (sldIoOutputTrimL_, 2, " dB");
    useSliderTextboxFormat (sldIoOutputTrimR_, 2, " dB");
    setupValueEdit (sldGainDrive_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldClipperDrive_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldCeiling_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldRelease_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldStereoLink_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldBandColor_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldIoInputTrimL_, ValueSlider::ValueLabelMode::Below);
    setupValueEdit (sldIoInputTrimR_, ValueSlider::ValueLabelMode::Below);
    setupValueEdit (sldIoOutputTrimL_, ValueSlider::ValueLabelMode::Below);
    setupValueEdit (sldIoOutputTrimR_, ValueSlider::ValueLabelMode::Below);
    sldCharacter_.textFromValueFunction = [] (double v)
    {
        switch (juce::jlimit (0, 2, (int) std::lround (v)))
        {
            case 0:  return juce::String ("Clean");
            case 2:  return juce::String ("Aggressive");
            default: return juce::String ("Tight");
        }
    };

    addAndMakeVisible (meterIn_);
    addAndMakeVisible (meterGr_);
    addAndMakeVisible (meterOut_);
    addAndMakeVisible (lufsPanel_);

    lblTruePeak_.setJustificationType (juce::Justification::centredLeft);
    lblTruePeak_.setFont (ui_.type().labelFont().withHeight (11.0f));
    lblTruePeak_.setColour (juce::Label::textColourId, theme.textMuted);
    addAndMakeVisible (lblTruePeak_);

    btnResetPeaks_.onClick = [this]
    {
        resetPeakHolds();
    };
    addAndMakeVisible (btnResetPeaks_);

    btnLimiterActive_.onClick = [this]
    {
        updateLimiterActiveState();
    };
    btnBypass_.onClick = [this]
    {
        updateBypassButtonState();
    };
    btnMeterScaleMinus_.onClick = [this] { stepMeterScale (-1); };
    btnMeterScalePlus_.onClick = [this] { stepMeterScale (1); };
    btnMeterRms_.onClick = [this] { setMeterShowRms (btnMeterRms_.getToggleState()); };
    btnReleaseAuto_.onClick = [this] { updateReleaseAutoControls(); };
    btnClipperMode_.onClick = [this]
    {
        if (attClipperMode_ != nullptr)
            attClipperMode_->setValueAsCompleteGesture (lastClipperModeIdx_ == 0 ? 1.0f : 0.0f);
    };

    sldIoInputTrimL_.onValueChange = [this] { syncLinkedFaders (sldIoInputTrimL_, sldIoInputTrimR_, btnIoInputLink_); };
    sldIoInputTrimR_.onValueChange = [this] { syncLinkedFaders (sldIoInputTrimR_, sldIoInputTrimL_, btnIoInputLink_); };
    sldIoOutputTrimL_.onValueChange = [this] { syncLinkedFaders (sldIoOutputTrimL_, sldIoOutputTrimR_, btnIoOutputLink_); };
    sldIoOutputTrimR_.onValueChange = [this] { syncLinkedFaders (sldIoOutputTrimR_, sldIoOutputTrimL_, btnIoOutputLink_); };
    btnIoInputLink_.onClick = [this] { syncLinkedFaders (sldIoInputTrimL_, sldIoInputTrimR_, btnIoInputLink_); };
    btnIoOutputLink_.onClick = [this] { syncLinkedFaders (sldIoOutputTrimL_, sldIoOutputTrimR_, btnIoOutputLink_); };
    btnStereoMode_.onClick = [this]
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::stereo_mode))))
        {
            const int nextIdx = c->getIndex() == 0 ? 1 : 0;
            c->beginChangeGesture();
            c->setValueNotifyingHost (nextIdx == 0 ? 0.0f : 1.0f);
            c->endChangeGesture();
            updateStereoModeControls();
        }
    };
    sldBandColor_.onValueChange = [this]
    {
        lblBandColor_.setText ("Color " + juce::String ((int) std::lround (sldBandColor_.getValue())) + "%", juce::dontSendNotification);
    };
    sldBandColor_.onDragEnd = [this]
    {
        constexpr double snapWindowPct = 3.0;
        for (double detent : { 0.0, 50.0, 100.0 })
        {
            if (std::abs (sldBandColor_.getValue() - detent) <= snapWindowPct)
            {
                sldBandColor_.setValue (detent, juce::sendNotificationAsync);
                break;
            }
        }
    };
    btnCeilingMode_.onClick = [this]
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::ceiling_mode))))
        {
            const int nextIdx = c->getIndex() == 0 ? 1 : 0;
            c->beginChangeGesture();
            c->setValueNotifyingHost (nextIdx == 0 ? 0.0f : 1.0f);
            c->endChangeGesture();
            updateCeilingModeButton (nextIdx);
        }
    };
    updateIoTrimReadouts();
    lblBandColor_.setText ("Color " + juce::String ((int) std::lround (sldBandColor_.getValue())) + "%", juce::dontSendNotification);
    updateLimiterActiveState();
    updateBypassButtonState();
    updateReleaseAutoControls (true);
    updateStereoModeControls();
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::ceiling_mode))))
        updateCeilingModeButton (c->getIndex());
    setMeterScaleMode (currentMeterScale_);
    setMeterShowRms (false);

    valueEditor_.setMultiLine (false);
    valueEditor_.setReturnKeyStartsNewLine (false);
    valueEditor_.setSelectAllWhenFocused (true);
    valueEditor_.setJustification (juce::Justification::centred);
    valueEditor_.setFont (ui_.type().labelFont().withHeight (11.0f));
    valueEditor_.setColour (juce::TextEditor::backgroundColourId, theme.background.brighter (0.08f));
    valueEditor_.setColour (juce::TextEditor::outlineColourId, theme.accent.withAlpha (0.75f));
    valueEditor_.setColour (juce::TextEditor::focusedOutlineColourId, theme.accent.brighter (0.35f));
    valueEditor_.setColour (juce::TextEditor::textColourId, theme.text);
    valueEditor_.onReturnKey = [this] { finishValueEdit (true); };
    valueEditor_.onEscapeKey = [this] { finishValueEdit (false); };
    valueEditor_.onFocusLost = [this] { finishValueEdit (true); };
    addChildComponent (valueEditor_);

    sldGainDrive_.setTooltip ("Drive into the limiter in dB.");
    btnLimiterActive_.setTooltip ("Turns the limiter section on or off while preserving I/O trims.");
    sldClipperDrive_.setTooltip ("Sets the clipper threshold from -12 to 0 dB.");
    btnClipperMode_.setTooltip ("Toggles the clipper curve between Hard and Soft.");
    lblClipperReadout_.setTooltip ("Shows current and maximum clip reduction in dB; click to reset max.");
    btnGainCeilingLink_.setTooltip ("When enabled, Gain and Ceiling move inversely.");
    btnGainMatchAutoTrack_.setTooltip ("Continuously matches limiter output loudness to the learned reference.");
    btnLearnInputGain_.setTooltip ("Click to learn a 3 s LUFS reference. Right-click to clear it.");
    btnLearnInputGain_.addMouseListener (this, false);
    sldIoInputTrimL_.setTooltip ("Left input trim before the drive stage.");
    sldIoInputTrimR_.setTooltip ("Right input trim before the drive stage.");
    btnIoInputLink_.setTooltip ("When enabled, moving one Input fader mirrors the other.");
    sldIoOutputTrimL_.setTooltip ("Left output trim after the ceiling stage.");
    sldIoOutputTrimR_.setTooltip ("Right output trim after the ceiling stage.");
    btnIoOutputLink_.setTooltip ("When enabled, moving one Output fader mirrors the other.");
    btnBypass_.setTooltip ("Plugin bypass. When on, the limiter is bypassed but I/O trims and Gain-Match still apply.");
    btnMeterScaleMinus_.setTooltip ("Zoom the I/O meter range out.");
    btnMeterScalePlus_.setTooltip ("Zoom the I/O meter range in.");
    btnMeterRms_.setTooltip ("Show RMS fill and RMS readout on the I/O meters.");
    sldCeiling_.setTooltip ("Output ceiling in dBFS.");
    sldRelease_.setTooltip ("Limiter release time in milliseconds.");
    btnReleaseAuto_.setTooltip ("When Auto is on, release time follows the program.");
    cmbAutoReleaseMode_.setTooltip ("Auto release response: Transparent, Balanced, or Reactive.");
    btnCeilingMode_.setTooltip ("Sample peak vs true-peak ceiling enforcement.");
    btnStereoMode_.setTooltip ("Click to toggle whether the wideband link operates in L/R stereo or M/S.");
    sldStereoLink_.setTooltip ("Link amount for the active stereo mode: 100% fully linked, 0% independent.");
    sldBandColor_.setTooltip ("Multiband Color: 0% glued/warm, 50% balanced, 100% open/bright.");
    sldCharacter_.setTooltip ("Character mode: Clean, Tight, or Aggressive.");
    lblTruePeak_.setTooltip ("Shows the current sample-peak or true-peak output readout in dB.");
    btnResetPeaks_.setTooltip ("Resets held peaks, maximum gain reduction, and clip maximums.");

    btnLearnInputGain_.onClick = [this]
    {
        processor_.armLearnReference();
        updateLearnStateDisplay();
    };

    updateLearnStateDisplay();
    updateCompensationReadout();
}

MainView::~MainView() = default;

void MainView::styleRotary (ValueSlider& s) const
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setScrollWheelEnabled (false);
}

void MainView::styleIoTrimFader (ValueSlider& s) const
{
    s.setSliderStyle (juce::Slider::LinearVertical);
    s.setSliderSnapsToMousePosition (false);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRange (-24.0, 24.0, 0.01);
    s.setNumDecimalPlacesToDisplay (2);
    s.setTextValueSuffix (" dB");
    s.setValue (0.0, juce::dontSendNotification);
    s.setScrollWheelEnabled (false);
}

void MainView::styleHorizontalPlaceholder (juce::Slider& s) const
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setScrollWheelEnabled (false);
}

void MainView::setupValueEdit (ValueSlider& s, ValueSlider::ValueLabelMode mode)
{
    s.setValueLabelMode (mode);
    s.onValueEditRequest = [this] (ValueSlider& slider) { beginValueEdit (slider); };
}

void MainView::beginValueEdit (ValueSlider& s)
{
    if (! s.isEnabled())
        return;

    finishValueEdit (false);
    editingSlider_ = &s;
    valueEditor_.setText (s.getTextFromValue (s.getValue()), false);
    positionValueEditor();
    valueEditor_.setVisible (true);
    valueEditor_.toFront (true);
    valueEditor_.grabKeyboardFocus();
    valueEditor_.selectAll();
}

void MainView::positionValueEditor()
{
    if (editingSlider_ == nullptr)
        return;

    const auto labelBounds = editingSlider_->getValueLabelBounds()
                                 .translated (editingSlider_->getX(), editingSlider_->getY())
                                 .expanded (3, 2);
    valueEditor_.setBounds (labelBounds);
}

void MainView::finishValueEdit (bool commit)
{
    if (editingSlider_ == nullptr || finishingValueEdit_)
        return;

    const juce::ScopedValueSetter<bool> guard (finishingValueEdit_, true);
    auto* slider = editingSlider_;
    editingSlider_ = nullptr;

    if (commit)
    {
        const auto parsed = slider->getValueFromText (valueEditor_.getText());
        const auto clamped = juce::jlimit (slider->getMinimum(), slider->getMaximum(), parsed);
        slider->startedDragging();
        slider->setValue (clamped, juce::sendNotificationSync);
        slider->stoppedDragging();
    }

    valueEditor_.setVisible (false);
}

void MainView::syncLinkedFaders (juce::Slider& source, juce::Slider& target, juce::ToggleButton& link)
{
    if (adjustingIoFaders_)
        return;

    const juce::ScopedValueSetter<bool> guard (adjustingIoFaders_, true);

    if (link.getToggleState())
        target.setValue (source.getValue(), juce::sendNotificationSync);

    updateIoTrimReadouts();
}

void MainView::updateIoTrimReadouts()
{
    const auto inputValue = btnIoInputLink_.getToggleState()
                                ? sldIoInputTrimL_.getValue()
                                : (sldIoInputTrimL_.getValue() + sldIoInputTrimR_.getValue()) * 0.5;
    const auto outputValue = btnIoOutputLink_.getToggleState()
                                 ? sldIoOutputTrimL_.getValue()
                                 : (sldIoOutputTrimL_.getValue() + sldIoOutputTrimR_.getValue()) * 0.5;

    lblIoInputReadout_.setText (formatDb1 (inputValue), juce::dontSendNotification);
    lblIoOutputReadout_.setText (formatDb1 (outputValue), juce::dontSendNotification);
}

void MainView::updateCeilingModeButton (int ceilingIdx)
{
    const bool isTruePeak = ceilingIdx == 1;
    btnCeilingMode_.setButtonText (isTruePeak ? "TP" : "SP");
    btnCeilingMode_.setToggleState (isTruePeak, juce::dontSendNotification);
}

void MainView::updateStereoModeControls()
{
    int stereoIdx = 0;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::stereo_mode))))
        stereoIdx = c->getIndex();

    const bool msMode = stereoIdx == 1;
    btnStereoMode_.setButtonText (msMode ? "M/S" : "Stereo");
    btnStereoMode_.setToggleState (msMode, juce::dontSendNotification);

    if (stereoIdx == lastStereoModeIdx_)
        return;

    lastStereoModeIdx_ = stereoIdx;
    const auto activeParam = msMode ? param::ms_link_pct : param::stereo_link_pct;
    attLink_.reset();
    attLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (activeParam), sldStereoLink_);
    repaint (btnStereoMode_.getBounds().getUnion (sldStereoLink_.getBounds()).expanded (8, 8));
}

void MainView::updateClipperModeButton (int clipperIdx)
{
    const bool soft = clipperIdx >= 1;
    lastClipperModeIdx_ = soft ? 1 : 0;
    btnClipperMode_.setButtonText (soft ? "Soft" : "Hard");
    btnClipperMode_.setToggleState (soft, juce::dontSendNotification);
    repaint (btnClipperMode_.getBounds().expanded (4, 4));
}

void MainView::updateReleaseAutoControls (bool forceRepaint)
{
    const bool autoRelease = btnReleaseAuto_.getToggleState();
    const bool changed = autoRelease != lastReleaseAuto_;
    lastReleaseAuto_ = autoRelease;

    sldRelease_.setEnabled (! autoRelease);
    lblRelease_.setEnabled (! autoRelease);
    cmbAutoReleaseMode_.setEnabled (autoRelease);

    if (changed || forceRepaint)
        repaint (sldRelease_.getBounds()
                     .getUnion (lblRelease_.getBounds())
                     .getUnion (btnReleaseAuto_.getBounds())
                     .getUnion (cmbAutoReleaseMode_.getBounds())
                     .expanded (8, 8));
}

void MainView::updateLimiterActiveState()
{
    const bool active = btnLimiterActive_.getToggleState();
    btnLimiterActive_.setButtonText (active ? "Lim On" : "Lim Off");
    lastLimiterActive_ = active;
    repaint (maximizerPanelArea_);
}

void MainView::updateBypassButtonState()
{
    const auto& theme = ui_.theme();
    const bool bypassed = btnBypass_.getToggleState();
    btnBypass_.setButtonText (bypassed ? "Bypassed" : "Bypass");
    if (bypassed)
        btnBypass_.setColour (juce::TextButton::buttonColourId, theme.warning.withAlpha (0.85f));
    else
        btnBypass_.removeColour (juce::TextButton::buttonColourId);
    repaint (btnBypass_.getBounds().expanded (4, 4));
}

void MainView::updateLearnStateDisplay()
{
    const auto& theme = ui_.theme();
    const float ref = processor_.getLearnedRefLufs();
    const bool hasRef = std::isfinite (ref);

    switch (processor_.getLearnState())
    {
        case MasterLimiterAudioProcessor::LearnState::Armed:
        {
            const auto tick = juce::Time::getMillisecondCounter() % 1000u;
            const float phase = static_cast<float> (tick) / 1000.0f;
            const float alpha = 0.35f + 0.35f * (0.5f + 0.5f * std::sin (phase * juce::MathConstants<float>::twoPi));
            btnLearnInputGain_.setButtonText ("Listening...");
            btnLearnInputGain_.setColour (juce::TextButton::buttonColourId, theme.accent.withAlpha (alpha));
            lblLearnInputLufs_.setText ("Listening...", juce::dontSendNotification);
            break;
        }

        case MasterLimiterAudioProcessor::LearnState::Captured:
            btnLearnInputGain_.setButtonText ("Learned");
            btnLearnInputGain_.setColour (juce::TextButton::buttonColourId, theme.accent.withAlpha (0.55f));
            lblLearnInputLufs_.setText (hasRef ? (juce::String (ref, 1) + " LUFS") : "No ref", juce::dontSendNotification);
            break;

        case MasterLimiterAudioProcessor::LearnState::Idle:
        default:
            btnLearnInputGain_.setButtonText ("Learn");
            btnLearnInputGain_.removeColour (juce::TextButton::buttonColourId);
            lblLearnInputLufs_.setText (hasRef ? (juce::String (ref, 1) + " LUFS") : "No ref", juce::dontSendNotification);
            break;
    }

    repaint (btnLearnInputGain_.getBounds().getUnion (lblLearnInputLufs_.getBounds()).expanded (8, 2));
}

void MainView::updateCompensationReadout()
{
    lblGainMatchNote_.setText (formatSignedDb1 (processor_.getCompGainDb()), juce::dontSendNotification);
    compGainBar_.repaint();
}

void MainView::setMeterScaleMode (MeterGroupComponent::ScaleMode mode)
{
    currentMeterScale_ = mode;
    meterIn_.setScaleMode (currentMeterScale_);
    meterOut_.setScaleMode (currentMeterScale_);
    updateMeterScaleControls();
    repaint (meterScaleColumnArea_.expanded (4, 2));
}

void MainView::stepMeterScale (int delta)
{
    setMeterScaleMode (scaleFromIndex (scaleIndex (currentMeterScale_) + delta));
}

void MainView::updateMeterScaleControls()
{
    const int idx = scaleIndex (currentMeterScale_);
    btnMeterScaleMinus_.setEnabled (idx > 0);
    btnMeterScalePlus_.setEnabled (idx < 4);
    lblMeterScaleRange_.setText (scaleLabel (currentMeterScale_), juce::dontSendNotification);

    const auto& theme = ui_.theme();
    btnMeterScaleMinus_.setColour (juce::TextButton::buttonColourId,
                                   (idx > 0 ? theme.panel.brighter (0.06f) : theme.panel.darker (0.08f)));
    btnMeterScalePlus_.setColour (juce::TextButton::buttonColourId,
                                  (idx < 4 ? theme.panel.brighter (0.06f) : theme.panel.darker (0.08f)));
}

void MainView::setMeterShowRms (bool shouldShow)
{
    btnMeterRms_.setToggleState (shouldShow, juce::dontSendNotification);
    meterIn_.setShowRms (shouldShow);
    meterOut_.setShowRms (shouldShow);

    const auto& theme = ui_.theme();
    if (shouldShow)
        btnMeterRms_.setColour (juce::TextButton::buttonColourId, theme.accent.withAlpha (0.48f));
    else
        btnMeterRms_.setColour (juce::TextButton::buttonColourId, theme.panel.brighter (0.04f));
}

void MainView::paintMeterScaleColumn (juce::Graphics& g)
{
    if (meterScaleColumnArea_.isEmpty())
        return;

    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    const auto area = meterScaleColumnArea_.toFloat();
    const float yMax = area.getBottom();
    const float h = area.getHeight();
    const float xLeft = area.getX();
    const float xRight = area.getRight();

    g.setColour (theme.grid.withAlpha (0.22f));
    g.drawVerticalLine (static_cast<int> (std::round (area.getCentreX())), area.getY(), area.getBottom());
    g.setFont (type.labelFont().withHeight (9.0f));

    juce::Array<float> ticks;
    appendScaleTicks (ticks, currentMeterScale_);

    for (const auto db : ticks)
    {
        const float norm = mdsp_ui::meters::MeterRenderStateProvider::normaliseDb (db, currentMeterScale_);
        const float y = yMax - (norm * h);
        if (y < area.getY() - 0.5f || y > area.getBottom() + 0.5f)
            continue;

        const bool zero = std::abs (db) < 0.001f;
        const auto tickColour = theme.textMuted.withAlpha (zero ? 0.58f : 0.42f);
        g.setColour (tickColour);
        g.drawLine (xLeft + 1.0f,
                    y,
                    xRight - 1.0f,
                    y,
                    zero ? 1.45f : 0.8f);

        g.drawText (tickLabel (db),
                    juce::Rectangle<float> (xLeft, y - 5.0f, area.getWidth(), 10.0f),
                    juce::Justification::centred);
    }
}

void MainView::mouseDown (const juce::MouseEvent& e)
{
    const auto eventOnLearn = e.eventComponent == &btnLearnInputGain_
                           || btnLearnInputGain_.getBounds().contains (e.getEventRelativeTo (this).getPosition());
    if (eventOnLearn && e.mods.isRightButtonDown())
    {
        processor_.clearLearnedReference();
        updateLearnStateDisplay();
        updateCompensationReadout();
        return;
    }

    const auto eventOnClipReadout = e.eventComponent == &lblClipperReadout_
                                 || lblClipperReadout_.getBounds().contains (e.getEventRelativeTo (this).getPosition());
    if (! eventOnClipReadout)
        return;

    processor_.resetMaxClip();
    master_limiter_ui::resetClipBallistics (*clipBallistics_);
    clipLedLevel_ = 0.0f;
    lblClipperReadout_.setText (formatClipReadout (master_limiter_ui::getClipReadoutCurrent (*clipBallistics_), 0.0f), juce::dontSendNotification);
    repaint (lblClipperReadout_.getBounds().expanded (12, 2));
}

void MainView::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    const auto& m = ui_.metrics();

    g.fillAll (theme.background);

    if (! headerArea_.isEmpty())
    {
        g.setColour (theme.background.brighter (0.04f));
        g.fillRect (headerArea_);
    }

    auto drawPanel = [&] (juce::Rectangle<int> area, const juce::String& title)
    {
        if (area.isEmpty())
            return;

        const auto panel = area.toFloat();
        g.setColour (theme.panel.brighter (0.02f));
        g.fillRoundedRectangle (panel, m.rLarge);
        g.setColour (theme.borderDivider);
        g.drawRoundedRectangle (panel.reduced (0.5f), m.rLarge, m.strokeThin);

        auto titleArea = area.reduced (18, 10).removeFromTop (18);
        const bool limiterOffTitle = title == "M A X I M I Z E R" && ! btnLimiterActive_.getToggleState();
        g.setColour (limiterOffTitle ? juce::Colours::red.withAlpha (0.78f)
                                      : theme.textMuted.withAlpha (0.8f));
        g.setFont (type.labelFont().withHeight (13.0f).boldened());
        g.drawText (title, titleArea, title == "I/O" ? juce::Justification::centred : juce::Justification::centredLeft, true);
    };

    drawPanel (maximizerPanelArea_, "M A X I M I Z E R");
    drawPanel (ioPanelArea_, "I/O");

    if (! gainMatchLabelArea_.isEmpty())
    {
        g.setColour (theme.textMuted.withAlpha (0.75f));
        g.setFont (type.labelFont().withHeight (12.0f).boldened());
        g.drawText ("G A I N  M A T C H", gainMatchLabelArea_, juce::Justification::centredLeft, true);
    }

    g.setColour (theme.textMuted.withAlpha (0.72f));
    g.setFont (type.labelFont().withHeight (11.0f));
    g.drawText ("Clean", juce::Rectangle<int> { 34, 362, 80, 16 }, juce::Justification::centredLeft);
    g.drawText ("Aggressive", juce::Rectangle<int> { 330, 362, 104, 16 }, juce::Justification::centredRight);

    if (! sldBandColor_.getBounds().isEmpty())
    {
        const auto b = sldBandColor_.getBounds();
        const int y = b.getBottom() + 2;
        const int x0 = b.getX() + 8;
        const int x1 = b.getCentreX();
        const int x2 = b.getRight() - 8;

        g.setColour (theme.textMuted.withAlpha (0.55f));
        for (int x : { x0, x1, x2 })
            g.drawVerticalLine (x, (float) y, (float) y + 5.0f);

        g.setFont (type.labelFont().withHeight (9.0f));
        g.drawText ("Glued", x0 - 22, y + 7, 44, 12, juce::Justification::centred, true);
        g.drawText ("Bal", x1 - 18, y + 7, 36, 12, juce::Justification::centred, true);
        g.drawText ("Open", x2 - 22, y + 7, 44, 12, juce::Justification::centred, true);
    }

    paintMeterScaleColumn (g);

    if (clipLedLevel_ > 0.001f)
    {
        const auto led = lblClipperDrive_.getBounds().removeFromRight (12).withSizeKeepingCentre (8, 8).toFloat();
        g.setColour (theme.warning.withAlpha (juce::jlimit (0.0f, 1.0f, clipLedLevel_)));
        g.fillEllipse (led);
    }

    if (! footerArea_.isEmpty())
    {
        g.setColour (theme.background.brighter (0.02f));
        g.fillRect (footerArea_);
        g.setColour (theme.grid.withAlpha (0.45f));
        auto footerTopLine = footerArea_;
        g.fillRect (footerTopLine.removeFromTop (1));
    }
}

void MainView::resized()
{
    headerArea_ = { 0, 0, 1100, 52 };
    maximizerPanelArea_ = { 16, 64, 736, 540 };
    ioPanelArea_ = { 768, 64, 316, 540 };
    footerArea_ = {};

    header_.setBounds (24, 8, 150, 34);
    headerMode_.setBounds (184, 12, 790, 24);
    btnBypass_.setBounds (982, 12, 92, 28);
    btnLimiterActive_.setBounds (206, 134, 86, 22);

    lblGainDrive_.setBounds (48, 116, 140, 18);
    sldGainDrive_.setBounds (40, 134, 156, 136);
    lblGainDriveRange_.setBounds (42, 270, 154, 18);
    btnGainCeilingLink_.setBounds (206, 164, 86, 22);

    lblCeiling_.setBounds (300, 116, 156, 18);
    sldCeiling_.setBounds (300, 134, 156, 136);
    lblCeilingMode_.setBounds (0, 0, 0, 0);
    btnCeilingMode_.setBounds (206, 194, 86, 22);
    lblTruePeak_.setBounds (300, 324, 156, 20);

    lblCharacter_.setBounds (34, 314, 128, 18);
    sldCharacter_.setBounds (34, 336, 400, 24);

    const int knobY = 388;
    const int knobW = 78;
    const int knobH = 86;
    lblRelease_.setBounds (62, knobY, knobW, 18);
    sldRelease_.setBounds (62, knobY + 18, knobW, knobH);
    lblReleaseAuto_.setBounds (174, knobY, 86, 18);
    btnReleaseAuto_.setBounds (184, knobY + 22, 76, 24);
    cmbAutoReleaseMode_.setBounds (158, knobY + 56, 98, 22);
    btnStereoMode_.setBounds (316, knobY + 48, 86, 24);
    lblStereoLink_.setBounds (414, knobY, knobW, 18);
    sldStereoLink_.setBounds (414, knobY + 18, knobW, knobH);
    lblClipperDrive_.setBounds (495, 116, 140, 18);
    sldClipperDrive_.setBounds (495, 134, 140, 120);
    btnClipperMode_.setBounds (515, 260, 100, 22);
    lblClipperReadout_.setBounds (495, 286, 140, 18);
    lblBandColor_.setBounds (526, 314, knobW, 18);
    sldBandColor_.setBounds (526, 332, knobW, knobH);

    gainMatchLabelArea_ = { 152, 528, 468, 18 };
    btnGainMatchAutoTrack_.setBounds (152, 550, 126, 30);
    lblGainMatchNote_.setBounds (288, 550, 76, 30);
    compGainBar_.setBounds (372, 562, 48, 8);
    // Slice 11b2.1: Learn sits with Auto/Track so the LUFS feature reads as one group.
    btnLearnInputGain_.setBounds (432, 550, 84, 30);
    lblLearnInputLufs_.setBounds (524, 550, 96, 30);

    meterGr_.setBounds (674, 104, 54, 354);

    meterIn_.setBounds (790, 154, 116, 314);
    meterOut_.setBounds (948, 154, 116, 314);

    lblIoInputTrim_.setBounds (790, 134, 116, 18);
    sldIoInputTrimL_.setBounds (800, 184, 42, 250);
    sldIoInputTrimR_.setBounds (856, 184, 42, 250);
    btnIoInputLink_.setBounds (826, 470, 44, 20);
    lblIoInputReadout_.setBounds (790, 492, 116, 20);

    lblIoOutputTrim_.setBounds (948, 134, 116, 18);
    sldIoOutputTrimL_.setBounds (960, 184, 42, 250);
    sldIoOutputTrimR_.setBounds (1014, 184, 42, 250);
    btnIoOutputLink_.setBounds (984, 470, 44, 20);
    lblIoOutputReadout_.setBounds (948, 492, 116, 20);

    const auto inputScaleRef = meterIn_.getScaleReferenceBoundsInParent();
    const int scaleX = meterIn_.getRight() + 4;
    const int scaleW = juce::jmax (24, meterOut_.getX() - meterIn_.getRight() - 8);
    meterScaleColumnArea_ = { scaleX, inputScaleRef.getY(), scaleW, inputScaleRef.getHeight() };
    btnMeterScaleMinus_.setBounds (scaleX, 114, 17, 17);
    btnMeterScalePlus_.setBounds (scaleX + scaleW - 17, 114, 17, 17);
    lblMeterScaleRange_.setBounds (scaleX, 132, scaleW, 18);
    btnMeterRms_.setBounds (scaleX, 470, scaleW, 20);

    lufsPanel_.setBounds (790, 520, 160, 68);
    btnResetPeaks_.setBounds (962, 540, 100, 26);

    sldIoInputTrimL_.toFront (false);
    sldIoInputTrimR_.toFront (false);
    sldIoOutputTrimL_.toFront (false);
    sldIoOutputTrimR_.toFront (false);
    btnIoInputLink_.toFront (false);
    btnIoOutputLink_.toFront (false);
    lblIoInputReadout_.toFront (false);
    lblIoOutputReadout_.toFront (false);
    btnMeterScaleMinus_.toFront (false);
    btnMeterScalePlus_.toFront (false);
    btnMeterRms_.toFront (false);
    lblMeterScaleRange_.toFront (false);
    btnLimiterActive_.toFront (false);
    btnBypass_.toFront (false);
    btnClipperMode_.toFront (false);
    cmbAutoReleaseMode_.toFront (false);
    compGainBar_.toFront (false);
    valueEditor_.toFront (false);

    meterStripArea_ = meterGr_.getBounds().getUnion (meterIn_.getBounds())
                                      .getUnion (meterOut_.getBounds())
                                      .getUnion (lufsPanel_.getBounds())
                                      .getUnion (lblTruePeak_.getBounds())
                                      .getUnion (meterScaleColumnArea_)
                                      .getUnion (btnMeterScaleMinus_.getBounds())
                                      .getUnion (btnMeterScalePlus_.getBounds())
                                      .getUnion (btnMeterRms_.getBounds())
                                      .getUnion (lblMeterScaleRange_.getBounds());

    positionValueEditor();
}

void MainView::syncMetersFromProcessor()
{
    const double sr = processor_.getSampleRate();
    constexpr float dt = 1.0f / 30.0f;

    meterIn_.sync (sr, dt);
    meterGr_.sync (dt);
    meterOut_.sync (sr, dt);
    lufsPanel_.refresh();
    updateLearnStateDisplay();
    updateCompensationReadout();
    updateBypassButtonState();
    updateReleaseAutoControls();

    int ceilingIdx = 0;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::ceiling_mode))))
        ceilingIdx = c->getIndex();
    updateCeilingModeButton (ceilingIdx);
    updateStereoModeControls();

    if (btnLimiterActive_.getToggleState() != lastLimiterActive_)
        updateLimiterActiveState();

    const float clipDb = processor_.getCurrentClipDb();
    const float maxClipDb = processor_.getMaxClipSinceResetDb();
    master_limiter_ui::processClipReadout (*clipBallistics_, clipDb, dt);
    clipLedLevel_ = master_limiter_ui::processClipLed (*clipBallistics_, clipDb > 0.0f, dt);
    lblClipperReadout_.setText (formatClipReadout (master_limiter_ui::getClipReadoutCurrent (*clipBallistics_), maxClipDb), juce::dontSendNotification);
    repaint (lblClipperReadout_.getBounds().expanded (12, 2));

    const auto tpTag = (ceilingIdx == 1 ? juce::String ("TP") : juce::String ("SP"));
    const float tpDb = processor_.getOutputTpDb();
    tpSmoother_.tick (tpDb, dt, 30);
    lblTruePeak_.setText (tpTag + ": " + TpReadoutSmoother::formatDb (tpSmoother_.duty), juce::dontSendNotification);
}

void MainView::repaintMeterStrip()
{
    if (! meterStripArea_.isEmpty())
        repaint (meterStripArea_);
}

void MainView::resetPeakHolds()
{
    processor_.getLoudnessAnalyzer().resetPeak();
    meterIn_.resetPeakHolds();
    meterGr_.resetPeakHolds();
    meterOut_.resetPeakHolds();
    processor_.resetMaxGr();
    processor_.resetMaxClip();
    tpSmoother_.reset();
    master_limiter_ui::resetClipBallistics (*clipBallistics_);
    clipLedLevel_ = 0.0f;
    lufsPanel_.refresh();
}
