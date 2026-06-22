#include "ui/DevControlsComponent.h"

#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"

namespace
{
namespace palette
{
const juce::Colour bgDeep       = juce::Colour::fromRGB (0x0d, 0x10, 0x15);
const juce::Colour panel        = juce::Colour::fromRGB (0x16, 0x1b, 0x22);
const juce::Colour control      = juce::Colour::fromRGB (0x1e, 0x24, 0x2e);
const juce::Colour border       = juce::Colour::fromRGB (0x2c, 0x33, 0x3f);
const juce::Colour text         = juce::Colour::fromRGB (0xe8, 0xed, 0xf3);
const juce::Colour textMuted    = juce::Colour::fromRGB (0x8c, 0x97, 0xa6);
const juce::Colour accent       = juce::Colour::fromRGB (0x33, 0xd2, 0xbe);
const juce::Colour accentBright = juce::Colour::fromRGB (0x5b, 0xe7, 0xd6);
const juce::Colour warning      = juce::Colour::fromRGB (0xe8, 0x70, 0x4f);
} // namespace palette

juce::String pid (std::string_view sv)
{
    return { sv.data(), static_cast<size_t> (sv.size()) };
}
} // namespace

DevControlsComponent::DevControlsComponent (MasterLimiterAudioProcessor& processor,
                                            mdsp_ui::UiContext& uiContext)
    : ui_ (uiContext),
      apvts_ (processor.getAPVTS())
{
    header_.setJustificationType (juce::Justification::centredLeft);
    header_.setFont (ui_.type().labelFont().withHeight (13.0f).boldened());
    header_.setColour (juce::Label::textColourId, palette::warning.withAlpha (0.95f));
    addAndMakeVisible (header_);

    viewport_.setViewedComponent (&content_, false);
    viewport_.setScrollBarsShown (true, false);
    viewport_.setScrollBarThickness (10);
    addAndMakeVisible (viewport_);

    for (auto* group : { &groupAttack_,
                         &groupLookahead_,
                         &groupReleaseEngine_,
                         &groupLookaheadRelease_,
                         &groupAdaptiveRelease_,
                         &groupBandScaling_,
                         &groupManualRelease_ })
    {
        setupGroup (*group);
        content_.addAndMakeVisible (*group);
    }

    setupLabel (lblAttackMode_, "Mode");
    setupCombo (cmbAttackMode_);
    cmbAttackMode_.addItem ("Ramp", 1);
    cmbAttackMode_.addItem ("Real", 2);
    cmbAttackMode_.setTooltip ("Ramp = cosine pre-peak ramp (current). Real = decoupled attack time-constant.");

    setupLabel (lblAttack_, "Attack");
    setupSlider (sldAttack_, 2, " ms");
    sldAttack_.setTooltip ("Ramp-mode attack: overrides Character; capped by the active lookahead window.");

    setupLabel (lblRealAttack_, "Real Atk");
    setupSlider (sldRealAttack_, 2, " ms");
    sldRealAttack_.setTooltip ("Decoupled attack time-constant; slow = transients punch through to the ceiling (punch).");

    setupLabel (lblLookaheadBand_, "Band");
    setupSlider (sldLookaheadBand_, 2, " ms");
    sldLookaheadBand_.setTooltip ("Per-band audio delay and envelope lookahead window.");

    setupLabel (lblLookaheadWide_, "Wide");
    setupSlider (sldLookaheadWide_, 2, " ms");
    sldLookaheadWide_.setTooltip ("Wideband audio delay and envelope lookahead window.");

    setupLabel (lblReleaseEngine_, "Engine");
    setupCombo (cmbReleaseEngine_);
    cmbReleaseEngine_.addItem ("Adaptive", 1);
    cmbReleaseEngine_.addItem ("Lookahead", 2);
    cmbReleaseEngine_.setTooltip ("Switches Auto release between Adaptive Sigma and Lookahead follower.");

    setupLabel (lblLaRelease_, "Time");
    setupSlider (sldLaRelease_, 1, " ms");
    sldLaRelease_.setTooltip ("Lookahead follower release time in milliseconds.");

    setupLabel (lblLaPoles_, "Poles");
    setupCombo (cmbLaPoles_);
    cmbLaPoles_.addItem ("2", 1);
    cmbLaPoles_.addItem ("3", 2);
    cmbLaPoles_.addItem ("4", 3);
    cmbLaPoles_.setTooltip ("Lookahead follower smoothing poles.");

    setupLabel (lblSigmaAttack_, "Sigma Atk");
    setupSlider (sldSigmaAttack_, 1, " ms");
    sldSigmaAttack_.setTooltip ("Adaptive Sigma tracker attack time.");

    setupLabel (lblSigmaDecay_, "Sigma Decay");
    setupSlider (sldSigmaDecay_, 2, {});
    sldSigmaDecay_.setTooltip ("Adaptive Sigma decay multiplier.");

    setupLabel (lblLowScale_, "Low x");
    setupSlider (sldLowScale_, 2, {});
    sldLowScale_.setTooltip ("Low-band auto-release timing multiplier.");

    setupLabel (lblHighScale_, "High/Wide x");
    setupSlider (sldHighScale_, 2, {});
    sldHighScale_.setTooltip ("High-band and wideband auto-release timing multiplier.");

    setupLabel (lblSustainRatio_, "Sustain Ratio");
    setupSlider (sldSustainRatio_, 2, {});
    sldSustainRatio_.setTooltip ("Manual-release sustain split. Active only when Release Auto is Off.");

    for (auto* label : { &lblAttackMode_, &lblAttack_, &lblRealAttack_, &lblLookaheadBand_, &lblLookaheadWide_,
                         &lblReleaseEngine_, &lblLaRelease_, &lblLaPoles_,
                         &lblSigmaAttack_, &lblSigmaDecay_, &lblLowScale_,
                         &lblHighScale_, &lblSustainRatio_ })
    {
        content_.addAndMakeVisible (*label);
    }

    for (auto* slider : { &sldAttack_, &sldRealAttack_, &sldLookaheadBand_, &sldLookaheadWide_,
                          &sldLaRelease_, &sldSigmaAttack_, &sldSigmaDecay_,
                          &sldLowScale_, &sldHighScale_, &sldSustainRatio_ })
    {
        content_.addAndMakeVisible (*slider);
    }

    content_.addAndMakeVisible (cmbAttackMode_);
    content_.addAndMakeVisible (cmbReleaseEngine_);
    content_.addAndMakeVisible (cmbLaPoles_);

    attAttack_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_attack_ms), sldAttack_);
    attAttackMode_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::dev_attack_mode), cmbAttackMode_);
    attRealAttack_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_real_attack_ms), sldRealAttack_);
    attLookaheadBand_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_lookahead_band_ms), sldLookaheadBand_);
    attLookaheadWide_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_lookahead_wide_ms), sldLookaheadWide_);
    attReleaseEngine_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::dev_release_engine), cmbReleaseEngine_);
    attLaRelease_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_la_release_ms), sldLaRelease_);
    attLaPoles_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::dev_la_release_poles), cmbLaPoles_);
    attSigmaAttack_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_sigma_attack_ms), sldSigmaAttack_);
    attSigmaDecay_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_sigma_decay_scale), sldSigmaDecay_);
    attLowScale_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_low_band_release_scale), sldLowScale_);
    attHighScale_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_high_band_release_scale), sldHighScale_);
    attSustainRatio_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::release_sustain_ratio), sldSustainRatio_);

    if (auto* releaseAuto = apvts_.getParameter (pid (param::release_auto)))
    {
        attReleaseAuto_ = std::make_unique<juce::ParameterAttachment> (
            *releaseAuto,
            [this] (float value)
            {
                updateManualReleaseEnabled (value < 0.5f);
            },
            nullptr);
        attReleaseAuto_->sendInitialUpdate();
    }

    if (auto* attackMode = apvts_.getParameter (pid (param::dev_attack_mode)))
    {
        attAttackModeListener_ = std::make_unique<juce::ParameterAttachment> (
            *attackMode,
            [this] (float value)
            {
                updateAttackModeControls ((int) value);
            },
            nullptr);
        attAttackModeListener_->sendInitialUpdate();
    }
}

void DevControlsComponent::paint (juce::Graphics& g)
{
    g.fillAll (palette::bgDeep);
}

void DevControlsComponent::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    header_.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);
    viewport_.setBounds (area);

    const int contentW = juce::jmax (260, area.getWidth() - 14);
    const int margin = 10;
    const int labelW = contentW < 360 ? 72 : 112;
    const int rowH = 28;
    const int gap = 10;
    int y = 10;

    if (contentW < 360)
    {
        for (auto* slider : { &sldAttack_, &sldLookaheadBand_, &sldLookaheadWide_, &sldLaRelease_,
                              &sldSigmaAttack_, &sldSigmaDecay_, &sldLowScale_, &sldHighScale_, &sldSustainRatio_ })
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 20);
    }

    auto placeGroup = [&] (juce::GroupComponent& group, int h)
    {
        group.setBounds (margin, y, contentW - 2 * margin, h);
        y += h + gap;
        return group.getBounds().reduced (16, 22);
    };

    auto placeSliderRow = [&] (juce::Rectangle<int> row, juce::Label& label, juce::Slider& slider)
    {
        label.setBounds (row.removeFromLeft (labelW));
        row.removeFromLeft (8);
        slider.setBounds (row);
    };

    auto placeComboRow = [&] (juce::Rectangle<int> row, juce::Label& label, juce::ComboBox& combo)
    {
        label.setBounds (row.removeFromLeft (labelW));
        row.removeFromLeft (8);
        combo.setBounds (row.withHeight (24));
    };

    auto inner = placeGroup (groupAttack_, 136);
    placeComboRow (inner.removeFromTop (rowH), lblAttackMode_, cmbAttackMode_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblAttack_, sldAttack_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblRealAttack_, sldRealAttack_);

    inner = placeGroup (groupLookahead_, 104);
    placeSliderRow (inner.removeFromTop (rowH), lblLookaheadBand_, sldLookaheadBand_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblLookaheadWide_, sldLookaheadWide_);

    inner = placeGroup (groupReleaseEngine_, 72);
    placeComboRow (inner.removeFromTop (rowH), lblReleaseEngine_, cmbReleaseEngine_);

    inner = placeGroup (groupLookaheadRelease_, 104);
    placeSliderRow (inner.removeFromTop (rowH), lblLaRelease_, sldLaRelease_);
    inner.removeFromTop (8);
    placeComboRow (inner.removeFromTop (rowH), lblLaPoles_, cmbLaPoles_);

    inner = placeGroup (groupAdaptiveRelease_, 104);
    placeSliderRow (inner.removeFromTop (rowH), lblSigmaAttack_, sldSigmaAttack_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblSigmaDecay_, sldSigmaDecay_);

    inner = placeGroup (groupBandScaling_, 104);
    placeSliderRow (inner.removeFromTop (rowH), lblLowScale_, sldLowScale_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblHighScale_, sldHighScale_);

    inner = placeGroup (groupManualRelease_, 72);
    placeSliderRow (inner.removeFromTop (rowH), lblSustainRatio_, sldSustainRatio_);

    content_.setSize (contentW, y + 4);
}

void DevControlsComponent::setupGroup (juce::GroupComponent& group)
{
    group.setColour (juce::GroupComponent::outlineColourId, palette::warning.withAlpha (0.30f));
    group.setColour (juce::GroupComponent::textColourId, palette::textMuted.withAlpha (0.90f));
}

void DevControlsComponent::setupLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredRight);
    label.setFont (ui_.type().labelFont().withHeight (11.0f));
    label.setColour (juce::Label::textColourId, palette::textMuted);
}

void DevControlsComponent::setupSlider (juce::Slider& slider, int decimals, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 78, 20);
    slider.setSliderSnapsToMousePosition (false);
    slider.setScrollWheelEnabled (false);
    slider.setNumDecimalPlacesToDisplay (decimals);
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::backgroundColourId, palette::control);
    slider.setColour (juce::Slider::trackColourId, palette::warning.withAlpha (0.72f));
    slider.setColour (juce::Slider::thumbColourId, palette::accentBright);
    slider.setColour (juce::Slider::textBoxTextColourId, palette::text);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, palette::control);
    slider.setColour (juce::Slider::textBoxOutlineColourId, palette::border);
}

void DevControlsComponent::setupCombo (juce::ComboBox& combo)
{
    combo.setJustificationType (juce::Justification::centred);
    combo.setColour (juce::ComboBox::backgroundColourId, palette::control);
    combo.setColour (juce::ComboBox::textColourId, palette::text);
    combo.setColour (juce::ComboBox::outlineColourId, palette::border);
    combo.setColour (juce::ComboBox::focusedOutlineColourId, palette::warning.withAlpha (0.70f));
    combo.setColour (juce::ComboBox::arrowColourId, palette::accentBright);
}

void DevControlsComponent::updateManualReleaseEnabled (bool enabled)
{
    lblSustainRatio_.setEnabled (enabled);
    sldSustainRatio_.setEnabled (enabled);
}

void DevControlsComponent::updateAttackModeControls (int attackModeIdx)
{
    const bool ramp = attackModeIdx == 0;
    lblAttack_.setEnabled (ramp);
    sldAttack_.setEnabled (ramp);
    lblRealAttack_.setEnabled (! ramp);
    sldRealAttack_.setEnabled (! ramp);
}
