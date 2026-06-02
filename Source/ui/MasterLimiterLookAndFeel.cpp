#include "MasterLimiterLookAndFeel.h"

#include <cmath>

namespace
{
constexpr float kPowerGlyphStrokePx = 3.2f;
constexpr float kLinkStrokePx = 3.0f;

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

void fillVerticalGradient (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour top, juce::Colour bottom, float radius)
{
    juce::ColourGradient gradient (top,
                                   bounds.getCentreX(),
                                   bounds.getY(),
                                   bottom,
                                   bounds.getCentreX(),
                                   bounds.getBottom(),
                                   false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, radius);
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
                               mdsp_ui::UiContext& ui,
                               juce::Rectangle<float> bounds,
                               bool highlighted,
                               bool down,
                               bool active,
                               bool enabled)
{
    const auto& theme = ui.theme();
    const auto& metrics = ui.metrics();
    bounds = bounds.reduced (0.5f);

    if (active && enabled)
    {
        g.setColour (theme.accent.withAlpha (0.16f));
        g.drawRoundedRectangle (bounds.expanded (metrics.buttonOuterGlowPx), metrics.buttonRadius + metrics.buttonOuterGlowPx, 1.0f);
        g.setColour (theme.accent.withAlpha (0.28f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), metrics.buttonRadius + 1.0f, 1.0f);
    }

    const auto top = active ? theme.accentDim.darker (0.05f)
                            : (highlighted ? theme.controlRaised.brighter (0.08f) : theme.controlRaised);
    const auto bottom = active ? theme.controlLow
                               : (highlighted ? theme.control : theme.controlLow);
    fillVerticalGradient (g, bounds, down ? bottom : top, down ? top : bottom, metrics.buttonRadius);

    g.setColour (juce::Colours::white.withAlpha (active ? 0.06f : 0.045f));
    g.drawLine (bounds.getX() + metrics.buttonRadius,
                bounds.getY() + 1.0f,
                bounds.getRight() - metrics.buttonRadius,
                bounds.getY() + 1.0f,
                1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.drawLine (bounds.getX() + metrics.buttonRadius,
                bounds.getBottom() - 1.0f,
                bounds.getRight() - metrics.buttonRadius,
                bounds.getBottom() - 1.0f,
                1.0f);

    const auto border = active ? theme.accent : (highlighted ? theme.borderStrong : theme.border);
    g.setColour (border.withAlpha (enabled ? 1.0f : 0.45f));
    g.drawRoundedRectangle (bounds, metrics.buttonRadius, metrics.buttonBorderPx);
}

void drawButtonLabel (juce::Graphics& g,
                      mdsp_ui::UiContext& ui,
                      const juce::Button& button,
                      juce::Rectangle<float> bounds,
                      bool active)
{
    const auto& theme = ui.theme();
    const auto alpha = button.isEnabled() ? 1.0f : 0.42f;
    g.setColour ((active ? theme.text : theme.textMuted).withAlpha (alpha));
    g.setFont (ui.type().labelFont().withHeight (11.0f).boldened());
    g.drawFittedText (button.getButtonText(), bounds.toNearestInt().reduced (6, 1), juce::Justification::centred, 1);
}

void drawMeterZoomGlyph (juce::Graphics& g, mdsp_ui::UiContext& ui, const juce::Button& button)
{
    const auto& theme = ui.theme();
    const auto bounds = button.getLocalBounds().toFloat().reduced (4.0f);
    const auto centre = bounds.getCentre();
    const auto halfW = bounds.getWidth() * 0.32f;
    const auto colour = (button.isEnabled() ? theme.text : theme.textMuted.withAlpha (0.42f));

    g.setColour (colour);
    g.drawLine (centre.x - halfW, centre.y, centre.x + halfW, centre.y, 1.8f);

    if (button.getName() == "MeterScalePlus")
        g.drawLine (centre.x, centre.y - halfW, centre.x, centre.y + halfW, 1.8f);
}

void drawPowerGlyph (juce::Graphics& g, mdsp_ui::UiContext& ui, juce::Rectangle<float> bounds, bool active, bool highlighted, bool enabled)
{
    const auto& theme = ui.theme();
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto circle = bounds.withSizeKeepingCentre (size, size).reduced (2.0f);
    const auto centre = circle.getCentre();
    const auto radius = circle.getWidth() * 0.5f;

    if (active && enabled)
    {
        g.setColour (theme.accent.withAlpha (0.22f));
        g.fillEllipse (circle.expanded (3.0f));
    }

    g.setColour (active ? theme.accentDim.withAlpha (0.20f) : theme.panel);
    g.fillEllipse (circle);
    g.setColour (active ? theme.accent.withAlpha (0.55f) : theme.border);
    g.drawEllipse (circle, 1.4f);
    g.setColour (juce::Colours::black.withAlpha (0.36f));
    g.drawEllipse (circle.reduced (4.0f), 1.0f);

    const auto glyphColour = (active ? theme.accentBright : (highlighted ? theme.textMuted : theme.iconOff))
                                 .withAlpha (enabled ? 1.0f : 0.38f);
    const float glyphRadius = radius * 0.43f;
    juce::Path arc;
    addKnobArc (arc, centre, glyphRadius, juce::MathConstants<float>::pi * 0.28f, juce::MathConstants<float>::pi * 1.72f);

    if (active && enabled)
    {
        g.setColour (theme.accent.withAlpha (0.28f));
        g.strokePath (arc, juce::PathStrokeType (kPowerGlyphStrokePx + 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (centre.x, centre.y - glyphRadius - 3.0f, centre.x, centre.y + 1.0f, kPowerGlyphStrokePx + 3.0f);
    }

    g.setColour (glyphColour);
    g.strokePath (arc, juce::PathStrokeType (kPowerGlyphStrokePx, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (centre.x, centre.y - glyphRadius - 4.0f, centre.x, centre.y + 1.0f, kPowerGlyphStrokePx);
}

void drawLinkIcon (juce::Graphics& g, mdsp_ui::UiContext& ui, juce::Rectangle<float> bounds, bool active, bool highlighted, bool enabled)
{
    const auto& theme = ui.theme();
    drawSoftButtonBackground (g, ui, bounds, highlighted, false, active, enabled);

    const auto iconBounds = bounds.reduced (9.0f, juce::jmax (5.0f, bounds.getHeight() * 0.24f));
    const auto linkW = iconBounds.getWidth() * 0.52f;
    const auto linkH = juce::jmin (iconBounds.getHeight(), 14.0f);
    const auto y = iconBounds.getCentreY() - linkH * 0.5f;
    const auto left = juce::Rectangle<float> (iconBounds.getX(), y, linkW, linkH);
    const auto right = juce::Rectangle<float> (iconBounds.getRight() - linkW, y, linkW, linkH);
    const auto colour = (active ? theme.accent : (highlighted ? theme.textMuted : theme.iconOff))
                            .withAlpha (enabled ? 1.0f : 0.38f);

    if (active && enabled)
    {
        g.setColour (theme.accent.withAlpha (0.26f));
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
    const auto& theme = ui.theme();
    const auto& metrics = ui.metrics();
    const auto name = button.getName();
    const juce::String left = name == "ClipperModeSegment" ? "Hard" : (name == "StereoModeSegment" ? "Stereo" : "SP");
    const juce::String right = name == "ClipperModeSegment" ? "Soft" : (name == "StereoModeSegment" ? "M/S" : "TP");
    const bool rightSelected = button.getButtonText() == right || button.getToggleState();

    bounds = bounds.reduced (0.5f);
    g.setColour (theme.controlLow);
    g.fillRoundedRectangle (bounds, metrics.buttonRadius);
    g.setColour (highlighted ? theme.borderStrong : theme.border);
    g.drawRoundedRectangle (bounds, metrics.buttonRadius, metrics.buttonBorderPx);

    auto leftBounds = bounds;
    leftBounds.setWidth (std::floor (bounds.getWidth() * 0.5f));
    auto rightBounds = bounds;
    rightBounds.setLeft (leftBounds.getRight());
    const auto selected = rightSelected ? rightBounds : leftBounds;

    juce::ColourGradient gradient (theme.accentDim.darker (0.05f),
                                   selected.getCentreX(),
                                   selected.getY(),
                                   theme.controlLow,
                                   selected.getCentreX(),
                                   selected.getBottom(),
                                   false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (selected.reduced (1.0f), metrics.buttonRadius - 1.0f);
    g.setColour (theme.accent.withAlpha (0.18f));
    g.drawRoundedRectangle (selected.reduced (1.0f), metrics.buttonRadius - 1.0f, metrics.buttonBorderPx);

    g.setColour (theme.border.withAlpha (0.82f));
    g.drawVerticalLine (static_cast<int> (std::round (bounds.getCentreX())), bounds.getY() + 3.0f, bounds.getBottom() - 3.0f);

    g.setFont (ui.type().labelFont().withHeight (10.5f).boldened());
    g.setColour ((rightSelected ? theme.textMuted : theme.text).withAlpha (button.isEnabled() ? 1.0f : 0.42f));
    g.drawFittedText (left, leftBounds.toNearestInt().reduced (4, 1), juce::Justification::centred, 1);
    g.setColour ((rightSelected ? theme.text : theme.textMuted).withAlpha (button.isEnabled() ? 1.0f : 0.42f));
    g.drawFittedText (right, rightBounds.toNearestInt().reduced (4, 1), juce::Justification::centred, 1);
}
} // namespace

MasterLimiterLookAndFeel::MasterLimiterLookAndFeel (mdsp_ui::UiContext& ui)
    : mdsp_ui::LookAndFeel (ui),
      ui_ (ui)
{
    const auto& theme = ui.theme();
    setColour (juce::ResizableWindow::backgroundColourId, theme.background);
    setColour (juce::Slider::trackColourId, theme.grid);
    setColour (juce::Slider::backgroundColourId, theme.controlLow);
    setColour (juce::Slider::thumbColourId, theme.accent);
    setColour (juce::Slider::textBoxTextColourId, theme.text);
    setColour (juce::ComboBox::backgroundColourId, theme.control);
    setColour (juce::ComboBox::outlineColourId, theme.border);
    setColour (juce::ComboBox::textColourId, theme.text);
    setColour (juce::ComboBox::arrowColourId, theme.accent);
    setColour (juce::TextButton::buttonColourId, theme.control);
    setColour (juce::TextButton::buttonOnColourId, theme.accent.withAlpha (0.28f));
    setColour (juce::TextButton::textColourOffId, theme.textMuted);
    setColour (juce::TextButton::textColourOnId, theme.text);
}

MasterLimiterLookAndFeel::~MasterLimiterLookAndFeel() = default;

void MasterLimiterLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                     juce::Button& button,
                                                     const juce::Colour& backgroundColour,
                                                     bool shouldDrawButtonAsHighlighted,
                                                     bool shouldDrawButtonAsDown)
{
    if (isTextButtonActive (button) && ! button.getToggleState())
    {
        juce::ignoreUnused (backgroundColour);
        drawSoftButtonBackground (g,
                              ui_,
                              button.getLocalBounds().toFloat(),
                              shouldDrawButtonAsHighlighted,
                              shouldDrawButtonAsDown,
                              true,
                              button.isEnabled());
        return;
    }

    mdsp_ui::LookAndFeel::drawButtonBackground (g, button, backgroundColour, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

void MasterLimiterLookAndFeel::drawButtonText (juce::Graphics& g,
                                               juce::TextButton& button,
                                               bool shouldDrawButtonAsHighlighted,
                                               bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    if (isMeterZoomButton (button))
    {
        drawMeterZoomGlyph (g, ui_, button);
        return;
    }

    if (isTextButtonActive (button) && ! button.getToggleState())
    {
        drawButtonLabel (g, ui_, button, button.getLocalBounds().toFloat(), true);
        return;
    }

    mdsp_ui::LookAndFeel::drawButtonText (g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

void MasterLimiterLookAndFeel::drawToggleButton (juce::Graphics& g,
                                                 juce::ToggleButton& button,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();

    if (isPowerButton (button))
    {
        drawPowerGlyph (g, ui_, bounds, button.getToggleState(), shouldDrawButtonAsHighlighted, button.isEnabled());
        return;
    }

    if (isLinkButton (button))
    {
        drawLinkIcon (g, ui_, bounds, button.getToggleState(), shouldDrawButtonAsHighlighted, button.isEnabled());
        return;
    }

    if (isSegmentedButton (button))
    {
        drawSegmentedToggle (g, ui_, button, bounds, shouldDrawButtonAsHighlighted);
        return;
    }

    mdsp_ui::LookAndFeel::drawToggleButton (g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

