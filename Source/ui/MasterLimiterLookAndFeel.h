#pragma once

#include <mdsp_ui/LookAndFeel.h>

class MasterLimiterLookAndFeel : public mdsp_ui::LookAndFeel
{
public:
    explicit MasterLimiterLookAndFeel (mdsp_ui::UiContext& ui);
    ~MasterLimiterLookAndFeel() override;

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics& g,
                           juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

private:
    mdsp_ui::UiContext& ui_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLimiterLookAndFeel)
};
