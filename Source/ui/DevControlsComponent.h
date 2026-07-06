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
    void updateReleaseEngineEnablement();
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

    juce::GroupComponent groupAttackScaling_ { "AttackScalingGroup", "ATTACK \u00b7 per-band trim (\u00d7 base)" };
    juce::Label lblLowAttackScale_ {};
    juce::Slider sldLowAttackScale_;
    juce::Label lblMidAttackScale_ {};
    juce::Slider sldMidAttackScale_;
    juce::Label lblHighAttackScale_ {};
    juce::Slider sldHighAttackScale_;

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

    juce::GroupComponent groupLookaheadRelease_ { "LookaheadReleaseGroup", "RELEASE \u00b7 Auto (Lookahead)" };
    juce::Label lblLaRelease_ {};
    juce::Slider sldLaRelease_;
    juce::Label lblLaPoles_ {};
    juce::ComboBox cmbLaPoles_ { "DEV LA Poles" };

    juce::GroupComponent groupSmartRelease_ { "SmartReleaseGroup", "RELEASE \u00b7 Smart" };
    juce::Label lblSmartFast_ {};
    juce::Slider sldSmartFast_;
    juce::Label lblSmartSlow_ {};
    juce::Slider sldSmartSlow_;
    juce::Label lblSmartSustain_ {};
    juce::Slider sldSmartSustain_;
    juce::Label lblSmartLeak_ {};
    juce::Slider sldSmartLeak_;

    juce::GroupComponent groupAdaptiveRelease_ { "AdaptiveReleaseGroup", "RELEASE \u00b7 Auto (Adaptive \u00b7 legacy)" };
    juce::Label lblSigmaAttack_ {};
    juce::Slider sldSigmaAttack_;
    juce::Label lblSigmaDecay_ {};
    juce::Slider sldSigmaDecay_;

    juce::GroupComponent groupBandScaling_ { "BandScalingGroup", "RELEASE \u00b7 per-band trim (\u00d7 base)" };
    juce::Label lblLowScale_ {};
    juce::Slider sldLowScale_;
    juce::Label lblMidScale_ {};
    juce::Slider sldMidScale_;
    juce::Label lblHighScale_ {};
    juce::Slider sldHighScale_;
    juce::Label lblWideScale_ {};
    juce::Slider sldWideScale_;

    juce::GroupComponent groupMultiband_ { "MultibandGroup", "BAND \u00b7 Multiband link" };

    juce::GroupComponent groupBandStereo_ { "BandStereoGroup", "BAND \u00b7 Stereo link" };
    juce::Label lblBandStereoLink_ {};
    juce::Slider sldBandStereoLink_;

    juce::GroupComponent groupBandMs_ { "BandMsGroup", "BAND \u00b7 M/S per-band" };
    juce::ToggleButton btnBandMs_ { "Band M/S" };
    juce::Label lblBandMsLink_ {};
    juce::Slider sldBandMsLink_;

    juce::GroupComponent groupPeakControl_ { "PeakControlGroup", "PEAK CONTROL (DEV)" };
    juce::ToggleButton btnMsSafetyClamp_ { "M/S Safety Clamp" };
    juce::Label lblMsClampReadout_ {};
    juce::ToggleButton btnFinalCeiling_ { "Final Ceiling" };
    juce::Label lblFinalCeilingReadout_ {};
    juce::Label lblFcRelease_ {};
    juce::Slider sldFcRelease_;

    juce::GroupComponent groupManualRelease_ { "ManualReleaseGroup", "RELEASE - Manual" };
    juce::Label lblSustainRatio_ {};
    juce::Slider sldSustainRatio_;

    juce::GroupComponent groupMbEngine_ { "MbEngineGroup", "MB Engine (2-band parity)" };
    juce::ToggleButton btnMbEngine_ { "MB Engine" };
    juce::Label lblMbCrossover_ {};
    juce::Slider sldMbCrossover_;
    juce::Label lblMbAttackMode_ {};
    juce::ComboBox cmbMbAttackMode_ { "DEV MB Attack Mode" };
    juce::Label lblMbAttackMs_ {};
    juce::Slider sldMbAttackMs_;
    juce::Label lblMbRelease_ {};
    juce::Slider sldMbRelease_;
    juce::ToggleButton btnMbSafety_ { "MB Safety (TP)" };
    juce::Label lblMbLookahead_ {};
    juce::Slider sldMbLookahead_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attAttack_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attAttackMode_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRealAttack_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLowAttackScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMidAttackScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attHighAttackScale_;
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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSmartFast_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSmartSlow_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSmartSustain_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSmartLeak_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSigmaAttack_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSigmaDecay_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLowScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMidScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attHighScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attWideScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attBandStereoLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attBandMs_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attBandMsLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attMsSafetyClamp_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attFinalCeiling_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attFcRelease_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSustainRatio_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attMbEngine_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMbCrossover_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attMbAttackMode_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMbAttackMs_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMbRelease_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attMbSafety_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMbLookahead_;
    std::unique_ptr<juce::ParameterAttachment> attReleaseAuto_;
    std::unique_ptr<juce::ParameterAttachment> attAttackModeListener_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DevControlsComponent)
};
