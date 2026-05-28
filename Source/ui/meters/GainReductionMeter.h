#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

class MasterLimiterAudioProcessor;

class GainReductionMeter : public juce::Component
{
public:
    GainReductionMeter (mdsp_ui::UiContext& ui, MasterLimiterAudioProcessor& processor);
    ~GainReductionMeter() override = default;

    void sync (float dtSec);
    void resetPeakHolds() noexcept;

    void paint (juce::Graphics& g) override;

private:
    static juce::String formatDb (float grDb);

    mdsp_ui::UiContext& ui_;
    MasterLimiterAudioProcessor& processor_;

    float grDb_ { 0.0f };
    float displayGrDb_ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainReductionMeter)
};
