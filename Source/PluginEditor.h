#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/ThemeVariant.h>
#include <mdsp_ui/LookAndFeel.h>

#include "PluginProcessor.h"
#include "ui/MainView.h"

//==============================================================================
class MasterLimiterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MasterLimiterAudioProcessorEditor (MasterLimiterAudioProcessor&);
    ~MasterLimiterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MasterLimiterAudioProcessor& audioProcessor;
    mdsp_ui::UiContext ui_;
    mdsp_ui::LookAndFeel lnf_;
    MainView mainView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLimiterAudioProcessorEditor)
};
