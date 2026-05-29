#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

#include "ui/loudness/LoudnessNumericPanel.h"
#include "ui/meters/GainReductionMeter.h"
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
    void mouseDown (const juce::MouseEvent& e) override;

    void syncMetersFromProcessor();
    void repaintMeterStrip();
    void resetPeakHolds();

private:
    static constexpr const char* kPlaceholderTooltip = "Placeholder - wired in a later slice.";

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
    void styleIoTrimFader (juce::Slider& s) const;
    void styleHorizontalPlaceholder (juce::Slider& s) const;
    void syncLinkedFaders (juce::Slider& source, juce::Slider& target, juce::ToggleButton& link);
    void updateIoTrimReadouts();
    void updateCeilingModeButton (int ceilingIdx);
    void updateLimiterActiveState();

    mdsp_ui::UiContext& ui_;
    MasterLimiterAudioProcessor& processor_;
    juce::AudioProcessorValueTreeState& apvts_;

    juce::Rectangle<int> meterStripArea_;
    juce::Rectangle<int> headerArea_;
    juce::Rectangle<int> maximizerPanelArea_;
    juce::Rectangle<int> ioPanelArea_;
    juce::Rectangle<int> footerArea_;
    juce::Rectangle<int> gainMatchLabelArea_;

    juce::Label header_ { {}, "MasterLimiter" };
    juce::Label headerMode_ { {}, "v0.2 - Maximizer" };

    juce::Label lblGainDrive_ { {}, "Gain" };
    juce::Slider sldGainDrive_;
    juce::Label lblGainDriveRange_ { {}, "0 to +24 dB drive" };
    juce::ToggleButton btnGainCeilingLink_ { "Gain / Ceiling Link" };
    juce::ToggleButton btnLimiterActive_ { "Limiter" };

    juce::Label lblClipperDrive_ { {}, "Clipper" };
    juce::Slider sldClipperDrive_;
    juce::Label lblClipperReadout_ { {}, "Clip 0.0 / 0.0" };

    juce::Label lblCeiling_ { {}, "Output Level" };
    juce::Slider sldCeiling_;

    juce::Label lblRelease_ { {}, "Release" };
    juce::Slider sldRelease_;

    juce::Label lblReleaseSustain_ { {}, "Sustain" };
    juce::Slider sldReleaseSustain_;

    juce::Label lblReleaseAuto_ { {}, "Auto Rel" };
    juce::ToggleButton btnReleaseAuto_ { "Auto Rel" };

    juce::Label lblLookahead_ { {}, "Lookahead" };
    juce::Slider sldLookahead_;

    juce::Label lblCeilingMode_ { {}, "SP / TP" };
    juce::ToggleButton btnCeilingMode_ { "SP" };

    juce::Label lblStereoLink_ { {}, "Link" };
    juce::Slider sldStereoLink_;

    juce::Label lblMsLink_ { {}, "M/S Lk" };
    juce::Slider sldMsLink_;

    juce::Label lblCharacter_ { {}, "Character" };
    juce::Slider sldCharacter_;

    juce::ToggleButton btnGainMatchAutoTrack_ { "Auto / Track" };
    juce::Label lblGainMatchNote_ { {}, "measured LUFS match" };

    juce::TextButton btnLearnInputGain_ { "Learn" };
    juce::Label lblLearnInputLufs_ { {}, "-11.0 LUFS" };
    juce::Label lblIoInputTrim_ { {}, "Input" };
    juce::Slider sldIoInputTrimL_;
    juce::Slider sldIoInputTrimR_;
    juce::ToggleButton btnIoInputLink_ { "L/R Link" };
    juce::Label lblIoInputReadout_ { {}, "0.0 dB" };
    juce::Label lblIoOutputTrim_ { {}, "Output" };
    juce::Slider sldIoOutputTrimL_;
    juce::Slider sldIoOutputTrimR_;
    juce::ToggleButton btnIoOutputLink_ { "L/R Link" };
    juce::Label lblIoOutputReadout_ { {}, "0.0 dB" };

    MeterGroupComponent meterIn_;
    GainReductionMeter meterGr_;
    MeterGroupComponent meterOut_;
    LoudnessNumericPanel lufsPanel_;
    juce::Label lblTruePeak_ {};

    juce::TextButton btnBypass_ { "Bypass" };
    juce::TextButton btnResetPeaks_ { "Reset peaks" };

    TpReadoutSmoother tpSmoother_ {};
    bool adjustingIoFaders_ { false };
    bool lastLimiterActive_ { true };
    float clipLedLevel_ { 0.0f };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attGainDrive_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attLimiterActive_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attClipperDrive_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attCeiling_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attGainCeilingLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRelease_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attReleaseSustain_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attReleaseAuto_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLookahead_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attStereoLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMsLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attCharacter_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoInputTrimL_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoInputTrimR_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attIoInputLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoOutputTrimL_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoOutputTrimR_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attIoOutputLink_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
