#pragma once

#include <mdsp_ui/LookAndFeel.h>

class MasterLimiterLookAndFeel : public mdsp_ui::LookAndFeel
{
public:
    explicit MasterLimiterLookAndFeel (mdsp_ui::UiContext& ui);
    ~MasterLimiterLookAndFeel() override;

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawLinearSlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           juce::Slider::SliderStyle style,
                           juce::Slider& slider) override;

private:
    mdsp_ui::UiContext& ui_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLimiterLookAndFeel)
};
