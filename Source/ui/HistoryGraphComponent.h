#pragma once

#include <array>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

#include "PluginProcessor.h"

class HistoryGraphComponent : public juce::Component,
                              private juce::Timer
{
public:
    HistoryGraphComponent (MasterLimiterAudioProcessor& processor, mdsp_ui::UiContext& uiContext);
    ~HistoryGraphComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void resetToLive() noexcept;

private:
    using HistoryFrame = MasterLimiterAudioProcessor::HistoryFrame;

    struct PixelFrame
    {
        float grDb = 0.0f;
        float outDb = -120.0f;
        float inDb = -120.0f;
        float clipDb = 0.0f;
        float grLowDb = 0.0f;
        float grMidDb = 0.0f;
        float grHighDb = 0.0f;
        bool valid = false;
    };

    void timerCallback() override;
    void setWindowSeconds (double seconds);
    void setLevelBottomDb (float bottomDb);
    void setGrRangeDb (float rangeDb);
    void styleSelector (juce::ComboBox& selector) const;
    void updateVisibleCapacity();
    void appendFrames (const HistoryFrame* frames, int count);
    PixelFrame frameForPixel (int x, int width) const noexcept;
    float levelY (float db, juce::Rectangle<float> graph) const noexcept;
    float grY (float db, juce::Rectangle<float> graph) const noexcept;
    void drawGrid (juce::Graphics& g, juce::Rectangle<float> graph);
    void drawTraces (juce::Graphics& g, juce::Rectangle<float> graph);

    MasterLimiterAudioProcessor& processor_;
    mdsp_ui::UiContext& ui_;
    juce::ComboBox windowSelector_ { "History Window" };
    juce::ComboBox levelRangeSelector_ { "Level Range" };
    juce::ComboBox grRangeSelector_ { "GR Range" };
    std::vector<HistoryFrame> displayFrames_;
    std::array<HistoryFrame, 1024> tmpFrames_ {};
    uint32_t cursor_ = 0;
    double windowSeconds_ = 3.0;
    float levelBottomDb_ = -48.0f;
    float grRangeDb_ = 18.0f;
    int visibleFrameCapacity_ = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HistoryGraphComponent)
};
