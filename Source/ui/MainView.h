#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

//==============================================================================
class MainView : public juce::Component
{
public:
    MainView (mdsp_ui::UiContext& uiContext, juce::AudioProcessorValueTreeState& state);
    ~MainView() override = default;

    void resized() override;

private:
    mdsp_ui::UiContext& ui_;
    juce::AudioProcessorValueTreeState& apvts_;

    juce::Label header_ { {}, "MasterLimiter v0.1.0" };

    juce::Label lblInputGain_ { {}, "Input Gain (dB)" };
    juce::Slider sldInputGain_;

    juce::Label lblCeiling_ { {}, "Ceiling (dB)" };
    juce::Slider sldCeiling_;

    juce::Label lblRelease_ { {}, "Release (ms)" };
    juce::Slider sldRelease_;

    juce::Label lblReleaseAuto_ { {}, "Release Auto" };
    juce::ToggleButton btnReleaseAuto_ { "Off / Auto" };

    juce::Label lblLookahead_ { {}, "Lookahead (ms)" };
    juce::Slider sldLookahead_;

    juce::Label lblCeilingMode_ { {}, "Ceiling Mode" };
    juce::ComboBox cmbCeilingMode_;

    juce::Label lblStereoLink_ { {}, "Stereo Link (%)" };
    juce::Slider sldStereoLink_;

    juce::Label lblMsLink_ { {}, "M/S Link (%)" };
    juce::Slider sldMsLink_;

    juce::Label lblCharacter_ { {}, "Character" };
    juce::ComboBox cmbCharacter_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attInputGain_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attCeiling_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRelease_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attReleaseAuto_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLookahead_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attCeilingMode_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attStereoLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMsLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attCharacter_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
