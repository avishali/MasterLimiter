#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "util/PlatformOcclusion.h"

//==============================================================================
MasterLimiterAudioProcessorEditor::MasterLimiterAudioProcessorEditor (MasterLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      ui_ (mdsp_ui::ThemeVariant::Custom),
      lnf_ (ui_),
      mainView (ui_, p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf_);

    constrainer_.setMinimumSize (960, 540);
    constrainer_.setMaximumSize (4000, 3000);
    setConstrainer (&constrainer_);

    addAndMakeVisible (mainView);

    setResizable (true, true);
    setSize (1100, 620);

    startTimerHz (30);
}

MasterLimiterAudioProcessorEditor::~MasterLimiterAudioProcessorEditor()
{
    stopTimer();
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

void MasterLimiterAudioProcessorEditor::timerCallback()
{
    if (! mdsp::isComponentVisibleOnScreen (*this))
        return;

    mainView.syncMetersFromProcessor();
    mainView.repaintMeterStrip();
}
