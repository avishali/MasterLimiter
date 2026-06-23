#pragma once

#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/ThemeVariant.h>

#include "PluginProcessor.h"
#include "ui/DevControlsComponent.h"
#include "ui/MasterLimiterLookAndFeel.h"
#include "ui/MainView.h"

//==============================================================================
class MasterLimiterAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MasterLimiterAudioProcessorEditor (MasterLimiterAudioProcessor&);
    ~MasterLimiterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kDesignWidth = 1100;
    static constexpr int kDesignHeight = 580;
    static constexpr int kDevDockWidth = 360;
    static constexpr int kMinBaseWidth = 958;
    static constexpr int kMinBaseHeight = 505;
    static constexpr int kMaxBaseWidth = 4000;
    static constexpr int kMaxBaseHeight = 2109;

    class DevPanel : public juce::Component
    {
    public:
        DevPanel (MasterLimiterAudioProcessor& processor, mdsp_ui::UiContext& uiContext);

        void paint (juce::Graphics& g) override;
        void resized() override;

        std::function<void()> onClose;

    private:
        juce::Label title_ { {}, "DEV CONTROLS — tuning (temporary)" };
        juce::TextButton closeButton_ { "✕" };
        DevControlsComponent devControls_;
    };

    void timerCallback() override;
    void toggleHistoryGraph();
    void closeHistoryGraphWindow();
    void toggleDevControls();
    void updateConstrainer();

    class HistoryWindow : public juce::DocumentWindow
    {
    public:
        explicit HistoryWindow (juce::Colour backgroundColour);
        void closeButtonPressed() override;

        std::function<void()> onClose;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HistoryWindow)
    };

    mdsp_ui::UiContext ui_;
    MasterLimiterLookAndFeel lnf_;
    juce::ComponentBoundsConstrainer constrainer_;
    MasterLimiterAudioProcessor& processor_;
    MainView mainView;
    DevPanel devPanel_;
    std::unique_ptr<HistoryWindow> historyWindow_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLimiterAudioProcessorEditor)
};
