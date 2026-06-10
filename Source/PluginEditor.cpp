#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/DevControlsComponent.h"
#include "ui/HistoryGraphComponent.h"
#include "util/PlatformOcclusion.h"

//==============================================================================
MasterLimiterAudioProcessorEditor::HistoryWindow::HistoryWindow (juce::Colour backgroundColour)
    : juce::DocumentWindow ("MasterLimiter History Graph",
                            backgroundColour,
                            juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
{
    setUsingNativeTitleBar (true);
}

void MasterLimiterAudioProcessorEditor::HistoryWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}

MasterLimiterAudioProcessorEditor::DevWindow::DevWindow (juce::Colour backgroundColour)
    : juce::DocumentWindow ("MasterLimiter DEV Controls",
                            backgroundColour,
                            juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
{
    setUsingNativeTitleBar (true);
}

void MasterLimiterAudioProcessorEditor::DevWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}

//==============================================================================
MasterLimiterAudioProcessorEditor::MasterLimiterAudioProcessorEditor (MasterLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      ui_ (mdsp_ui::ThemeVariant::Custom),
      lnf_ (ui_),
      processor_ (p),
      mainView (ui_, p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf_);

    constrainer_.setMinimumSize (958, 540);
    constrainer_.setMaximumSize (4000, 2255);
    constrainer_.setFixedAspectRatio (static_cast<double> (kDesignWidth) / static_cast<double> (kDesignHeight));
    setConstrainer (&constrainer_);

    addAndMakeVisible (mainView);
    mainView.onToggleHistoryGraph = [this] { toggleHistoryGraph(); };
    mainView.onToggleDevControls = [this] { toggleDevControls(); };

    setResizable (true, true);
    setSize (1100, 620);

    startTimerHz (30);
}

MasterLimiterAudioProcessorEditor::~MasterLimiterAudioProcessorEditor()
{
    devWindow_.reset();
    historyWindow_.reset();
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
    const auto scale = static_cast<float> (getWidth()) / static_cast<float> (kDesignWidth);
    mainView.setTransform (juce::AffineTransform::scale (scale));
    mainView.setBounds (0, 0, kDesignWidth, kDesignHeight);
}

void MasterLimiterAudioProcessorEditor::timerCallback()
{
    if (! mdsp::isComponentVisibleOnScreen (*this))
        return;

    mainView.syncMetersFromProcessor();
    mainView.repaintMeterStrip();
}

void MasterLimiterAudioProcessorEditor::toggleHistoryGraph()
{
    if (historyWindow_ != nullptr)
    {
        closeHistoryGraphWindow();
        return;
    }

    auto window = std::make_unique<HistoryWindow> (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    window->onClose = [this] { closeHistoryGraphWindow(); };
    window->setResizable (true, true);
    window->setResizeLimits (520, 240, 2000, 1000);
    window->setContentOwned (new HistoryGraphComponent (processor_, ui_), true);
    window->centreWithSize (940, 460);
    window->setAlwaysOnTop (true);
    window->setVisible (true);
    window->toFront (true);
    historyWindow_ = std::move (window);
}

void MasterLimiterAudioProcessorEditor::closeHistoryGraphWindow()
{
    // Defer deletion when called from HistoryWindow::closeButtonPressed(), so JUCE can
    // finish unwinding its internal button callback before the window is destroyed.
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MasterLimiterAudioProcessorEditor> (this)]
    {
        if (safe != nullptr)
            safe->historyWindow_.reset();
    });
}

void MasterLimiterAudioProcessorEditor::toggleDevControls()
{
    if (devWindow_ != nullptr)
    {
        closeDevControlsWindow();
        return;
    }

    auto window = std::make_unique<DevWindow> (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    window->onClose = [this] { closeDevControlsWindow(); };
    window->setResizable (true, true);
    window->setResizeLimits (420, 420, 900, 1000);
    window->setContentOwned (new DevControlsComponent (processor_, ui_), true);
    window->centreWithSize (520, 560);
    window->setAlwaysOnTop (true);
    window->setVisible (true);
    window->toFront (true);
    devWindow_ = std::move (window);
}

void MasterLimiterAudioProcessorEditor::closeDevControlsWindow()
{
    // Defer deletion when called from DevWindow::closeButtonPressed(), so JUCE can
    // finish unwinding its internal button callback before the window is destroyed.
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MasterLimiterAudioProcessorEditor> (this)]
    {
        if (safe != nullptr)
            safe->devWindow_.reset();
    });
}
