#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

class MasterLimiterAudioProcessor;

//==============================================================================
class DevControlsComponent : public juce::Component
{
public:
    DevControlsComponent (MasterLimiterAudioProcessor& processor, mdsp_ui::UiContext& uiContext);
    ~DevControlsComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void syncReadouts();

private:
    void setupGroup (juce::GroupComponent& group);
    void setupLabel (juce::Label& label, const juce::String& text);
    void setupSlider (juce::Slider& slider, int decimals, const juce::String& suffix);
    void setupCombo (juce::ComboBox& combo);
    void updateManualReleaseEnabled (bool enabled);
    void updateAttackModeControls (int attackModeIdx);
    static juce::String formatClampReadout (float currentDb, float maxDb);

    MasterLimiterAudioProcessor& processor_;
    mdsp_ui::UiContext& ui_;
    juce::AudioProcessorValueTreeState& apvts_;

    juce::Label header_ { {}, "DEV - tuning controls (temporary; baked & removed for 0.4)" };
    juce::Component content_;
    juce::Viewport viewport_ { "DEV Controls Viewport" };

    juce::GroupComponent groupAttack_ { "AttackGroup", "ATTACK" };
    juce::Label lblAttackMode_ {};
    juce::ComboBox cmbAttackMode_ { "DEV Attack Mode" };
    juce::Label lblAttack_ {};
    juce::Slider sldAttack_;
    juce::Label lblRealAttack_ {};
    juce::Slider sldRealAttack_;

    juce::GroupComponent groupLookahead_ { "LookaheadGroup", "LOOKAHEAD" };
    juce::Label lblLookaheadBand_ {};
    juce::Slider sldLookaheadBand_;
    juce::Label lblLookaheadWide_ {};
    juce::Slider sldLookaheadWide_;

    juce::GroupComponent groupCrossover_ { "CrossoverGroup", "CROSSOVER (linear-phase)" };
    juce::Label lblXoverCutoff_ {};
    juce::Slider sldXoverCutoff_;
    juce::Label lblXoverTransition_ {};
    juce::Slider sldXoverTransition_;
    juce::Label lblXoverAtten_ {};
    juce::Slider sldXoverAtten_;
    juce::Label lblXoverHiCutoff_ {};
    juce::Slider sldXoverHiCutoff_;
    juce::Label lblXoverHiTransition_ {};
    juce::Slider sldXoverHiTransition_;
    juce::Label lblXoverHiAtten_ {};
    juce::Slider sldXoverHiAtten_;
    juce::Label lblBandLink_ {};
    juce::Slider sldBandLink_;

    juce::GroupComponent groupReleaseEngine_ { "ReleaseEngineGroup", "RELEASE - Engine" };
    juce::Label lblReleaseEngine_ {};
    juce::ComboBox cmbReleaseEngine_ { "DEV Release Engine" };

    juce::GroupComponent groupLookaheadRelease_ { "LookaheadReleaseGroup", "RELEASE - Lookahead engine" };
    juce::Label lblLaRelease_ {};
    juce::Slider sldLaRelease_;
    juce::Label lblLaPoles_ {};
    juce::ComboBox cmbLaPoles_ { "DEV LA Poles" };

    juce::GroupComponent groupAdaptiveRelease_ { "AdaptiveReleaseGroup", "RELEASE - Adaptive engine" };
    juce::Label lblSigmaAttack_ {};
    juce::Slider sldSigmaAttack_;
    juce::Label lblSigmaDecay_ {};
    juce::Slider sldSigmaDecay_;

    juce::GroupComponent groupBandScaling_ { "BandScalingGroup", "RELEASE - Band scaling" };
    juce::Label lblLowScale_ {};
    juce::Slider sldLowScale_;
    juce::Label lblMidScale_ {};
    juce::Slider sldMidScale_;
    juce::Label lblHighScale_ {};
    juce::Slider sldHighScale_;

    juce::GroupComponent groupBandStereo_ { "BandStereoGroup", "BAND - Stereo link" };
    juce::Label lblBandStereoLink_ {};
    juce::Slider sldBandStereoLink_;

    juce::GroupComponent groupPeakControl_ { "PeakControlGroup", "PEAK CONTROL (DEV)" };
    juce::ToggleButton btnMsSafetyClamp_ { "M/S Safety Clamp" };
    juce::Label lblMsClampReadout_ {};
    juce::ToggleButton btnFinalCeiling_ { "Final Ceiling" };
    juce::Label lblFinalCeilingReadout_ {};

    juce::GroupComponent groupManualRelease_ { "ManualReleaseGroup", "RELEASE - Manual" };
    juce::Label lblSustainRatio_ {};
    juce::Slider sldSustainRatio_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attAttack_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attAttackMode_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRealAttack_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLookaheadBand_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLookaheadWide_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attXoverCutoff_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attXoverTransition_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attXoverAtten_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attXoverHiCutoff_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attXoverHiTransition_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attXoverHiAtten_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attBandLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attReleaseEngine_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLaRelease_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attLaPoles_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSigmaAttack_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSigmaDecay_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLowScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMidScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attHighScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attBandStereoLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attMsSafetyClamp_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attFinalCeiling_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSustainRatio_;
    std::unique_ptr<juce::ParameterAttachment> attReleaseAuto_;
    std::unique_ptr<juce::ParameterAttachment> attAttackModeListener_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DevControlsComponent)
};
