#include "MasterLimiterLookAndFeel.h"

#include <cmath>

namespace
{
namespace palette
{
const juce::Colour bgDeep       = juce::Colour::fromRGB (0x0d, 0x10, 0x15);
const juce::Colour panel        = juce::Colour::fromRGB (0x16, 0x1b, 0x22);
const juce::Colour control      = juce::Colour::fromRGB (0x1e, 0x24, 0x2e);
const juce::Colour controlRaised = juce::Colour::fromRGB (0x23, 0x2a, 0x35);
const juce::Colour controlLow   = juce::Colour::fromRGB (0x1a, 0x20, 0x29);
const juce::Colour border       = juce::Colour::fromRGB (0x2c, 0x33, 0x3f);
const juce::Colour borderStrong = juce::Colour::fromRGB (0x3a, 0x43, 0x50);
const juce::Colour grid         = juce::Colour::fromRGB (0x38, 0x41, 0x4e);
const juce::Colour text         = juce::Colour::fromRGB (0xe8, 0xed, 0xf3);
const juce::Colour textMuted    = juce::Colour::fromRGB (0x8c, 0x97, 0xa6);
const juce::Colour accent       = juce::Colour::fromRGB (0x33, 0xd2, 0xbe);
const juce::Colour accentBright = juce::Colour::fromRGB (0x5b, 0xe7, 0xd6);
const juce::Colour accentDim    = juce::Colour::fromRGB (0x1c, 0x7a, 0x70);
const juce::Colour warning      = juce::Colour::fromRGB (0xe8, 0x70, 0x4f);
const juce::Colour iconOff      = juce::Colour::fromRGB (0x5a, 0x66, 0x75);
} // namespace palette

constexpr float kButtonRadius = 7.0f;
constexpr float kButtonBorderPx = 1.0f;
constexpr float kButtonOuterGlowPx = 2.0f;
constexpr float kPowerGlyphStrokePx = 3.2f;
constexpr float kLinkStrokePx = 3.0f;
constexpr float kKnobTrackStrokePx = 5.2f;
constexpr float kKnobValueStrokePx = 5.6f;
constexpr float kKnobGlowStrokePx = 8.0f;

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

void fillVerticalGradient (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour top, juce::Colour bottom)
{
    juce::ColourGradient gradient (top,
                                   bounds.getCentreX(),
                                   bounds.getY(),
                                   bottom,
                                   bounds.getCentreX(),
                                   bounds.getBottom(),
                                   false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, kButtonRadius);
}

bool isPowerButton (const juce::Button& button)
{
    return button.getName() == "LimiterPower";
}

bool isLinkButton (const juce::Button& button)
{
    const auto name = button.getName();
    return name == "GainCeilingLinkIcon" || name == "IoInputLinkIcon" || name == "IoOutputLinkIcon";
}

bool isSegmentedButton (const juce::Button& button)
{
    const auto name = button.getName();
    return name == "ClipperModeSegment" || name == "StereoModeSegment" || name == "CeilingModeSegment";
}

bool isMeterZoomButton (const juce::Button& button)
{
    const auto name = button.getName();
    return name == "MeterScaleMinus" || name == "MeterScalePlus";
}

bool isTextButtonActive (const juce::Button& button)
{
    const auto text = button.getButtonText();
    return button.getToggleState() || text == "Listening..." || text == "Learned" || text == "Bypassed";
}

void drawSoftButtonBackground (juce::Graphics& g,
                               juce::Rectangle<float> bounds,
                               bool highlighted,
                               bool down,
                               bool active,
                               bool enabled)
{
    bounds = bounds.reduced (0.5f);

    if (active && enabled)
    {
        g.setColour (palette::accent.withAlpha (0.16f));
        g.drawRoundedRectangle (bounds.expanded (kButtonOuterGlowPx), kButtonRadius + kButtonOuterGlowPx, 1.0f);
        g.setColour (palette::accent.withAlpha (0.28f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), kButtonRadius + 1.0f, 1.0f);
    }

    const auto top = active ? juce::Colour::fromRGB (0x1f, 0x2d, 0x2f)
                            : (highlighted ? juce::Colour::fromRGB (0x28, 0x30, 0x40) : palette::controlRaised);
    const auto bottom = active ? juce::Colour::fromRGB (0x16, 0x24, 0x24)
                               : (highlighted ? palette::control : palette::controlLow);
    fillVerticalGradient (g, bounds, down ? bottom : top, down ? top : bottom);

    g.setColour (juce::Colours::white.withAlpha (active ? 0.06f : 0.045f));
    g.drawLine (bounds.getX() + kButtonRadius,
                bounds.getY() + 1.0f,
                bounds.getRight() - kButtonRadius,
                bounds.getY() + 1.0f,
                1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.drawLine (bounds.getX() + kButtonRadius,
                bounds.getBottom() - 1.0f,
                bounds.getRight() - kButtonRadius,
                bounds.getBottom() - 1.0f,
                1.0f);

    const auto border = active ? palette::accent : (highlighted ? palette::borderStrong : palette::border);
    g.setColour (border.withAlpha (enabled ? 1.0f : 0.45f));
    g.drawRoundedRectangle (bounds, kButtonRadius, kButtonBorderPx);
}

void drawButtonLabel (juce::Graphics& g,
                      mdsp_ui::UiContext& ui,
                      const juce::Button& button,
                      juce::Rectangle<float> bounds,
                      bool active)
{
    const auto alpha = button.isEnabled() ? 1.0f : 0.42f;
    g.setColour ((active ? palette::text : palette::textMuted).withAlpha (alpha));
    g.setFont (ui.type().labelFont().withHeight (11.0f).boldened());
    g.drawFittedText (button.getButtonText(), bounds.toNearestInt().reduced (6, 1), juce::Justification::centred, 1);
}

void drawMeterZoomGlyph (juce::Graphics& g, const juce::Button& button)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (4.0f);
    const auto centre = bounds.getCentre();
    const auto halfW = bounds.getWidth() * 0.32f;
    const auto colour = (button.isEnabled() ? palette::text : palette::textMuted.withAlpha (0.42f));

    g.setColour (colour);
    g.drawLine (centre.x - halfW, centre.y, centre.x + halfW, centre.y, 1.8f);

    if (button.getName() == "MeterScalePlus")
        g.drawLine (centre.x, centre.y - halfW, centre.x, centre.y + halfW, 1.8f);
}

void drawPowerGlyph (juce::Graphics& g, juce::Rectangle<float> bounds, bool active, bool highlighted, bool enabled)
{
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto circle = bounds.withSizeKeepingCentre (size, size).reduced (2.0f);
    const auto centre = circle.getCentre();
    const auto radius = circle.getWidth() * 0.5f;

    if (active && enabled)
    {
        g.setColour (palette::accent.withAlpha (0.22f));
        g.fillEllipse (circle.expanded (3.0f));
    }

    g.setColour (active ? palette::accentDim.withAlpha (0.20f) : palette::panel);
    g.fillEllipse (circle);
    g.setColour (active ? palette::accent.withAlpha (0.55f) : palette::border);
    g.drawEllipse (circle, 1.4f);
    g.setColour (juce::Colours::black.withAlpha (0.36f));
    g.drawEllipse (circle.reduced (4.0f), 1.0f);

    const auto glyphColour = (active ? palette::accentBright : (highlighted ? palette::textMuted : palette::iconOff))
                                 .withAlpha (enabled ? 1.0f : 0.38f);
    const float glyphRadius = radius * 0.43f;
    juce::Path arc;
    addKnobArc (arc, centre, glyphRadius, juce::MathConstants<float>::pi * 0.28f, juce::MathConstants<float>::pi * 1.72f);

    if (active && enabled)
    {
        g.setColour (palette::accent.withAlpha (0.28f));
        g.strokePath (arc, juce::PathStrokeType (kPowerGlyphStrokePx + 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (centre.x, centre.y - glyphRadius - 3.0f, centre.x, centre.y + 1.0f, kPowerGlyphStrokePx + 3.0f);
    }

    g.setColour (glyphColour);
    g.strokePath (arc, juce::PathStrokeType (kPowerGlyphStrokePx, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (centre.x, centre.y - glyphRadius - 4.0f, centre.x, centre.y + 1.0f, kPowerGlyphStrokePx);
}

void drawLinkIcon (juce::Graphics& g, juce::Rectangle<float> bounds, bool active, bool highlighted, bool enabled)
{
    drawSoftButtonBackground (g, bounds, highlighted, false, active, enabled);

    const auto iconBounds = bounds.reduced (9.0f, juce::jmax (5.0f, bounds.getHeight() * 0.24f));
    const auto linkW = iconBounds.getWidth() * 0.52f;
    const auto linkH = juce::jmin (iconBounds.getHeight(), 14.0f);
    const auto y = iconBounds.getCentreY() - linkH * 0.5f;
    const auto left = juce::Rectangle<float> (iconBounds.getX(), y, linkW, linkH);
    const auto right = juce::Rectangle<float> (iconBounds.getRight() - linkW, y, linkW, linkH);
    const auto colour = (active ? palette::accent : (highlighted ? palette::textMuted : palette::iconOff))
                            .withAlpha (enabled ? 1.0f : 0.38f);

    if (active && enabled)
    {
        g.setColour (palette::accent.withAlpha (0.26f));
        g.drawRoundedRectangle (left.expanded (1.4f), linkH * 0.5f + 1.4f, kLinkStrokePx + 2.2f);
        g.drawRoundedRectangle (right.expanded (1.4f), linkH * 0.5f + 1.4f, kLinkStrokePx + 2.2f);
    }

    g.setColour (colour);
    g.drawRoundedRectangle (left, linkH * 0.5f, kLinkStrokePx);
    g.drawRoundedRectangle (right, linkH * 0.5f, kLinkStrokePx);
}

void drawSegmentedToggle (juce::Graphics& g,
                          mdsp_ui::UiContext& ui,
                          const juce::Button& button,
                          juce::Rectangle<float> bounds,
                          bool highlighted)
{
    const auto name = button.getName();
    const juce::String left = name == "ClipperModeSegment" ? "Hard" : (name == "StereoModeSegment" ? "Stereo" : "SP");
    const juce::String right = name == "ClipperModeSegment" ? "Soft" : (name == "StereoModeSegment" ? "M/S" : "TP");
    const bool rightSelected = button.getButtonText() == right || button.getToggleState();

    bounds = bounds.reduced (0.5f);
    g.setColour (palette::controlLow);
    g.fillRoundedRectangle (bounds, kButtonRadius);
    g.setColour (highlighted ? palette::borderStrong : palette::border);
    g.drawRoundedRectangle (bounds, kButtonRadius, 1.0f);

    auto leftBounds = bounds;
    leftBounds.setWidth (std::floor (bounds.getWidth() * 0.5f));
    auto rightBounds = bounds;
    rightBounds.setLeft (leftBounds.getRight());
    const auto selected = rightSelected ? rightBounds : leftBounds;

    juce::ColourGradient gradient (juce::Colour::fromRGB (0x1f, 0x2d, 0x2f),
                                   selected.getCentreX(),
                                   selected.getY(),
                                   juce::Colour::fromRGB (0x18, 0x24, 0x26),
                                   selected.getCentreX(),
                                   selected.getBottom(),
                                   false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (selected.reduced (1.0f), kButtonRadius - 1.0f);
    g.setColour (palette::accent.withAlpha (0.18f));
    g.drawRoundedRectangle (selected.reduced (1.0f), kButtonRadius - 1.0f, 1.0f);

    g.setColour (palette::border.withAlpha (0.82f));
    g.drawVerticalLine (static_cast<int> (std::round (bounds.getCentreX())), bounds.getY() + 3.0f, bounds.getBottom() - 3.0f);

    g.setFont (ui.type().labelFont().withHeight (10.5f).boldened());
    g.setColour ((rightSelected ? palette::textMuted : palette::text).withAlpha (button.isEnabled() ? 1.0f : 0.42f));
    g.drawFittedText (left, leftBounds.toNearestInt().reduced (4, 1), juce::Justification::centred, 1);
    g.setColour ((rightSelected ? palette::text : palette::textMuted).withAlpha (button.isEnabled() ? 1.0f : 0.42f));
    g.drawFittedText (right, rightBounds.toNearestInt().reduced (4, 1), juce::Justification::centred, 1);
}
} // namespace

MasterLimiterLookAndFeel::MasterLimiterLookAndFeel (mdsp_ui::UiContext& ui)
    : mdsp_ui::LookAndFeel (ui),
      ui_ (ui)
{
    setColour (juce::ResizableWindow::backgroundColourId, palette::bgDeep);
    setColour (juce::Slider::trackColourId, palette::grid);
    setColour (juce::Slider::backgroundColourId, palette::controlLow);
    setColour (juce::Slider::thumbColourId, palette::accent);
    setColour (juce::Slider::textBoxTextColourId, palette::text);
    setColour (juce::ComboBox::backgroundColourId, palette::control);
    setColour (juce::ComboBox::outlineColourId, palette::border);
    setColour (juce::ComboBox::textColourId, palette::text);
    setColour (juce::ComboBox::arrowColourId, palette::accent);
    setColour (juce::TextButton::buttonColourId, palette::control);
    setColour (juce::TextButton::buttonOnColourId, palette::accent.withAlpha (0.28f));
    setColour (juce::TextButton::textColourOffId, palette::textMuted);
    setColour (juce::TextButton::textColourOnId, palette::text);
}

MasterLimiterLookAndFeel::~MasterLimiterLookAndFeel() = default;

void MasterLimiterLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                     juce::Button& button,
                                                     const juce::Colour& backgroundColour,
                                                     bool shouldDrawButtonAsHighlighted,
                                                     bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (backgroundColour);
    drawSoftButtonBackground (g,
                              button.getLocalBounds().toFloat(),
                              shouldDrawButtonAsHighlighted,
                              shouldDrawButtonAsDown,
                              isTextButtonActive (button),
                              button.isEnabled());
}

void MasterLimiterLookAndFeel::drawButtonText (juce::Graphics& g,
                                               juce::TextButton& button,
                                               bool shouldDrawButtonAsHighlighted,
                                               bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    if (isMeterZoomButton (button))
    {
        drawMeterZoomGlyph (g, button);
        return;
    }

    drawButtonLabel (g, ui_, button, button.getLocalBounds().toFloat(), isTextButtonActive (button));
}

void MasterLimiterLookAndFeel::drawToggleButton (juce::Graphics& g,
                                                 juce::ToggleButton& button,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();

    if (isPowerButton (button))
    {
        drawPowerGlyph (g, bounds, button.getToggleState(), shouldDrawButtonAsHighlighted, button.isEnabled());
        return;
    }

    if (isLinkButton (button))
    {
        drawLinkIcon (g, bounds, button.getToggleState(), shouldDrawButtonAsHighlighted, button.isEnabled());
        return;
    }

    if (isSegmentedButton (button))
    {
        drawSegmentedToggle (g, ui_, button, bounds, shouldDrawButtonAsHighlighted);
        return;
    }

    const bool active = button.getToggleState();
    drawSoftButtonBackground (g, bounds, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown, active, button.isEnabled());
    drawButtonLabel (g, ui_, button, bounds, active);
}

void MasterLimiterLookAndFeel::drawComboBox (juce::Graphics& g,
                                             int width,
                                             int height,
                                             bool isButtonDown,
                                             int buttonX,
                                             int buttonY,
                                             int buttonW,
                                             int buttonH,
                                             juce::ComboBox& box)
{
    juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    drawSoftButtonBackground (g, bounds, box.isMouseOver(), isButtonDown, false, box.isEnabled());

    const auto arrow = bounds.removeFromRight (18.0f).reduced (4.0f, 6.0f);
    juce::Path chevron;
    chevron.startNewSubPath (arrow.getX(), arrow.getY());
    chevron.lineTo (arrow.getCentreX(), arrow.getBottom());
    chevron.lineTo (arrow.getRight(), arrow.getY());
    g.setColour (palette::accent.withAlpha (box.isEnabled() ? 0.92f : 0.35f));
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

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

    juce::ColourGradient face (palette::controlRaised,
                               knobBounds.getCentreX(),
                               knobBounds.getY(),
                               palette::controlLow,
                               knobBounds.getCentreX(),
                               knobBounds.getBottom(),
                               false);
    g.setGradientFill (face);
    g.fillEllipse (knobBounds);
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillEllipse (knobBounds.reduced (5.0f));
    g.setGradientFill (face);
    g.fillEllipse (knobBounds.reduced (6.0f));
    g.setColour (palette::borderStrong.withAlpha (0.78f));
    g.drawEllipse (knobBounds, m.strokeMed);

    juce::Path track;
    addKnobArc (track, centre, radius + 2.0f, startAngle, endAngle);
    g.setColour (juce::Colour::fromRGB (0x28, 0x30, 0x38).withAlpha (0.96f));
    g.strokePath (track, juce::PathStrokeType (kKnobTrackStrokePx, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (clampedPos > 0.002f)
    {
        juce::Path valueArc;
        addKnobArc (valueArc, centre, radius + 2.0f, startAngle, valueAngle);
        g.setColour (palette::accent.withAlpha (0.24f));
        g.strokePath (valueArc, juce::PathStrokeType (kKnobGlowStrokePx, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (palette::accent);
        g.strokePath (valueArc, juce::PathStrokeType (kKnobValueStrokePx, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto tickInner = radius * 0.78f;
    const auto tickOuter = radius * 0.95f;
    const auto pointerStart = juce::Point<float> (centre.x + std::sin (valueAngle) * tickInner,
                                                 centre.y - std::cos (valueAngle) * tickInner);
    const auto pointerEnd = juce::Point<float> (centre.x + std::sin (valueAngle) * tickOuter,
                                               centre.y - std::cos (valueAngle) * tickOuter);
    g.setColour (palette::text.withAlpha (0.95f));
    g.drawLine ({ pointerStart, pointerEnd }, 2.4f);

    g.setColour (palette::text);
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

    const auto& m = ui_.metrics();
    const bool transparentFader = slider.getComponentID() == "IoTrimFader";
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                               static_cast<float> (y),
                                               static_cast<float> (width),
                                               static_cast<float> (height)).reduced (4.0f, 6.0f);
    const auto trackW = transparentFader ? 4.0f : juce::jmin (12.0f, bounds.getWidth() * 0.33f);
    const auto track = juce::Rectangle<float> (bounds.getCentreX() - trackW * 0.5f,
                                              bounds.getY(),
                                              trackW,
                                              bounds.getHeight());

    g.setColour ((transparentFader ? palette::bgDeep.withAlpha (0.16f) : palette::bgDeep.brighter (0.02f)));
    g.fillRoundedRectangle (track, m.rMed);
    g.setColour (palette::border.withAlpha (transparentFader ? 0.46f : 0.9f));
    g.drawRoundedRectangle (track, m.rMed, m.strokeThin);

    const auto handleY = juce::jlimit (bounds.getY() + 6.0f, bounds.getBottom() - 6.0f, sliderPos);
    if (! transparentFader)
    {
        auto fill = juce::Rectangle<float> (track.getX(), handleY, track.getWidth(), track.getBottom() - handleY);
        if (fill.getHeight() > 1.0f)
        {
            juce::ColourGradient fillGradient (palette::accentDim,
                                               fill.getCentreX(),
                                               fill.getBottom(),
                                               palette::warning,
                                               fill.getCentreX(),
                                               fill.getY(),
                                               false);
            fillGradient.addColour (0.78, palette::accent);
            g.setGradientFill (fillGradient);
            g.fillRoundedRectangle (fill, m.rMed);
        }
    }

    const auto handleHeight = transparentFader ? 8.0f : 10.0f;
    const auto handle = juce::Rectangle<float> (bounds.getX() + 3.0f,
                                               handleY - handleHeight * 0.5f,
                                               bounds.getWidth() - 6.0f,
                                               handleHeight);
    fillVerticalGradient (g,
                          handle,
                          transparentFader ? palette::accent.withAlpha (0.86f) : palette::borderStrong.brighter (0.08f),
                          transparentFader ? palette::accentDim.withAlpha (0.86f) : palette::control);
    g.setColour ((transparentFader ? palette::text : palette::text.withAlpha (0.28f)));
    g.drawRoundedRectangle (handle, 3.0f, m.strokeThin);
}
