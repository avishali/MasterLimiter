#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

#include "ui/loudness/LoudnessNumericPanel.h"
#include "ui/meters/ClipBallistics.h"
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

    class ValueSlider : public juce::Slider
    {
    public:
        enum class ValueLabelMode
        {
            Centre,
            Below,
            Hidden
        };

        void setValueLabelMode (ValueLabelMode mode) noexcept { valueLabelMode_ = mode; }
        juce::Rectangle<int> getValueLabelBounds() const;
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;

        std::function<void (ValueSlider&)> onValueEditRequest;

    private:
        ValueLabelMode valueLabelMode_ { ValueLabelMode::Centre };
    };

    class SegmentedChoice : public juce::Component,
                            public juce::SettableTooltipClient
    {
    public:
        void setOptions (juce::StringArray options);
        void setSelectedIndex (int index, juce::NotificationType notification);
        int getSelectedIndex() const noexcept { return selectedIndex_; }
        void setUiContext (mdsp_ui::UiContext* ui) noexcept { ui_ = ui; }

        std::function<void (int)> onChange;

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;

    private:
        int indexAt (juce::Point<int> p) const noexcept;

        mdsp_ui::UiContext* ui_ = nullptr;
        juce::StringArray options_;
        int selectedIndex_ { 0 };
    };

    void styleRotary (ValueSlider& s) const;
    void styleDevTuningSlider (ValueSlider& s) const;
    void styleIoTrimFader (ValueSlider& s) const;
    void setupValueEdit (ValueSlider& s, ValueSlider::ValueLabelMode mode);
    void beginValueEdit (ValueSlider& s);
    void positionValueEditor();
    void finishValueEdit (bool commit);
    void syncLinkedFaders (juce::Slider& source, juce::Slider& target, juce::ToggleButton& link);
    void updateIoTrimReadouts();
    void updateCeilingModeButton (int ceilingIdx);
    void updateStereoModeControls();
    void updateClipperModeButton (int clipperIdx);
    void updateClipperActiveState();
    void updateCharacterModeControl (int characterIdx);
    void updateAutoReleaseModeControl (int modeIdx);
    void updateDevReleaseEngineControl (int engineIdx);
    void updateDevLaReleasePolesControl (int polesIdx);
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
    juce::TooltipWindow tooltipWindow_;

    juce::Rectangle<int> meterStripArea_;
    juce::Rectangle<int> headerArea_;
    juce::Rectangle<int> maximizerPanelArea_;
    juce::Rectangle<int> ioPanelArea_;
    juce::Rectangle<int> footerArea_;
    juce::Rectangle<int> devReleasePanelArea_;
    juce::Rectangle<int> gainMatchLabelArea_;
    juce::Rectangle<int> meterScaleColumnArea_;

    juce::Label header_ { {}, "MasterLimiter" };
    juce::Label headerMode_ { {}, "v0.3.0 (beta) - Maximizer" };
    juce::ComboBox presetMenu_ { "Factory Presets" };

    juce::Label lblGainDrive_ { {}, "Gain" };
    ValueSlider sldGainDrive_;
    juce::Label lblGainDriveRange_ { {}, "0 to +24 dB drive" };
    juce::ToggleButton btnGainCeilingLink_ { "Link" };
    juce::ToggleButton btnLimiterActive_ { "Limiter" };

    juce::Label lblClipperDrive_ { {}, "Clipper" };
    juce::ToggleButton btnClipperActive_ { "Clipper" };
    juce::ToggleButton btnClipperMode_ { "Hard" };
    ValueSlider sldClipperDrive_;
    juce::Label lblClipperReadout_ { {}, "Clip 0.0 / 0.0" };

    juce::Label lblCeiling_ { {}, "Output Level" };
    ValueSlider sldCeiling_;

    juce::Label lblRelease_ { {}, "Release" };
    ValueSlider sldRelease_;

    juce::Label lblReleaseAuto_ { {}, "Auto" };
    juce::ToggleButton btnReleaseAuto_ { "Auto" };
    SegmentedChoice segAutoReleaseMode_;

    juce::Label lblCeilingMode_ { {}, "SP / TP" };
    juce::ToggleButton btnCeilingMode_ { "SP" };

    juce::Label lblStereoLink_ { {}, "Link" };
    ValueSlider sldStereoLink_;

    juce::ToggleButton btnStereoMode_ { "Stereo" };

    juce::Label lblBandColor_ { {}, "Color" };
    ValueSlider sldBandColor_;

    juce::Label lblCharacter_ { {}, "Character" };
    SegmentedChoice segCharacter_;

    juce::Label lblDevReleaseTuning_ { {}, "DEV RELEASE - TEMP" };
    juce::Label lblDevReleaseEngine_ { {}, "Engine" };
    SegmentedChoice segDevReleaseEngine_;
    juce::Label lblDevLaReleaseMs_ { {}, "LA Release" };
    ValueSlider sldDevLaReleaseMs_;
    juce::Label lblDevLaReleasePoles_ { {}, "Poles" };
    SegmentedChoice segDevLaReleasePoles_;
    juce::Label lblDevLowBandReleaseScale_ { {}, "Low x" };
    ValueSlider sldDevLowBandReleaseScale_;
    juce::Label lblDevHighBandReleaseScale_ { {}, "High/Wide x" };
    ValueSlider sldDevHighBandReleaseScale_;
    juce::Label lblDevSigmaAttackMs_ { {}, "Sigma Atk" };
    ValueSlider sldDevSigmaAttackMs_;
    juce::Label lblDevSigmaDecayScale_ { {}, "Sigma Decay x" };
    ValueSlider sldDevSigmaDecayScale_;

    juce::ToggleButton btnGainMatchAutoTrack_ { "Auto / Track" };
    juce::Label lblGainMatchNote_ { {}, "+0.0 dB" };
    CompensationBar compGainBar_;

    juce::TextButton btnLearnInputGain_ { "Learn" };
    juce::Label lblLearnInputLufs_ { {}, "No ref" };
    juce::Label lblIoInputTrim_ { {}, "Input" };
    ValueSlider sldIoInputTrimL_;
    ValueSlider sldIoInputTrimR_;
    juce::ToggleButton btnIoInputLink_ { "L/R Link" };
    juce::Label lblIoInputReadout_ { {}, "0.0 dB" };
    juce::Label lblIoInputReadoutL_ { {}, "0.0" };
    juce::Label lblIoInputReadoutR_ { {}, "0.0" };
    juce::Label lblIoOutputTrim_ { {}, "Output" };
    ValueSlider sldIoOutputTrimL_;
    ValueSlider sldIoOutputTrimR_;
    juce::ToggleButton btnIoOutputLink_ { "L/R Link" };
    juce::Label lblIoOutputReadout_ { {}, "0.0 dB" };
    juce::Label lblIoOutputReadoutL_ { {}, "0.0" };
    juce::Label lblIoOutputReadoutR_ { {}, "0.0" };

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
    bool finishingValueEdit_ { false };
    bool lastLimiterActive_ { true };
    bool lastClipperActive_ { true };
    bool lastReleaseAuto_ { false };
    int lastClipperModeIdx_ { -1 };
    int lastCharacterModeIdx_ { -1 };
    int lastAutoReleaseModeIdx_ { -1 };
    int lastDevReleaseEngineIdx_ { -1 };
    int lastDevLaReleasePolesIdx_ { -1 };
    bool ignoreNextEditorClickAway_ { false };
    ValueSlider* editingSlider_ { nullptr };
    juce::TextEditor valueEditor_ { "Value Entry" };
    master_limiter_ui::ClipBallisticsPtr clipBallistics_;
    MeterGroupComponent::ScaleMode currentMeterScale_ { MeterGroupComponent::ScaleMode::FullRange };
    float clipLedLevel_ { 0.0f };
    int lastStereoModeIdx_ { -1 };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attGainDrive_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attLimiterActive_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attPluginBypass_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attClipperDrive_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attClipperActive_;
    std::unique_ptr<juce::ParameterAttachment> attClipperMode_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attCeiling_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attGainCeilingLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRelease_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attReleaseAuto_;
    std::unique_ptr<juce::ParameterAttachment> attAutoReleaseMode_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attBandColor_;
    std::unique_ptr<juce::ParameterAttachment> attDevReleaseEngine_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attDevLaReleaseMs_;
    std::unique_ptr<juce::ParameterAttachment> attDevLaReleasePoles_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attDevLowBandReleaseScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attDevHighBandReleaseScale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attDevSigmaAttackMs_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attDevSigmaDecayScale_;
    std::unique_ptr<juce::ParameterAttachment> attCharacter_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attGainMatchAutoTrack_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoInputTrimL_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoInputTrimR_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attIoInputLink_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoOutputTrimL_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIoOutputTrimR_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attIoOutputLink_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
