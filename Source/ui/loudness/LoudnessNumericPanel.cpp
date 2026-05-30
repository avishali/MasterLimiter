#include "LoudnessNumericPanel.h"

#include <cmath>

namespace
{
constexpr juce::uint32 kLufsCommitIntervalMs = 200;

namespace palette
{
const juce::Colour panel     = juce::Colour::fromRGB (0x16, 0x1b, 0x22);
const juce::Colour control   = juce::Colour::fromRGB (0x1e, 0x24, 0x2e);
const juce::Colour border    = juce::Colour::fromRGB (0x2c, 0x33, 0x3f);
const juce::Colour text      = juce::Colour::fromRGB (0xe8, 0xed, 0xf3);
const juce::Colour textMuted = juce::Colour::fromRGB (0x8c, 0x97, 0xa6);
} // namespace palette
}

LoudnessNumericPanel::LoudnessNumericPanel (mdsp_ui::UiContext& ui, MasterLimiterAudioProcessor& processor)
    : ui_ (ui),
      processor_ (processor)
{
    mLine_ = "-.-- LUFS";
    sLine_ = "-.-- LUFS";
    iLine_ = "-.-- LUFS";
}

void LoudnessNumericPanel::refresh()
{
    snapshot_ = processor_.getLoudnessAnalyzer().getSnapshot();

    const auto now = juce::Time::getMillisecondCounter();
    if (lastCommitMs_ == 0 || now - lastCommitMs_ >= kLufsCommitIntervalMs)
    {
        lastCommitMs_ = now;
        mLine_ = formatLufs (snapshot_.momentaryLufs) + " LUFS";
        sLine_ = formatLufs (snapshot_.shortTermLufs) + " LUFS";
        iLine_ = formatLufs (snapshot_.integratedLufs) + " LUFS";
        repaint();
    }
}

juce::String LoudnessNumericPanel::formatLufs (float lufs)
{
    if (! std::isfinite (lufs) || lufs <= -100.0f)
        return "-inf";

    return juce::String (lufs, 1);
}

void LoudnessNumericPanel::paint (juce::Graphics& g)
{
    const auto& type = ui_.type();
    const auto& m = ui_.metrics();

    const auto panel = getLocalBounds().toFloat().reduced (0.5f);
    juce::ColourGradient bg (palette::control,
                             panel.getCentreX(),
                             panel.getY(),
                             palette::panel,
                             panel.getCentreX(),
                             panel.getBottom(),
                             false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (panel, 8.0f);

    g.setColour (palette::border);
    g.drawRoundedRectangle (panel, 8.0f, 1.0f);

    auto bounds = getLocalBounds().reduced (m.gap, m.gapSmall);
    const int rowH = juce::jmax (14, bounds.getHeight() / 3);

    auto r0 = bounds.removeFromTop (rowH);
    auto r1 = bounds.removeFromTop (rowH);
    auto r2 = bounds.removeFromTop (rowH);

    auto drawRow = [&] (juce::Rectangle<int> row, const juce::String& label, const juce::String& value)
    {
        auto labelArea = row.removeFromLeft (22);
        g.setFont (type.labelFont().withHeight (10.5f).boldened());
        g.setColour (palette::textMuted);
        g.drawText (label, labelArea, juce::Justification::centredLeft, true);

        g.setFont (type.labelFont().withHeight (12.0f).boldened());
        g.setColour (palette::text);
        g.drawText (value, row, juce::Justification::centredRight, true);
    };

    drawRow (r0, "M", mLine_);
    drawRow (r1, "S", sLine_);
    drawRow (r2, "I", iLine_);
}

void LoudnessNumericPanel::resized() {}
