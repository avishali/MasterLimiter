#include "ui/DevControlsComponent.h"

#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"

#include <cmath>

#ifndef MASTERLIMITER_GIT_SHA
 #define MASTERLIMITER_GIT_SHA "nogit"
#endif

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
    : processor_ (processor),
      ui_ (uiContext),
      apvts_ (processor.getAPVTS())
{
    header_.setJustificationType (juce::Justification::centredLeft);
    header_.setFont (ui_.type().labelFont().withHeight (13.0f).boldened());
    header_.setColour (juce::Label::textColourId, palette::warning.withAlpha (0.95f));
    header_.setText ("DEV - tuning controls (temporary; baked & removed for 0.4)  ["
                     + juce::String (MASTERLIMITER_GIT_SHA) + "]",
                     juce::dontSendNotification);
    addAndMakeVisible (header_);

    viewport_.setViewedComponent (&content_, false);
    viewport_.setScrollBarsShown (true, false);
    viewport_.setScrollBarThickness (10);
    addAndMakeVisible (viewport_);

    for (auto* group : { &groupAttack_,
                         &groupLookahead_,
                         &groupCrossover_,
                         &groupReleaseEngine_,
                         &groupLookaheadRelease_,
                         &groupAdaptiveRelease_,
                         &groupBandScaling_,
                         &groupMultiband_,
                         &groupBandStereo_,
                         &groupBandMs_,
                         &groupPeakControl_,
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

    setupLabel (lblXoverCutoff_, "Lo/Mid Cut");
    setupSlider (sldXoverCutoff_, 0, " Hz");
    sldXoverCutoff_.setTooltip ("Stage-1 linear-phase split (Low vs Mid+High).");

    setupLabel (lblXoverTransition_, "Lo/Mid Trans");
    setupSlider (sldXoverTransition_, 0, " Hz");
    sldXoverTransition_.setTooltip ("Stage-1 transition width; wider = gentler split / shorter kernel.");

    setupLabel (lblXoverAtten_, "Lo/Mid Atten");
    setupSlider (sldXoverAtten_, 0, " dB");
    sldXoverAtten_.setTooltip ("Stage-1 stop-band attenuation; lower = shorter kernel.");

    setupLabel (lblXoverHiCutoff_, "Mid/Hi Cut");
    setupSlider (sldXoverHiCutoff_, 0, " Hz");
    sldXoverHiCutoff_.setTooltip ("Stage-2 linear-phase split (Mid vs High).");

    setupLabel (lblXoverHiTransition_, "Mid/Hi Trans");
    setupSlider (sldXoverHiTransition_, 0, " Hz");
    sldXoverHiTransition_.setTooltip ("Stage-2 transition width.");

    setupLabel (lblXoverHiAtten_, "Mid/Hi Atten");
    setupSlider (sldXoverHiAtten_, 0, " dB");
    sldXoverHiAtten_.setTooltip ("Stage-2 stop-band attenuation.");

    setupLabel (lblBandLink_, "Band Split");
    setupSlider (sldBandLink_, 0, " %");
    sldBandLink_.setTooltip ("Multiband band-to-band link. 0 = bands glued (single shared GR), 100 = fully independent 3-band. (Main Color knob is greyed; this is the live control.)");

    setupLabel (lblReleaseEngine_, "Auto Engine");
    setupCombo (cmbReleaseEngine_);
    cmbReleaseEngine_.addItem ("Adaptive", 1);
    cmbReleaseEngine_.addItem ("Lookahead", 2);
    cmbReleaseEngine_.setTooltip ("Auto-release algorithm. Lookahead = recovers only in real gaps seen in the lookahead window (smooth, program-dependent, current best). Adaptive = legacy sigma tracker (A/B only).");

    setupLabel (lblLaRelease_, "Release (ms)");
    setupSlider (sldLaRelease_, 1, " ms");
    sldLaRelease_.setTooltip ("Lookahead engine: recovery time (how fast gain lets go in a gap). Per-band \u00d7 trims multiply this.");

    setupLabel (lblLaPoles_, "Smoothness");
    setupCombo (cmbLaPoles_);
    cmbLaPoles_.addItem ("2", 1);
    cmbLaPoles_.addItem ("3", 2);
    cmbLaPoles_.addItem ("4", 3);
    cmbLaPoles_.setTooltip ("Lookahead engine: recovery-curve order (2\u20134). More = rounder S-curve, same speed.");

    setupLabel (lblSigmaAttack_, "Adapt Onset (ms)");
    setupSlider (sldSigmaAttack_, 1, " ms");
    sldSigmaAttack_.setTooltip ("Adaptive (legacy): how fast it decides limiting is sustained \u2192 switches to slow release. Lower = reacts sooner.");

    setupLabel (lblSigmaDecay_, "Adapt Hold \u00d7");
    setupSlider (sldSigmaDecay_, 2, {});
    sldSigmaDecay_.setTooltip ("Adaptive (legacy): how long it stays in slow-release after limiting stops.");

    setupLabel (lblLowScale_, "Low \u00d7");
    setupSlider (sldLowScale_, 2, {});
    sldLowScale_.setTooltip ("Low-band release = this \u00d7 the base release. >1 slower (bass, less pump), <1 faster. Affects Auto + Manual.");

    setupLabel (lblMidScale_, "Mid \u00d7");
    setupSlider (sldMidScale_, 2, {});
    sldMidScale_.setTooltip ("Mid-band release trim (\u00d7 the base release).");

    setupLabel (lblHighScale_, "High \u00d7");
    setupSlider (sldHighScale_, 2, {});
    sldHighScale_.setTooltip ("High-band release trim (\u00d7 the base release).");

    setupLabel (lblWideScale_, "Wide \u00d7");
    setupSlider (sldWideScale_, 2, {});
    sldWideScale_.setTooltip ("Wideband final-stage release trim (\u00d7 the base release).");

    setupLabel (lblBandStereoLink_, "Band Stereo");
    setupSlider (sldBandStereoLink_, 0, " %");
    sldBandStereoLink_.setTooltip ("Per-band L/R stereo link. 0 = independent L/R per band, 100 = mono-linked GR per band.");

    btnBandMs_.setClickingTogglesState (true);
    btnBandMs_.setTooltip ("M/S mode only: encode each band to Mid/Side, limit independently, decode back. Wideband M/S stage unchanged.");

    setupLabel (lblBandMsLink_, "M/S Link");
    setupSlider (sldBandMsLink_, 0, " %");
    sldBandMsLink_.setTooltip ("Per-band Mid/Side link when Band M/S is on. 0 = independent M/S per band, 100 = linked max(|M|,|S|) per band.");

    btnMsSafetyClamp_.setClickingTogglesState (true);
    btnMsSafetyClamp_.setTooltip ("M/S decoded-L/R safety clamp. Off = skip clamp (FinalCeiling still ceiling-safe).");
    lblMsClampReadout_.setJustificationType (juce::Justification::centredRight);
    lblMsClampReadout_.setFont (ui_.type().labelFont().withHeight (11.0f));
    lblMsClampReadout_.setColour (juce::Label::textColourId, palette::textMuted);

    btnFinalCeiling_.setClickingTogglesState (true);
    btnFinalCeiling_.setTooltip ("OFF lets peaks exceed the ceiling - audition only.");
    lblFinalCeilingReadout_.setJustificationType (juce::Justification::centredRight);
    lblFinalCeilingReadout_.setFont (ui_.type().labelFont().withHeight (11.0f));
    lblFinalCeilingReadout_.setColour (juce::Label::textColourId, palette::textMuted);

    setupLabel (lblSustainRatio_, "Manual Sustain");
    setupSlider (sldSustainRatio_, 2, {});
    sldSustainRatio_.setTooltip ("Manual release only (Auto OFF): fast+slow split. Higher = more sustain held.");

    for (auto* label : { &lblAttackMode_, &lblAttack_, &lblRealAttack_, &lblLookaheadBand_, &lblLookaheadWide_,
                         &lblXoverCutoff_, &lblXoverTransition_, &lblXoverAtten_,
                         &lblXoverHiCutoff_, &lblXoverHiTransition_, &lblXoverHiAtten_, &lblBandLink_,
                         &lblReleaseEngine_, &lblLaRelease_, &lblLaPoles_,
                         &lblSigmaAttack_, &lblSigmaDecay_, &lblLowScale_, &lblMidScale_,
                         &lblHighScale_, &lblWideScale_, &lblBandStereoLink_, &lblBandMsLink_,
                         &lblMsClampReadout_,
                         &lblFinalCeilingReadout_, &lblSustainRatio_ })
    {
        content_.addAndMakeVisible (*label);
    }

    for (auto* slider : { &sldAttack_, &sldRealAttack_, &sldLookaheadBand_, &sldLookaheadWide_,
                          &sldXoverCutoff_, &sldXoverTransition_, &sldXoverAtten_,
                          &sldXoverHiCutoff_, &sldXoverHiTransition_, &sldXoverHiAtten_, &sldBandLink_,
                          &sldLaRelease_, &sldSigmaAttack_, &sldSigmaDecay_,
                          &sldLowScale_, &sldMidScale_, &sldHighScale_, &sldBandStereoLink_, &sldBandMsLink_, &sldSustainRatio_ })
    {
        content_.addAndMakeVisible (*slider);
    }

    content_.addAndMakeVisible (cmbAttackMode_);
    content_.addAndMakeVisible (cmbReleaseEngine_);
    content_.addAndMakeVisible (cmbLaPoles_);
    content_.addAndMakeVisible (btnMsSafetyClamp_);
    content_.addAndMakeVisible (btnFinalCeiling_);
    content_.addAndMakeVisible (btnBandMs_);

    attAttack_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_attack_ms), sldAttack_);
    attAttackMode_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::dev_attack_mode), cmbAttackMode_);
    attRealAttack_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_real_attack_ms), sldRealAttack_);
    attLookaheadBand_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_lookahead_band_ms), sldLookaheadBand_);
    attLookaheadWide_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_lookahead_wide_ms), sldLookaheadWide_);
    attXoverCutoff_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_xover_cutoff_hz), sldXoverCutoff_);
    attXoverTransition_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_xover_transition_hz), sldXoverTransition_);
    attXoverAtten_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_xover_atten_db), sldXoverAtten_);
    attXoverHiCutoff_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_xover_hi_cutoff_hz), sldXoverHiCutoff_);
    attXoverHiTransition_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_xover_hi_transition_hz), sldXoverHiTransition_);
    attXoverHiAtten_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_xover_hi_atten_db), sldXoverHiAtten_);
    attBandLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::band_color), sldBandLink_);
    attReleaseEngine_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::dev_release_engine), cmbReleaseEngine_);
    attLaRelease_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_la_release_ms), sldLaRelease_);
    attLaPoles_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::dev_la_release_poles), cmbLaPoles_);
    attSigmaAttack_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_sigma_attack_ms), sldSigmaAttack_);
    attSigmaDecay_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_sigma_decay_scale), sldSigmaDecay_);
    attLowScale_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_low_band_release_scale), sldLowScale_);
    attMidScale_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_mid_band_release_scale), sldMidScale_);
    attHighScale_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_high_band_release_scale), sldHighScale_);
    attWideScale_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_wide_release_scale), sldWideScale_);
    attBandStereoLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_band_stereo_link_pct), sldBandStereoLink_);
    attBandMs_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::dev_band_ms), btnBandMs_);
    attBandMsLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::dev_band_ms_link_pct), sldBandMsLink_);
    attMsSafetyClamp_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::dev_ms_safety_clamp), btnMsSafetyClamp_);
    attFinalCeiling_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::dev_final_ceiling), btnFinalCeiling_);
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

    cmbReleaseEngine_.onChange = [this]
    {
        updateReleaseEngineEnablement();
    };
    updateReleaseEngineEnablement();
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
        for (auto* slider : { &sldAttack_, &sldLookaheadBand_, &sldLookaheadWide_,
                              &sldXoverCutoff_, &sldXoverTransition_, &sldXoverAtten_,
                              &sldXoverHiCutoff_, &sldXoverHiTransition_, &sldXoverHiAtten_, &sldBandLink_,
                              &sldLaRelease_,
                          &sldSigmaAttack_, &sldSigmaDecay_, &sldLowScale_, &sldMidScale_, &sldHighScale_,
                          &sldWideScale_, &sldBandStereoLink_, &sldBandMsLink_, &sldSustainRatio_ })
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

    inner = placeGroup (groupCrossover_, 248);
    placeSliderRow (inner.removeFromTop (rowH), lblXoverCutoff_, sldXoverCutoff_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblXoverTransition_, sldXoverTransition_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblXoverAtten_, sldXoverAtten_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblXoverHiCutoff_, sldXoverHiCutoff_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblXoverHiTransition_, sldXoverHiTransition_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblXoverHiAtten_, sldXoverHiAtten_);

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

    inner = placeGroup (groupBandScaling_, 172);
    placeSliderRow (inner.removeFromTop (rowH), lblLowScale_, sldLowScale_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblMidScale_, sldMidScale_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblHighScale_, sldHighScale_);
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblWideScale_, sldWideScale_);

    inner = placeGroup (groupMultiband_, 72);
    placeSliderRow (inner.removeFromTop (rowH), lblBandLink_, sldBandLink_);

    inner = placeGroup (groupBandStereo_, 72);
    placeSliderRow (inner.removeFromTop (rowH), lblBandStereoLink_, sldBandStereoLink_);

    inner = placeGroup (groupBandMs_, 104);
    {
        auto row = inner.removeFromTop (rowH);
        btnBandMs_.setBounds (row.removeFromLeft (juce::jmax (120, row.getWidth() - 8)));
    }
    inner.removeFromTop (8);
    placeSliderRow (inner.removeFromTop (rowH), lblBandMsLink_, sldBandMsLink_);

    inner = placeGroup (groupPeakControl_, 104);
    {
        auto row = inner.removeFromTop (rowH);
        btnMsSafetyClamp_.setBounds (row.removeFromLeft (juce::jmax (120, row.getWidth() - 88)));
        row.removeFromLeft (8);
        lblMsClampReadout_.setBounds (row);
    }
    inner.removeFromTop (8);
    {
        auto row = inner.removeFromTop (rowH);
        btnFinalCeiling_.setBounds (row.removeFromLeft (juce::jmax (120, row.getWidth() - 88)));
        row.removeFromLeft (8);
        lblFinalCeilingReadout_.setBounds (row);
    }

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

juce::String DevControlsComponent::formatClampReadout (float currentDb, float maxDb)
{
    if (! std::isfinite (currentDb))
        currentDb = 0.0f;
    if (! std::isfinite (maxDb))
        maxDb = 0.0f;

    return juce::String (juce::jmax (0.0f, currentDb), 1) + " / " + juce::String (juce::jmax (0.0f, maxDb), 1) + " dB";
}

void DevControlsComponent::updateReleaseEngineEnablement()
{
    const int engineIdx = [&]
    {
        if (auto* raw = apvts_.getRawParameterValue (pid (param::dev_release_engine)))
            return (int) raw->load (std::memory_order_relaxed);

        return cmbReleaseEngine_.getSelectedItemIndex();
    }();
    const bool lookahead = engineIdx == 1;

    lblLaRelease_.setEnabled (lookahead);
    sldLaRelease_.setEnabled (lookahead);
    lblLaPoles_.setEnabled (lookahead);
    cmbLaPoles_.setEnabled (lookahead);

    lblSigmaAttack_.setEnabled (! lookahead);
    sldSigmaAttack_.setEnabled (! lookahead);
    lblSigmaDecay_.setEnabled (! lookahead);
    sldSigmaDecay_.setEnabled (! lookahead);
}

void DevControlsComponent::syncReadouts()
{
    updateReleaseEngineEnablement();

    lblMsClampReadout_.setText (formatClampReadout (processor_.getCurrentMsClampDb(), processor_.getMaxMsClampDb()),
                                juce::dontSendNotification);
    lblFinalCeilingReadout_.setText (formatClampReadout (processor_.getCurrentFinalCeilingDb(), processor_.getMaxFinalCeilingDb()),
                                     juce::dontSendNotification);
}
