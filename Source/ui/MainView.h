#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

#include "ui/loudness/LoudnessNumericPanel.h"
#include "ui/meters/MeterGroupComponent.h"

class MasterLimiterAudioProcessor;

//==============================================================================
class MainView : public juce::Component
{
public:
    MainView (mdsp_ui::UiContext& uiContext, MasterLimiterAudioProcessor& processor);
    ~MainView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void syncMetersFromProcessor();
    void repaintMeterStrip();
    void resetPeakHolds();

private:
    struct TpReadoutSmoother
    {
        float held { -200.0f };
        float duty { -200.0f };
        int holdTicksLeft { 0 };

        void reset() noexcept;
        void tick (float raw, float dtSec, int holdTicksAt30Hz) noexcept;
        static juce::String formatDb (float v) noexcept;
    };

    void styleRotary (juce::Slider& s) const;

    mdsp_ui::UiContext& ui_;
    MasterLimiterAudioProcessor& processor_;
    juce::AudioProcessorValueTreeState& apvts_;

    juce::Rectangle<int> meterStripArea_;
    juce::Rectangle<int> vSeparatorBounds_;

    juce::Label header_ { {}, "MasterLimiter v0.1.0" };

    juce::Label lblInputGain_ { {}, "Input Gain (dB)" };
    juce::Slider sldInputGain_;

    juce::Label lblCeiling_ { {}, "Ceiling (dB)" };
    juce::Slider sldCeiling_;

    juce::Label lblRelease_ { {}, "Release (ms)" };
    juce::Slider sldRelease_;

    juce::Label lblReleaseSustain_ { {}, "Sustain Ratio" };
    juce::Slider sldReleaseSustain_;

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

    MeterGroupComponent meterIn_;
    MeterGroupComponent meterGr_;
    MeterGroupComponent meterOut_;
    LoudnessNumericPanel lufsPanel_;
    juce::Label lblTruePeak_ {};

    juce::TextButton btnResetPeaks_ { "Reset peaks" };

    TpReadoutSmoother tpSmoother_ {};

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attInputGain_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attCeiling_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRelease_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attReleaseSustain_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attReleaseAuto_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLookahead_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attCeilingMode_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attStereoLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMsLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attCharacter_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
