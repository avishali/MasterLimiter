#include "MainView.h"
#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"

#include <cmath>

namespace
{
juce::String pid (std::string_view sv)
{
    return { sv.data(), static_cast<size_t> (sv.size()) };
}
} // namespace

void MainView::TpReadoutSmoother::reset() noexcept
{
    held = duty = -200.0f;
    holdTicksLeft = 0;
}

void MainView::TpReadoutSmoother::tick (float raw, float dtSec, int holdTicksAt30Hz) noexcept
{
    if (! std::isfinite (raw))
        raw = -200.0f;

    if (raw > held)
    {
        held = raw;
        duty = raw;
        holdTicksLeft = holdTicksAt30Hz;
        return;
    }

    if (holdTicksLeft > 0)
    {
        --holdTicksLeft;
        duty = held;
        return;
    }

    const float tau = 0.3f;
    const float a = 1.0f - std::exp (-dtSec / tau);
    held += (raw - held) * a;
    duty = held;
}

juce::String MainView::TpReadoutSmoother::formatDb (float v) noexcept
{
    if (! std::isfinite (v) || v <= -100.0f)
        return "-inf dB";

    return juce::String (v, 1) + " dB";
}

//==============================================================================
MainView::MainView (mdsp_ui::UiContext& uiContext, MasterLimiterAudioProcessor& processor)
    : ui_ (uiContext),
      processor_ (processor),
      apvts_ (processor.getAPVTS()),
      meterIn_ (ui_, processor, MeterGroupComponent::BusKind::Input),
      meterGr_ (ui_, processor, MeterGroupComponent::BusKind::GainReduction),
      meterOut_ (ui_, processor, MeterGroupComponent::BusKind::Output),
      lufsPanel_ (ui_, processor)
{
    const auto& theme = ui_.theme();

    header_.setJustificationType (juce::Justification::centredLeft);
    header_.setFont (ui_.type().titleFont().withHeight (20.0f).boldened());
    header_.setColour (juce::Label::textColourId, theme.text);
    addAndMakeVisible (header_);

    auto setupLabel = [&] (juce::Label& l)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setFont (ui_.type().labelFont().withHeight (11.0f));
        l.setColour (juce::Label::textColourId, theme.textMuted);
        addAndMakeVisible (l);
    };

    setupLabel (lblInputGain_);
    setupLabel (lblCeiling_);
    setupLabel (lblRelease_);
    setupLabel (lblReleaseSustain_);
    setupLabel (lblReleaseAuto_);
    setupLabel (lblLookahead_);
    setupLabel (lblCeilingMode_);
    setupLabel (lblStereoLink_);
    setupLabel (lblMsLink_);
    setupLabel (lblCharacter_);

    styleRotary (sldInputGain_);
    styleRotary (sldCeiling_);
    styleRotary (sldRelease_);
    styleRotary (sldReleaseSustain_);
    styleRotary (sldLookahead_);
    styleRotary (sldStereoLink_);
    styleRotary (sldMsLink_);

    addAndMakeVisible (sldInputGain_);
    addAndMakeVisible (sldCeiling_);
    addAndMakeVisible (sldRelease_);
    addAndMakeVisible (sldReleaseSustain_);
    btnReleaseAuto_.setClickingTogglesState (true);
    addAndMakeVisible (btnReleaseAuto_);
    addAndMakeVisible (sldLookahead_);
    addAndMakeVisible (cmbCeilingMode_);
    addAndMakeVisible (cmbCharacter_);
    addAndMakeVisible (sldStereoLink_);
    addAndMakeVisible (sldMsLink_);

    if (auto* p = apvts_.getParameter (pid (param::ceiling_mode)))
        cmbCeilingMode_.addItemList (p->getAllValueStrings(), 1);

    if (auto* p = apvts_.getParameter (pid (param::character)))
        cmbCharacter_.addItemList (p->getAllValueStrings(), 1);

    attInputGain_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::input_gain_db), sldInputGain_);
    attCeiling_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::ceiling_db), sldCeiling_);
    attRelease_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::release_ms), sldRelease_);
    attReleaseSustain_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::release_sustain_ratio), sldReleaseSustain_);
    attReleaseAuto_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::release_auto), btnReleaseAuto_);
    attLookahead_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::lookahead_ms), sldLookahead_);
    attCeilingMode_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::ceiling_mode), cmbCeilingMode_);
    attStereoLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::stereo_link_pct), sldStereoLink_);
    attMsLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::ms_link_pct), sldMsLink_);
    attCharacter_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::character), cmbCharacter_);

    addAndMakeVisible (meterIn_);
    addAndMakeVisible (meterGr_);
    addAndMakeVisible (meterOut_);
    addAndMakeVisible (lufsPanel_);

    lblTruePeak_.setJustificationType (juce::Justification::centredLeft);
    lblTruePeak_.setFont (ui_.type().labelFont().withHeight (11.0f));
    lblTruePeak_.setColour (juce::Label::textColourId, theme.textMuted);
    addAndMakeVisible (lblTruePeak_);

    btnResetPeaks_.onClick = [this]
    {
        resetPeakHolds();
    };
    addAndMakeVisible (btnResetPeaks_);

    sldInputGain_.setTooltip ("Input gain applied before limiting.");
    sldCeiling_.setTooltip ("Output ceiling in dBFS.");
    sldRelease_.setTooltip ("Limiter release time in milliseconds.");
    sldReleaseSustain_.setTooltip ("Multiplier on release time for sustained peaks (1–10×).");
    btnReleaseAuto_.setTooltip ("When Auto is on, release time follows the program.");
    sldLookahead_.setTooltip ("Lookahead delay in milliseconds (higher catches peaks earlier).");
    cmbCeilingMode_.setTooltip ("Sample peak vs true-peak ceiling enforcement.");
    sldStereoLink_.setTooltip ("Stereo linking between channels (0–100%).");
    sldMsLink_.setTooltip ("Mid/side stereo link amount (0–100%).");
    cmbCharacter_.setTooltip ("Character / color mode (Clean in v0.1).");
}

MainView::~MainView() = default;

void MainView::styleRotary (juce::Slider& s) const
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    s.setScrollWheelEnabled (false);
}

void MainView::paint (juce::Graphics& g)
{
    if (! vSeparatorBounds_.isEmpty())
    {
        g.setColour (ui_.theme().grid.withAlpha (0.45f));
        g.fillRect (vSeparatorBounds_);
    }
}

void MainView::resized()
{
    const auto& m = ui_.metrics();
    auto bounds = getLocalBounds().reduced (static_cast<int> (m.pad), 10);

    auto headerBar = bounds.removeFromTop (40);
    header_.setBounds (headerBar.reduced (4, 6));

    const int stripW = 280;
    auto meterCol = bounds.removeFromRight (stripW);
    bounds.removeFromRight (static_cast<int> (m.padSmall));
    vSeparatorBounds_ = bounds.removeFromRight (1);
    bounds.removeFromRight (static_cast<int> (m.padSmall));
    meterStripArea_ = meterCol;

    auto strip = meterCol.reduced (static_cast<int> (m.padSmall), static_cast<int> (m.padSmall));
    const int gapY = 6;
    const int lufsH = 92;
    const int tpH = 22;
    const int btnH = 30;
    const int minMeterH = 72;
    const int fixedBelow = lufsH + tpH + btnH + gapY * 4;
    const int metersTotal = juce::jmax (minMeterH * 3 + gapY * 2, strip.getHeight() - fixedBelow);
    const int oneMeterH = (metersTotal - gapY * 2) / 3;

    meterIn_.setBounds (strip.removeFromTop (oneMeterH));
    strip.removeFromTop (gapY);
    meterGr_.setBounds (strip.removeFromTop (oneMeterH));
    strip.removeFromTop (gapY);
    meterOut_.setBounds (strip.removeFromTop (oneMeterH));
    strip.removeFromTop (gapY);
    lufsPanel_.setBounds (strip.removeFromTop (lufsH));
    strip.removeFromTop (gapY);
    lblTruePeak_.setBounds (strip.removeFromTop (tpH));
    strip.removeFromTop (gapY);
    btnResetPeaks_.setBounds (strip.removeFromTop (btnH));

    const int labelH = 18;
    const int colGap = static_cast<int> (m.padSmall);
    const int rowGap = 10;
    const int nCols = 5;
    const int colW = (bounds.getWidth() - colGap * (nCols - 1)) / nCols;

    auto placeCell = [&] (juce::Rectangle<int> cell, juce::Label& lbl, juce::Component& ctrl)
    {
        lbl.setBounds (cell.removeFromTop (labelH));
        cell.removeFromTop (3);
        ctrl.setBounds (cell);
    };

    auto row1 = bounds.removeFromTop (128);
    int x = row1.getX();

    placeCell ({ x, row1.getY(), colW, row1.getHeight() }, lblInputGain_, sldInputGain_);
    x += colW + colGap;
    placeCell ({ x, row1.getY(), colW, row1.getHeight() }, lblCeiling_, sldCeiling_);
    x += colW + colGap;
    placeCell ({ x, row1.getY(), colW, row1.getHeight() }, lblRelease_, sldRelease_);
    x += colW + colGap;
    placeCell ({ x, row1.getY(), colW, row1.getHeight() }, lblReleaseAuto_, btnReleaseAuto_);
    x += colW + colGap;
    placeCell ({ x, row1.getY(), colW, row1.getHeight() }, lblLookahead_, sldLookahead_);

    bounds.removeFromTop (rowGap);

    auto row2 = bounds.removeFromTop (128);
    x = row2.getX();

    placeCell ({ x, row2.getY(), colW, row2.getHeight() }, lblCeilingMode_, cmbCeilingMode_);
    x += colW + colGap;
    placeCell ({ x, row2.getY(), colW, row2.getHeight() }, lblStereoLink_, sldStereoLink_);
    x += colW + colGap;
    placeCell ({ x, row2.getY(), colW, row2.getHeight() }, lblMsLink_, sldMsLink_);
    x += colW + colGap;
    placeCell ({ x, row2.getY(), colW, row2.getHeight() }, lblCharacter_, cmbCharacter_);
    x += colW + colGap;
    placeCell ({ x, row2.getY(), colW, row2.getHeight() }, lblReleaseSustain_, sldReleaseSustain_);
}

void MainView::syncMetersFromProcessor()
{
    const double sr = processor_.getSampleRate();
    constexpr float dt = 1.0f / 30.0f;

    meterIn_.sync (sr, dt);
    meterGr_.sync (sr, dt);
    meterOut_.sync (sr, dt);
    lufsPanel_.refresh();

    int ceilingIdx = 0;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::ceiling_mode))))
        ceilingIdx = c->getIndex();

    const auto tpTag = (ceilingIdx == 1 ? juce::String ("TP") : juce::String ("SP"));
    const float tpDb = processor_.getOutputTpDb();
    tpSmoother_.tick (tpDb, dt, 30);
    lblTruePeak_.setText (tpTag + ": " + TpReadoutSmoother::formatDb (tpSmoother_.duty), juce::dontSendNotification);
}

void MainView::repaintMeterStrip()
{
    if (! meterStripArea_.isEmpty())
        repaint (meterStripArea_);
}

void MainView::resetPeakHolds()
{
    processor_.getLoudnessAnalyzer().resetPeak();
    meterIn_.resetPeakHolds();
    meterGr_.resetPeakHolds();
    meterOut_.resetPeakHolds();
    tpSmoother_.reset();
    lufsPanel_.refresh();
}
