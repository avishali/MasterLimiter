#include "MasterLimiterLookAndFeel.h"

#include <cmath>

namespace
{
void addKnobArc (juce::Path& path,
                 juce::Point<float> centre,
                 float radius,
                 float startAngle,
                 float endAngle)
{
    constexpr int kSegments = 48;

    for (int i = 0; i <= kSegments; ++i)
    {
        const auto t = static_cast<float> (i) / static_cast<float> (kSegments);
        const auto angle = startAngle + (endAngle - startAngle) * t;
        const auto p = juce::Point<float> (centre.x + std::sin (angle) * radius,
                                           centre.y - std::cos (angle) * radius);

        if (i == 0)
            path.startNewSubPath (p);
        else
            path.lineTo (p);
    }
}
} // namespace

MasterLimiterLookAndFeel::MasterLimiterLookAndFeel (mdsp_ui::UiContext& ui)
    : mdsp_ui::LookAndFeel (ui),
      ui_ (ui)
{
}

MasterLimiterLookAndFeel::~MasterLimiterLookAndFeel() = default;

void MasterLimiterLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height,
                                                 float sliderPosProportional,
                                                 float rotaryStartAngle,
                                                 float rotaryEndAngle,
                                                 juce::Slider& slider)
{
    juce::ignoreUnused (rotaryStartAngle, rotaryEndAngle);

    const auto& theme = ui_.theme();
    const auto& m = ui_.metrics();
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                               static_cast<float> (y),
                                               static_cast<float> (width),
                                               static_cast<float> (height)).reduced (4.0f);
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto radius = size * 0.46f;
    const auto centre = bounds.getCentre();
    const auto knobBounds = juce::Rectangle<float> (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f; // 225 degrees
    constexpr float endAngle = juce::MathConstants<float>::pi * 2.75f;   // 495 degrees
    const float clampedPos = juce::jlimit (0.0f, 1.0f, sliderPosProportional);
    const float valueAngle = startAngle + clampedPos * (endAngle - startAngle);

    g.setColour (theme.background.brighter (0.11f));
    g.fillEllipse (knobBounds);
    g.setColour (theme.gridMajor.withAlpha (0.75f));
    g.drawEllipse (knobBounds, m.strokeMed);

    juce::Path track;
    addKnobArc (track, centre, radius + 2.0f, startAngle, endAngle);
    g.setColour (theme.grid.withAlpha (0.92f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (clampedPos > 0.002f)
    {
        juce::Path valueArc;
        addKnobArc (valueArc, centre, radius + 2.0f, startAngle, valueAngle);
        g.setColour (theme.accent.brighter (0.65f));
        g.strokePath (valueArc, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto tickInner = radius * 0.70f;
    const auto tickOuter = radius * 0.93f;
    const auto pointerStart = juce::Point<float> (centre.x + std::sin (valueAngle) * tickInner,
                                                 centre.y - std::cos (valueAngle) * tickInner);
    const auto pointerEnd = juce::Point<float> (centre.x + std::sin (valueAngle) * tickOuter,
                                               centre.y - std::cos (valueAngle) * tickOuter);
    g.setColour (theme.accent.brighter (0.75f));
    g.drawLine ({ pointerStart, pointerEnd }, 3.0f);

    g.setColour (theme.text);
    g.setFont (ui_.type().labelFont().withHeight (knobBounds.getWidth() > 92.0f ? 18.0f : 11.0f).boldened());
    g.drawText (slider.getTextFromValue (slider.getValue()),
                knobBounds.toNearestInt().reduced (8),
                juce::Justification::centred,
                true);
}

void MasterLimiterLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height,
                                                 float sliderPos,
                                                 float minSliderPos,
                                                 float maxSliderPos,
                                                 juce::Slider::SliderStyle style,
                                                 juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos, slider);

    if (style != juce::Slider::LinearVertical)
    {
        mdsp_ui::LookAndFeel::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto& theme = ui_.theme();
    const auto& m = ui_.metrics();
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                               static_cast<float> (y),
                                               static_cast<float> (width),
                                               static_cast<float> (height)).reduced (4.0f, 6.0f);
    const auto trackW = juce::jmin (12.0f, bounds.getWidth() * 0.33f);
    const auto track = juce::Rectangle<float> (bounds.getCentreX() - trackW * 0.5f,
                                              bounds.getY(),
                                              trackW,
                                              bounds.getHeight());

    g.setColour (theme.background.darker (0.2f));
    g.fillRoundedRectangle (track, m.rMed);
    g.setColour (theme.grid.withAlpha (0.8f));
    g.drawRoundedRectangle (track, m.rMed, m.strokeThin);

    const auto handleY = juce::jlimit (bounds.getY() + 6.0f, bounds.getBottom() - 6.0f, sliderPos);
    const auto handle = juce::Rectangle<float> (bounds.getX() + 2.0f,
                                               handleY - 5.0f,
                                               bounds.getWidth() - 4.0f,
                                               10.0f);
    g.setColour (juce::Colour::fromRGB (0x1d, 0x1d, 0x37));
    g.fillRoundedRectangle (handle, 3.0f);
    g.setColour (theme.text.withAlpha (0.28f));
    g.drawRoundedRectangle (handle, 3.0f, m.strokeThin);
}
