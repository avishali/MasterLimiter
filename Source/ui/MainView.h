#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

#include "ui/loudness/LoudnessNumericPanel.h"
#include "ui/meters/GainReductionMeter.h"
#include "ui/meters/MeterGroupComponent.h"

class MasterLimiterAudioProcessor;

namespace mdsp_ui::meters
{
class MeterBallistics;
class PeakHoldModel;
} // namespace mdsp_ui::meters

namespace master_limiter_ui
{
struct ClipBallisticsState;
struct ClipBallisticsDeleter
{
    void operator() (ClipBallisticsState* state) const noexcept;
};

using ClipBallisticsPtr = std::unique_ptr<ClipBallisticsState, ClipBallisticsDeleter>;

ClipBallisticsPtr makeClipBallisticsState();
void resetClipBallistics (ClipBallisticsState& state) noexcept;
void processClipReadout (ClipBallisticsState& state, float clipDb, float dtSec) noexcept;
float processClipLed (ClipBallisticsState& state, bool clipped, float dtSec) noexcept;
float getClipReadoutCurrent (const ClipBallisticsState& state) noexcept;
} // namespace master_limiter_ui

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
    void updateStereoModeControls();
    void updateReleaseAutoControls (bool forceRepaint = false);
    void updateLimiterActiveState();
    void updateBypassButtonState();
    void updateLearnStateDisplay();
    void updateCompensationReadout();
    void setMeterScaleMode (MeterGroupComponent::ScaleMode mode);
    void stepMeterScale (int delta);
    void updateMeterScaleControls();
    void setMeterShowRms (bool shouldShow);
    void paintMeterScaleColumn (juce::Graphics& g);

    class CompensationBar : public juce::Component
    {
    public:
        void setProcessor (MasterLimiterAudioProcessor* processor) noexcept { processor_ = processor; }
        void setColours (juce::Colour negative, juce::Colour positive, juce::Colour track) noexcept;
        void paint (juce::Graphics& g) override;

    private:
        MasterLimiterAudioProcessor* processor_ = nullptr;
        juce::Colour negative_ { 0xffb45a5a };
        juce::Colour positive_ { 0xff56c7b7 };
        juce::Colour track_ { 0x33ffffff };
    };

    mdsp_ui::UiContext& ui_;
    MasterLimiterAudioProcessor& processor_;
    juce::AudioProcessorValueTreeState& apvts_;

    juce::Rectangle<int> meterStripArea_;
    juce::Rectangle<int> headerArea_;
    juce::Rectangle<int> maximizerPanelArea_;
    juce::Rectangle<int> ioPanelArea_;
    juce::Rectangle<int> footerArea_;
    juce::Rectangle<int> gainMatchLabelArea_;
    juce::Rectangle<int> meterScaleColumnArea_;

    juce::Label header_ { {}, "MasterLimiter" };
    juce::Label headerMode_ { {}, "v0.2 - Maximizer" };

    juce::Label lblGainDrive_ { {}, "Gain" };
    juce::Slider sldGainDrive_;
    juce::Label lblGainDriveRange_ { {}, "0 to +24 dB drive" };
    juce::ToggleButton btnGainCeilingLink_ { "Gain / Ceiling Link" };
    juce::ToggleButton btnLimiterActive_ { "Limiter" };

    juce::Label lblClipperDrive_ { {}, "Clipper" };
    juce::ComboBox cmbClipperMode_ { "Clipper Mode" };
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
    juce::ComboBox cmbAutoReleaseMode_ { "Auto Release" };

    juce::Label lblLookahead_ { {}, "Lookahead" };
    juce::Slider sldLookahead_;

    juce::Label lblCeilingMode_ { {}, "SP / TP" };
    juce::ToggleButton btnCeilingMode_ { "SP" };

    juce::Label lblStereoLink_ { {}, "Link" };
    juce::Slider sldStereoLink_;

    juce::ToggleButton btnStereoMode_ { "Stereo" };

    juce::Label lblBandColor_ { {}, "Color" };
    juce::Slider sldBandColor_;

    juce::Label lblCharacter_ { {}, "Character" };
    juce::Slider sldCharacter_;

    juce::ToggleButton btnGainMatchAutoTrack_ { "Auto / Track" };
    juce::Label lblGainMatchNote_ { {}, "+0.0 dB" };
    CompensationBar compGainBar_;

    juce::TextButton btnLearnInputGain_ { "Learn" };
    juce::Label lblLearnInputLufs_ { {}, "No ref" };
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
    juce::TextButton btnMeterScaleMinus_ { "-" };
    juce::TextButton btnMeterScalePlus_ { "+" };
    juce::TextButton btnMeterRms_ { "RMS" };
    juce::Label lblMeterScaleRange_ { {}, "Full" };

    juce::TextButton btnBypass_ { "Bypass" };
    juce::TextButton btnResetPeaks_ { "Reset peaks" };

    TpReadoutSmoother tpSmoother_ {};
    bool adjustingIoFaders_ { false };
    bool lastLimiterActive_ { true };
    bool lastReleaseAuto_ { false };
    master_limiter_ui::ClipBallisticsPtr clipBallistics_;
    MeterGroupComponent::ScaleMode currentMeterScale_ { MeterGroupComponent::ScaleMode::FullRange };
    float clipLedLevel_ { 0.0f };
    int lastStereoModeIdx_ { -1 };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attGainDrive_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attLimiterActive_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attPluginBypass_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attClipperDrive_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attClipperMode_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attCeiling_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attGainCeilingLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRelease_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attReleaseSustain_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attReleaseAuto_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attAutoReleaseMode_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLookahead_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attBandColor_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attCharacter_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attGainMatchAutoTrack_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoInputTrimL_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoInputTrimR_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attIoInputLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoOutputTrimL_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoOutputTrimR_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attIoOutputLink_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
