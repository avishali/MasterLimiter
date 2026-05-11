#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MasterLimiterAudioProcessorEditor::MasterLimiterAudioProcessorEditor (MasterLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      audioProcessor (p),
      ui_ (mdsp_ui::ThemeVariant::Custom),
      lnf_ (ui_),
      mainView (ui_, p.getAPVTS())
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf_);

    addAndMakeVisible (mainView);

    setResizable (false, false);
    setSize (720, 360);
}

MasterLimiterAudioProcessorEditor::~MasterLimiterAudioProcessorEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void MasterLimiterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MasterLimiterAudioProcessorEditor::resized()
{
    mainView.setBounds (getLocalBounds());
}
