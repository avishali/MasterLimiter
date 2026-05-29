#include "MainView.h"
#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"

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

juce::String formatTimingReadout (const MasterLimiterAudioProcessor& p)
{
    return "Blk " + juce::String (p.getAudioBlockMaxUs())
         + " | Drv " + juce::String (p.getSectionMaxUsDrive())
         + " / Up " + juce::String (p.getSectionMaxUsUpsample())
         + " / Clp " + juce::String (p.getSectionMaxUsClipperPeak())
         + " / Env " + juce::String (p.getSectionMaxUsEnvelope())
         + " / GMul " + juce::String (p.getSectionMaxUsGainMul())
         + " / Dn " + juce::String (p.getSectionMaxUsDownsample())
         + " / Out " + juce::String (p.getSectionMaxUsOutput()) + " us"
         + " | Clicks " + juce::String (p.getOutputClickCount())
         + " / D " + juce::String (p.getOutputMaxDeltaSeen(), 2)
         + " / Abs " + juce::String (p.getOutputMaxAbsSeen(), 2);
}
} // namespace

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

//==============================================================================
MainView::MainView (mdsp_ui::UiContext& uiContext, MasterLimiterAudioProcessor& processor)
    : ui_ (uiContext),
      processor_ (processor),
      apvts_ (processor.getAPVTS()),
      meterIn_ (ui_, processor, MeterGroupComponent::BusKind::Input),
      meterGr_ (ui_, processor),
      meterOut_ (ui_, processor, MeterGroupComponent::BusKind::Output),
      lufsPanel_ (ui_, processor)
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
    setupLabel (lblReleaseSustain_);
    setupLabel (lblReleaseAuto_);
    setupLabel (lblLookahead_);
    setupLabel (lblCeilingMode_);
    setupLabel (lblStereoLink_);
    setupLabel (lblMsLink_);
    setupLabel (lblCharacter_);
    setupLabel (lblGainMatchNote_);
    setupLabel (lblLearnInputLufs_);
    setupLabel (lblIoInputTrim_);
    setupLabel (lblIoInputReadout_);
    setupLabel (lblIoOutputTrim_);
    setupLabel (lblIoOutputReadout_);

    styleRotary (sldGainDrive_);
    styleRotary (sldClipperDrive_);
    styleRotary (sldCeiling_);
    styleRotary (sldRelease_);
    styleRotary (sldReleaseSustain_);
    styleRotary (sldLookahead_);
    styleRotary (sldStereoLink_);
    styleRotary (sldMsLink_);
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
    addAndMakeVisible (sldCeiling_);
    addAndMakeVisible (sldRelease_);
    addAndMakeVisible (sldReleaseSustain_);
    btnReleaseAuto_.setClickingTogglesState (true);
    addAndMakeVisible (btnReleaseAuto_);
    addAndMakeVisible (sldLookahead_);
    btnCeilingMode_.setClickingTogglesState (true);
    addAndMakeVisible (btnCeilingMode_);
    addAndMakeVisible (sldCharacter_);
    addAndMakeVisible (sldStereoLink_);
    addAndMakeVisible (sldMsLink_);
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
    addAndMakeVisible (btnGainCeilingLink_);
    addAndMakeVisible (btnLimiterActive_);
    addAndMakeVisible (btnGainMatchAutoTrack_);
    addAndMakeVisible (btnLearnInputGain_);
    addAndMakeVisible (sldIoInputTrimL_);
    addAndMakeVisible (sldIoInputTrimR_);
    addAndMakeVisible (btnIoInputLink_);
    addAndMakeVisible (sldIoOutputTrimL_);
    addAndMakeVisible (sldIoOutputTrimR_);
    addAndMakeVisible (btnIoOutputLink_);
    addAndMakeVisible (btnBypass_);

    attGainDrive_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::input_gain_db), sldGainDrive_);
    attLimiterActive_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::limiter_active), btnLimiterActive_);
    attClipperDrive_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::clipper_drive_db), sldClipperDrive_);
    attCeiling_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::ceiling_db), sldCeiling_);
    attGainCeilingLink_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::gain_ceiling_link), btnGainCeilingLink_);
    attRelease_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::release_ms), sldRelease_);
    attReleaseSustain_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::release_sustain_ratio), sldReleaseSustain_);
    attReleaseAuto_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::release_auto), btnReleaseAuto_);
    attLookahead_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::lookahead_ms), sldLookahead_);
    attStereoLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::stereo_link_pct), sldStereoLink_);
    attMsLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::ms_link_pct), sldMsLink_);
    attCharacter_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::character), sldCharacter_);
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
    useSliderTextboxFormat (sldReleaseSustain_, 1, juce::String());
    useSliderTextboxFormat (sldLookahead_, 2, " ms");
    useSliderTextboxFormat (sldStereoLink_, 0, "%");
    useSliderTextboxFormat (sldMsLink_, 0, "%");
    useSliderTextboxFormat (sldIoInputTrimL_, 2, " dB");
    useSliderTextboxFormat (sldIoInputTrimR_, 2, " dB");
    useSliderTextboxFormat (sldIoOutputTrimL_, 2, " dB");
    useSliderTextboxFormat (sldIoOutputTrimR_, 2, " dB");
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

    sldIoInputTrimL_.onValueChange = [this] { syncLinkedFaders (sldIoInputTrimL_, sldIoInputTrimR_, btnIoInputLink_); };
    sldIoInputTrimR_.onValueChange = [this] { syncLinkedFaders (sldIoInputTrimR_, sldIoInputTrimL_, btnIoInputLink_); };
    sldIoOutputTrimL_.onValueChange = [this] { syncLinkedFaders (sldIoOutputTrimL_, sldIoOutputTrimR_, btnIoOutputLink_); };
    sldIoOutputTrimR_.onValueChange = [this] { syncLinkedFaders (sldIoOutputTrimR_, sldIoOutputTrimL_, btnIoOutputLink_); };
    btnIoInputLink_.onClick = [this] { syncLinkedFaders (sldIoInputTrimL_, sldIoInputTrimR_, btnIoInputLink_); };
    btnIoOutputLink_.onClick = [this] { syncLinkedFaders (sldIoOutputTrimL_, sldIoOutputTrimR_, btnIoOutputLink_); };
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
    updateLimiterActiveState();
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::ceiling_mode))))
        updateCeilingModeButton (c->getIndex());

    sldGainDrive_.setTooltip ("Drive into the limiter in dB.");
    btnLimiterActive_.setTooltip ("Turns the limiter section on or off while preserving I/O trims.");
    sldClipperDrive_.setTooltip ("Pre-envelope clipper drive in dB.");
    lblClipperReadout_.setTooltip ("Current / max clipper excess. Click to reset max.");
    btnGainCeilingLink_.setTooltip ("When enabled, Gain and Ceiling move inversely.");
    btnGainMatchAutoTrack_.setTooltip (kPlaceholderTooltip);
    btnLearnInputGain_.setTooltip (kPlaceholderTooltip);
    sldIoInputTrimL_.setTooltip ("Left input trim before the drive stage.");
    sldIoInputTrimR_.setTooltip ("Right input trim before the drive stage.");
    btnIoInputLink_.setTooltip ("When enabled, moving one Input fader mirrors the other.");
    sldIoOutputTrimL_.setTooltip ("Left output trim after the ceiling stage.");
    sldIoOutputTrimR_.setTooltip ("Right output trim after the ceiling stage.");
    btnIoOutputLink_.setTooltip ("When enabled, moving one Output fader mirrors the other.");
    btnBypass_.setTooltip (kPlaceholderTooltip);
    sldCeiling_.setTooltip ("Output ceiling in dBFS.");
    sldRelease_.setTooltip ("Limiter release time in milliseconds.");
    sldReleaseSustain_.setTooltip ("Multiplier on release time for sustained peaks (1–10×).");
    btnReleaseAuto_.setTooltip ("When Auto is on, release time follows the program.");
    sldLookahead_.setTooltip ("Lookahead delay in milliseconds (higher catches peaks earlier).");
    btnCeilingMode_.setTooltip ("Sample peak vs true-peak ceiling enforcement.");
    sldStereoLink_.setTooltip ("Stereo Link amount: 100% fully linked, 0% independent L/R limiting.");
    sldMsLink_.setTooltip ("Mid/side stereo link amount (0–100%).");
    sldCharacter_.setTooltip ("Character mode: Clean, Tight, or Aggressive.");
}

MainView::~MainView() = default;

void MainView::styleRotary (juce::Slider& s) const
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setScrollWheelEnabled (false);
}

void MainView::styleIoTrimFader (juce::Slider& s) const
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

void MainView::updateLimiterActiveState()
{
    const bool active = btnLimiterActive_.getToggleState();
    btnLimiterActive_.setButtonText (active ? "Limiter On" : "Limiter Off");
    lastLimiterActive_ = active;
    repaint (maximizerPanelArea_);
}

void MainView::mouseDown (const juce::MouseEvent& e)
{
    const auto eventOnClipReadout = e.eventComponent == &lblClipperReadout_
                                 || lblClipperReadout_.getBounds().contains (e.getEventRelativeTo (this).getPosition());
    if (! eventOnClipReadout)
        return;

    processor_.resetMaxClip();
    lblClipperReadout_.setText (formatClipReadout (processor_.getCurrentClipDb(), 0.0f), juce::dontSendNotification);
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
    headerMode_.setBounds (184, 12, 770, 28);
    btnBypass_.setBounds (982, 12, 92, 28);
    btnLimiterActive_.setBounds (190, 94, 116, 24);

    lblGainDrive_.setBounds (48, 116, 140, 18);
    sldGainDrive_.setBounds (40, 134, 156, 136);
    lblGainDriveRange_.setBounds (42, 270, 154, 18);
    btnGainCeilingLink_.setBounds (204, 222, 90, 26);

    lblCeiling_.setBounds (300, 116, 156, 18);
    sldCeiling_.setBounds (300, 134, 156, 136);
    lblCeilingMode_.setBounds (318, 274, 120, 18);
    btnCeilingMode_.setBounds (330, 294, 96, 26);
    lblTruePeak_.setBounds (300, 324, 156, 20);

    lblCharacter_.setBounds (34, 314, 128, 18);
    sldCharacter_.setBounds (34, 336, 400, 24);

    const int knobY = 388;
    const int knobW = 78;
    const int knobH = 86;
    lblRelease_.setBounds (42, knobY, knobW, 18);
    sldRelease_.setBounds (42, knobY + 18, knobW, knobH);
    lblReleaseSustain_.setBounds (132, knobY, knobW, 18);
    sldReleaseSustain_.setBounds (132, knobY + 18, knobW, knobH);
    lblLookahead_.setBounds (222, knobY, knobW, 18);
    sldLookahead_.setBounds (222, knobY + 18, knobW, knobH);
    lblReleaseAuto_.setBounds (312, knobY, knobW, 18);
    btnReleaseAuto_.setBounds (314, knobY + 46, 74, 28);
    lblStereoLink_.setBounds (402, knobY, knobW, 18);
    sldStereoLink_.setBounds (402, knobY + 18, knobW, knobH);
    lblMsLink_.setBounds (492, knobY, knobW, 18);
    sldMsLink_.setBounds (492, knobY + 18, knobW, knobH);
    lblClipperDrive_.setBounds (582, knobY, knobW, 18);
    sldClipperDrive_.setBounds (582, knobY + 18, knobW, knobH - 18);
    lblClipperReadout_.setBounds (582, knobY + 82, knobW, 18);

    gainMatchLabelArea_ = { 34, 492, 150, 18 };
    btnGainMatchAutoTrack_.setBounds (34, 514, 126, 30);
    lblGainMatchNote_.setBounds (170, 514, 220, 30);

    meterGr_.setBounds (674, 104, 54, 354);

    btnLearnInputGain_.setBounds (830, 102, 54, 22);
    lblLearnInputLufs_.setBounds (894, 102, 92, 22);

    meterIn_.setBounds (790, 154, 116, 314);
    meterOut_.setBounds (934, 154, 116, 314);

    lblIoInputTrim_.setBounds (790, 134, 116, 18);
    sldIoInputTrimL_.setBounds (800, 184, 42, 250);
    sldIoInputTrimR_.setBounds (856, 184, 42, 250);
    btnIoInputLink_.setBounds (826, 470, 44, 20);
    lblIoInputReadout_.setBounds (790, 492, 116, 20);

    lblIoOutputTrim_.setBounds (934, 134, 116, 18);
    sldIoOutputTrimL_.setBounds (946, 184, 42, 250);
    sldIoOutputTrimR_.setBounds (1000, 184, 42, 250);
    btnIoOutputLink_.setBounds (970, 470, 44, 20);
    lblIoOutputReadout_.setBounds (934, 492, 116, 20);

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
    btnLimiterActive_.toFront (false);
    btnBypass_.toFront (false);

    meterStripArea_ = meterGr_.getBounds().getUnion (meterIn_.getBounds())
                                      .getUnion (meterOut_.getBounds())
                                      .getUnion (lufsPanel_.getBounds())
                                      .getUnion (lblTruePeak_.getBounds());
}

void MainView::syncMetersFromProcessor()
{
    const double sr = processor_.getSampleRate();
    constexpr float dt = 1.0f / 30.0f;

    meterIn_.sync (sr, dt);
    meterGr_.sync (dt);
    meterOut_.sync (sr, dt);
    lufsPanel_.refresh();
    headerMode_.setText (formatTimingReadout (processor_), juce::dontSendNotification);

    int ceilingIdx = 0;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::ceiling_mode))))
        ceilingIdx = c->getIndex();
    updateCeilingModeButton (ceilingIdx);

    if (btnLimiterActive_.getToggleState() != lastLimiterActive_)
        updateLimiterActiveState();

    const float clipDb = processor_.getCurrentClipDb();
    const float maxClipDb = processor_.getMaxClipSinceResetDb();
    if (clipDb > 0.0f)
    {
        clipLedLevel_ = 1.0f;
    }
    else
    {
        clipLedLevel_ *= std::exp (-dt / 0.2f);
    }
    lblClipperReadout_.setText (formatClipReadout (clipDb, maxClipDb), juce::dontSendNotification);
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
    lufsPanel_.refresh();
}
