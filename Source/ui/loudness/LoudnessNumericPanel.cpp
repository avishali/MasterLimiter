#include "LoudnessNumericPanel.h"

#include <cmath>

namespace
{
constexpr juce::uint32 kLufsCommitIntervalMs = 200;
}

LoudnessNumericPanel::LoudnessNumericPanel (mdsp_ui::UiContext& ui, MasterLimiterAudioProcessor& processor)
    : ui_ (ui),
      processor_ (processor)
{
    mLine_ = juce::String ("M: ") + "-.-- LUFS";
    sLine_ = juce::String ("S: ") + "-.-- LUFS";
    iLine_ = juce::String ("I: ") + "-.-- LUFS";
}

void LoudnessNumericPanel::refresh()
{
    snapshot_ = processor_.getLoudnessAnalyzer().getSnapshot();

    const auto now = juce::Time::getMillisecondCounter();
    if (lastCommitMs_ == 0 || now - lastCommitMs_ >= kLufsCommitIntervalMs)
    {
        lastCommitMs_ = now;
        mLine_ = juce::String ("M: ") + formatLufs (snapshot_.momentaryLufs) + " LUFS";
        sLine_ = juce::String ("S: ") + formatLufs (snapshot_.shortTermLufs) + " LUFS";
        iLine_ = juce::String ("I: ") + formatLufs (snapshot_.integratedLufs) + " LUFS";
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
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    const auto& m = ui_.metrics();

    g.setColour (theme.background.brighter (0.02f));
    g.fillAll();

    g.setColour (theme.borderDivider);
    g.drawRect (getLocalBounds());

    auto bounds = getLocalBounds().reduced (m.pad);
    const int rowH = juce::jmax (14, bounds.getHeight() / 3);

    g.setFont (type.labelFont().withHeight (13.0f));
    g.setColour (theme.text);

    auto r0 = bounds.removeFromTop (rowH);
    auto r1 = bounds.removeFromTop (rowH);
    auto r2 = bounds.removeFromTop (rowH);

    g.drawText (mLine_, r0, juce::Justification::centredLeft, true);
    g.drawText (sLine_, r1, juce::Justification::centredLeft, true);
    g.drawText (iLine_, r2, juce::Justification::centredLeft, true);
}

void LoudnessNumericPanel::resized() {}
