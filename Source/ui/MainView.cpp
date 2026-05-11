#include "MainView.h"
#include "parameters/ParameterIDs.h"

namespace
{
juce::String pid (std::string_view sv)
{
    return { sv.data(), static_cast<size_t> (sv.size()) };
}

void styleRotary (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    s.setScrollWheelEnabled (false);
}

} // namespace

//==============================================================================
MainView::MainView (mdsp_ui::UiContext& uiContext, juce::AudioProcessorValueTreeState& state)
    : ui_ (uiContext),
      apvts_ (state)
{
    const auto& theme = ui_.theme();

    header_.setJustificationType (juce::Justification::centredLeft);
    header_.setFont (juce::Font (juce::FontOptions().withHeight (18.0f)).boldened());
    header_.setColour (juce::Label::textColourId, theme.text);
    addAndMakeVisible (header_);

    auto setupLabel = [&] (juce::Label& l)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
        l.setColour (juce::Label::textColourId, theme.textMuted);
        addAndMakeVisible (l);
    };

    setupLabel (lblInputGain_);
    setupLabel (lblCeiling_);
    setupLabel (lblRelease_);
    setupLabel (lblReleaseAuto_);
    setupLabel (lblLookahead_);
    setupLabel (lblCeilingMode_);
    setupLabel (lblStereoLink_);
    setupLabel (lblMsLink_);
    setupLabel (lblCharacter_);

    styleRotary (sldInputGain_);
    styleRotary (sldCeiling_);
    styleRotary (sldRelease_);
    styleRotary (sldLookahead_);
    styleRotary (sldStereoLink_);
    styleRotary (sldMsLink_);

    addAndMakeVisible (sldInputGain_);
    addAndMakeVisible (sldCeiling_);
    addAndMakeVisible (sldRelease_);
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
    attReleaseAuto_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::release_auto), btnReleaseAuto_);
    attLookahead_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::lookahead_ms), sldLookahead_);
    attCeilingMode_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::ceiling_mode), cmbCeilingMode_);
    attStereoLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::stereo_link_pct), sldStereoLink_);
    attMsLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::ms_link_pct), sldMsLink_);
    attCharacter_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts_, pid (param::character), cmbCharacter_);
}

void MainView::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    header_.setBounds (area.removeFromTop (26));
    area.removeFromTop (6);

    const int labelH = 16;
    const int colGap = 6;
    const int rowGap = 8;
    const int nCols = 5;
    const int colW = (area.getWidth() - colGap * (nCols - 1)) / nCols;

    auto placeCell = [&] (juce::Rectangle<int> cell, juce::Label& lbl, juce::Component& ctrl)
    {
        lbl.setBounds (cell.removeFromTop (labelH));
        cell.removeFromTop (2);
        ctrl.setBounds (cell);
    };

    auto row1 = area.removeFromTop (130);
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

    area.removeFromTop (rowGap);

    auto row2 = area;
    x = row2.getX();
    const int row2H = juce::jmax (80, row2.getHeight());

    placeCell ({ x, row2.getY(), colW, row2H }, lblCeilingMode_, cmbCeilingMode_);
    x += colW + colGap;
    placeCell ({ x, row2.getY(), colW, row2H }, lblStereoLink_, sldStereoLink_);
    x += colW + colGap;
    placeCell ({ x, row2.getY(), colW, row2H }, lblMsLink_, sldMsLink_);
    x += colW + colGap;
    placeCell ({ x, row2.getY(), colW, row2H }, lblCharacter_, cmbCharacter_);
}
