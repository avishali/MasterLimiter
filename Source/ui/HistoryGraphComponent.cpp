#include "ui/HistoryGraphComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
namespace palette
{
const juce::Colour bgDeep       = juce::Colour::fromRGB (0x0d, 0x10, 0x15);
const juce::Colour panel        = juce::Colour::fromRGB (0x16, 0x1b, 0x22);
const juce::Colour control      = juce::Colour::fromRGB (0x1e, 0x24, 0x2e);
const juce::Colour border       = juce::Colour::fromRGB (0x2c, 0x33, 0x3f);
const juce::Colour grid         = juce::Colour::fromRGB (0x38, 0x41, 0x4e);
const juce::Colour text         = juce::Colour::fromRGB (0xe8, 0xed, 0xf3);
const juce::Colour textMuted    = juce::Colour::fromRGB (0x8c, 0x97, 0xa6);
const juce::Colour accent       = juce::Colour::fromRGB (0x33, 0xd2, 0xbe);
const juce::Colour accentBright = juce::Colour::fromRGB (0x5b, 0xe7, 0xd6);
const juce::Colour warning      = juce::Colour::fromRGB (0xe8, 0x70, 0x4f);
} // namespace palette

constexpr float kLevelTopDb = 0.0f;
constexpr float kLevelBottomDb = -24.0f;
constexpr float kGrRangeDb = 18.0f;
constexpr float kDbGrid[] { 0.0f, -3.0f, -6.0f, -9.0f, -12.0f, -18.0f };

juce::String formatTimeLabel (double seconds)
{
    if (std::abs (seconds - std::round (seconds)) < 0.01)
        return "-" + juce::String ((int) std::round (seconds)) + "s";

    return "-" + juce::String (seconds, 1) + "s";
}
} // namespace

HistoryGraphComponent::HistoryGraphComponent (MasterLimiterAudioProcessor& processor,
                                              mdsp_ui::UiContext& uiContext)
    : processor_ (processor),
      ui_ (uiContext)
{
    windowSelector_.addItem ("1.5 s", 1);
    windowSelector_.addItem ("3 s", 2);
    windowSelector_.addItem ("6 s", 3);
    windowSelector_.setSelectedId (2, juce::dontSendNotification);
    windowSelector_.setJustificationType (juce::Justification::centred);
    windowSelector_.setColour (juce::ComboBox::backgroundColourId, palette::control);
    windowSelector_.setColour (juce::ComboBox::textColourId, palette::text);
    windowSelector_.setColour (juce::ComboBox::outlineColourId, palette::border);
    windowSelector_.setColour (juce::ComboBox::focusedOutlineColourId, palette::accent.withAlpha (0.8f));
    windowSelector_.setColour (juce::ComboBox::arrowColourId, palette::accentBright);
    windowSelector_.onChange = [this]
    {
        switch (windowSelector_.getSelectedId())
        {
            case 1:  setWindowSeconds (1.5); break;
            case 3:  setWindowSeconds (6.0); break;
            default: setWindowSeconds (3.0); break;
        }
    };
    addAndMakeVisible (windowSelector_);

    resetToLive();
    updateVisibleCapacity();
    startTimerHz (60);
}

HistoryGraphComponent::~HistoryGraphComponent()
{
    stopTimer();
}

void HistoryGraphComponent::resetToLive() noexcept
{
    cursor_ = processor_.getHistoryWriteIndex();
    displayFrames_.clear();
}

void HistoryGraphComponent::timerCallback()
{
    updateVisibleCapacity();

    const int copied = processor_.readHistorySince (cursor_, tmpFrames_.data(), (int) tmpFrames_.size());
    if (copied > 0)
    {
        appendFrames (tmpFrames_.data(), copied);
        repaint();
    }
}

void HistoryGraphComponent::setWindowSeconds (double seconds)
{
    windowSeconds_ = seconds;
    updateVisibleCapacity();
    repaint();
}

void HistoryGraphComponent::updateVisibleCapacity()
{
    const double sr = processor_.getHistorySampleRate();
    const int frameSamples = processor_.getHistoryFrameSamples();
    const int capacity = (sr > 0.0 && frameSamples > 0)
        ? juce::jmax (1, (int) std::ceil (windowSeconds_ * sr / (double) frameSamples))
        : 1;

    if (capacity == visibleFrameCapacity_)
        return;

    visibleFrameCapacity_ = capacity;
    displayFrames_.reserve ((size_t) visibleFrameCapacity_);

    if ((int) displayFrames_.size() > visibleFrameCapacity_)
        displayFrames_.erase (displayFrames_.begin(),
                              displayFrames_.end() - visibleFrameCapacity_);
}

void HistoryGraphComponent::appendFrames (const HistoryFrame* frames, int count)
{
    if (frames == nullptr || count <= 0)
        return;

    const int needed = (int) displayFrames_.size() + count - visibleFrameCapacity_;
    if (needed > 0)
    {
        const int eraseCount = juce::jmin (needed, (int) displayFrames_.size());
        displayFrames_.erase (displayFrames_.begin(), displayFrames_.begin() + eraseCount);
    }

    displayFrames_.insert (displayFrames_.end(), frames, frames + count);

    if ((int) displayFrames_.size() > visibleFrameCapacity_)
        displayFrames_.erase (displayFrames_.begin(),
                              displayFrames_.end() - visibleFrameCapacity_);
}

HistoryGraphComponent::PixelFrame HistoryGraphComponent::frameForPixel (int x, int width) const noexcept
{
    PixelFrame result;

    if (width <= 0 || displayFrames_.empty())
        return result;

    const float framesPerPixel = (float) juce::jmax (1, visibleFrameCapacity_) / (float) width;
    const float ageStart = (float) (width - 1 - x) * framesPerPixel;
    const float ageEnd = (float) (width - x) * framesPerPixel;
    const int firstAge = juce::jmax (0, (int) std::floor (ageStart));
    const int lastAge = juce::jmax (firstAge, (int) std::ceil (ageEnd) - 1);
    const int newestIndex = (int) displayFrames_.size() - 1;

    for (int age = firstAge; age <= lastAge; ++age)
    {
        const int index = newestIndex - age;
        if (index < 0 || index >= (int) displayFrames_.size())
            continue;

        const auto& frame = displayFrames_[(size_t) index];
        result.grDb = std::max (result.grDb, frame.grDb);
        result.outDb = std::max (result.outDb, frame.outDb);
        result.inDb = std::max (result.inDb, frame.inDb);
        result.valid = true;
    }

    return result;
}

float HistoryGraphComponent::levelY (float db, juce::Rectangle<float> graph) const noexcept
{
    if (! std::isfinite (db))
        db = kLevelBottomDb;

    const float clamped = juce::jlimit (kLevelBottomDb, kLevelTopDb, db);
    const float norm = (kLevelTopDb - clamped) / (kLevelTopDb - kLevelBottomDb);
    return graph.getY() + norm * graph.getHeight();
}

float HistoryGraphComponent::grY (float db, juce::Rectangle<float> graph) const noexcept
{
    if (! std::isfinite (db))
        db = 0.0f;

    const float norm = juce::jlimit (0.0f, 1.0f, db / kGrRangeDb);
    return graph.getY() + norm * graph.getHeight();
}

void HistoryGraphComponent::paint (juce::Graphics& g)
{
    g.fillAll (palette::bgDeep);

    auto bounds = getLocalBounds().toFloat().reduced (12.0f);
    g.setColour (palette::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (palette::border);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    g.setFont (ui_.type().titleFont().withHeight (16.0f).boldened());
    g.setColour (palette::accentBright);
    g.drawText ("History Graph", bounds.removeFromTop (28.0f), juce::Justification::centredLeft);

    auto graph = getLocalBounds().toFloat().reduced (18.0f);
    graph.removeFromTop (36.0f);
    graph.removeFromBottom (18.0f);

    drawGrid (g, graph);
    drawTraces (g, graph);
}

void HistoryGraphComponent::resized()
{
    auto bounds = getLocalBounds().reduced (18);
    windowSelector_.setBounds (bounds.removeFromTop (26).removeFromRight (96));
}

void HistoryGraphComponent::drawGrid (juce::Graphics& g, juce::Rectangle<float> graph)
{
    g.setColour (palette::bgDeep.withAlpha (0.55f));
    g.fillRoundedRectangle (graph, 4.0f);

    g.setFont (ui_.type().labelFont().withHeight (10.0f));

    for (float db : kDbGrid)
    {
        const float y = levelY (db, graph);
        g.setColour (palette::grid.withAlpha (db == 0.0f ? 0.55f : 0.35f));
        g.drawHorizontalLine ((int) std::round (y), graph.getX(), graph.getRight());
        g.setColour (palette::textMuted.withAlpha (0.85f));
        g.drawText (juce::String ((int) db) + " dB",
                    juce::Rectangle<float> (graph.getX() + 5.0f, y - 8.0f, 44.0f, 16.0f),
                    juce::Justification::centredLeft);
    }

    for (double t = 0.5; t <= windowSeconds_ + 0.001; t += 0.5)
    {
        const float x = graph.getRight() - (float) (t / windowSeconds_) * graph.getWidth();
        if (x < graph.getX() || x > graph.getRight())
            continue;

        const bool fullSecond = std::abs (t - std::round (t)) < 0.01;
        g.setColour (palette::grid.withAlpha (fullSecond ? 0.42f : 0.24f));
        g.drawVerticalLine ((int) std::round (x), graph.getY(), graph.getBottom());

        if (fullSecond)
        {
            g.setColour (palette::textMuted.withAlpha (0.8f));
            g.drawText (formatTimeLabel (t),
                        juce::Rectangle<float> (x - 22.0f, graph.getBottom() + 2.0f, 44.0f, 14.0f),
                        juce::Justification::centred);
        }
    }

    const float ceilingY = levelY (processor_.getCeilingDbForGraph(), graph);
    g.setColour (palette::accentBright.withAlpha (0.85f));
    g.drawHorizontalLine ((int) std::round (ceilingY), graph.getX(), graph.getRight());
    g.drawText ("Ceiling",
                juce::Rectangle<float> (graph.getRight() - 58.0f, ceilingY - 16.0f, 54.0f, 14.0f),
                juce::Justification::centredRight);
}

void HistoryGraphComponent::drawTraces (juce::Graphics& g, juce::Rectangle<float> graph)
{
    const int width = juce::jmax (1, (int) std::round (graph.getWidth()));
    const float left = graph.getX();
    const float bottom = graph.getBottom();

    juce::Path outputFill;
    juce::Path grFill;
    juce::Path grStroke;
    juce::Path inputLine;
    bool hasOutput = false;
    bool hasGr = false;
    bool hasInput = false;

    outputFill.startNewSubPath (left, bottom);
    grFill.startNewSubPath (left, graph.getY());

    for (int x = 0; x < width; ++x)
    {
        const auto frame = frameForPixel (x, width);
        const float px = left + (float) x;

        if (! frame.valid)
            continue;

        const float outY = levelY (frame.outDb, graph);
        if (! hasOutput)
        {
            outputFill.lineTo (px, bottom);
            hasOutput = true;
        }
        outputFill.lineTo (px, outY);

        const float grDepthY = grY (frame.grDb, graph);
        if (! hasGr)
        {
            grFill.lineTo (px, graph.getY());
            grStroke.startNewSubPath (px, grDepthY);
            hasGr = true;
        }
        grFill.lineTo (px, grDepthY);
        grStroke.lineTo (px, grDepthY);

        const float inY = levelY (frame.inDb, graph);
        if (! hasInput)
        {
            inputLine.startNewSubPath (px, inY);
            hasInput = true;
        }
        else
        {
            inputLine.lineTo (px, inY);
        }
    }

    if (hasOutput)
    {
        outputFill.lineTo (graph.getRight(), bottom);
        outputFill.closeSubPath();
        g.setColour (palette::accent.withAlpha (0.22f));
        g.fillPath (outputFill);
    }

    if (hasGr)
    {
        grFill.lineTo (graph.getRight(), graph.getY());
        grFill.closeSubPath();
        g.setColour (palette::warning.withAlpha (0.42f));
        g.fillPath (grFill);
        g.setColour (palette::warning.withAlpha (0.95f));
        g.strokePath (grStroke, juce::PathStrokeType (1.5f));
    }

    if (hasInput)
    {
        g.setColour (palette::text.withAlpha (0.35f));
        g.strokePath (inputLine, juce::PathStrokeType (1.0f));
    }

    if (! hasOutput && ! hasGr && ! hasInput)
    {
        g.setFont (ui_.type().labelFont().withHeight (12.0f));
        g.setColour (palette::textMuted);
        g.drawText ("Waiting for audio...", graph, juce::Justification::centred);
    }
}
