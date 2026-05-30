#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

class MasterLimiterAudioProcessor;

namespace mdsp_ui::meters
{
class MeterBallistics;
class PeakHoldModel;
} // namespace mdsp_ui::meters

class GainReductionMeter : public juce::Component
{
public:
    GainReductionMeter (mdsp_ui::UiContext& ui, MasterLimiterAudioProcessor& processor);
    ~GainReductionMeter() override;

    void sync (float dtSec);
    void resetPeakHolds() noexcept;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    static juce::String formatDbBare (float grDb);

    mdsp_ui::UiContext& ui_;
    MasterLimiterAudioProcessor& processor_;

    float displayGrLDb_ { 0.0f };
    float displayGrRDb_ { 0.0f };
    float displayCurrentGrDb_ { 0.0f };
    float displayMaxGrDb_ { 0.0f };
    std::unique_ptr<mdsp_ui::meters::MeterBallistics> ballL_;
    std::unique_ptr<mdsp_ui::meters::MeterBallistics> ballR_;
    std::unique_ptr<mdsp_ui::meters::PeakHoldModel> peakHoldL_;
    std::unique_ptr<mdsp_ui::meters::PeakHoldModel> peakHoldR_;

    juce::Rectangle<int> meterBounds_;
    juce::Rectangle<int> readoutBounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainReductionMeter)
};
