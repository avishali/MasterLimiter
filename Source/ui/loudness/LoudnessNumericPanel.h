#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_dsp/loudness/LoudnessAnalyzer.h>
#include <mdsp_ui/UiContext.h>

#include "PluginProcessor.h"

class LoudnessNumericPanel : public juce::Component
{
public:
    LoudnessNumericPanel (mdsp_ui::UiContext& ui, MasterLimiterAudioProcessor& processor);
    ~LoudnessNumericPanel() override = default;

    /** Called from editor timer; LUFS text commits at most every 200 ms. */
    void refresh();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    static juce::String formatLufs (float lufs);

    mdsp_ui::UiContext& ui_;
    MasterLimiterAudioProcessor& processor_;

    juce::String mLine_;
    juce::String sLine_;
    juce::String iLine_;

    juce::uint32 lastCommitMs_ { 0 };
    mdsp_dsp::LoudnessSnapshot snapshot_ {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessNumericPanel)
};
