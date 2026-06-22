#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/HistoryGraphComponent.h"
#include "util/PlatformOcclusion.h"

namespace
{
const juce::Colour kDevPanelBg = juce::Colour::fromRGB (0x0d, 0x10, 0x15);
const juce::Colour kDevPanelBorder = juce::Colour::fromRGB (0x2c, 0x33, 0x3f);
const juce::Colour kDevPanelTitle = juce::Colour::fromRGB (0xe8, 0x70, 0x4f);
} // namespace

//==============================================================================
MasterLimiterAudioProcessorEditor::DevPanel::DevPanel (MasterLimiterAudioProcessor& processor,
                                                       mdsp_ui::UiContext& uiContext)
    : devControls_ (processor, uiContext)
{
    title_.setJustificationType (juce::Justification::centredLeft);
    title_.setFont (uiContext.type().labelFont().withHeight (13.0f).boldened());
    title_.setColour (juce::Label::textColourId, kDevPanelTitle.withAlpha (0.95f));
    addAndMakeVisible (title_);

    closeButton_.onClick = [this]
    {
        if (onClose != nullptr)
            onClose();
    };
    addAndMakeVisible (closeButton_);
    addAndMakeVisible (devControls_);
}

void MasterLimiterAudioProcessorEditor::DevPanel::paint (juce::Graphics& g)
{
    g.fillAll (kDevPanelBg);

    auto bounds = getLocalBounds();
    g.setColour (kDevPanelBorder);
    g.drawVerticalLine (0, 0.0f, static_cast<float> (bounds.getHeight()));
}

void MasterLimiterAudioProcessorEditor::DevPanel::resized()
{
    auto area = getLocalBounds().reduced (2);
    auto header = area.removeFromTop (36).reduced (14, 6);
    closeButton_.setBounds (header.removeFromRight (28).withSizeKeepingCentre (24, 24));
    title_.setBounds (header);
    devControls_.setBounds (area);
}

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

//==============================================================================
MasterLimiterAudioProcessorEditor::MasterLimiterAudioProcessorEditor (MasterLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      ui_ (mdsp_ui::ThemeVariant::Custom),
      lnf_ (ui_),
      processor_ (p),
      mainView (ui_, p),
      devPanel_ (p, ui_)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf_);

    constrainer_.setMinimumSize (kMinBaseWidth, kMinBaseHeight);
    constrainer_.setMaximumSize (kMaxBaseWidth, kMaxBaseHeight);
    constrainer_.setFixedAspectRatio (static_cast<double> (kDesignWidth) / static_cast<double> (kDesignHeight));
    setConstrainer (&constrainer_);

    addAndMakeVisible (mainView);
    mainView.onToggleHistoryGraph = [this] { toggleHistoryGraph(); };
    mainView.onToggleDevControls = [this] { toggleDevControls(); };

    devPanel_.onClose = [this] { toggleDevControls(); };
    addChildComponent (devPanel_);

    setResizable (true, true);
    setSize (1100, 620);

    startTimerHz (30);
}

MasterLimiterAudioProcessorEditor::~MasterLimiterAudioProcessorEditor()
{
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
    const int dockW = devPanel_.isVisible() ? kDevDockWidth : 0;
    const int baseW = getWidth() - dockW;
    const auto scale = static_cast<float> (baseW) / static_cast<float> (kDesignWidth);
    mainView.setTransform (juce::AffineTransform::scale (scale));
    mainView.setBounds (0, 0, kDesignWidth, kDesignHeight);

    if (dockW > 0)
        devPanel_.setBounds (baseW, 0, dockW, getHeight());
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

void MasterLimiterAudioProcessorEditor::updateConstrainer()
{
    const bool devOpen = devPanel_.isVisible();
    const int minW = devOpen ? (kMinBaseWidth + kDevDockWidth) : kMinBaseWidth;
    const int maxW = devOpen ? (kMaxBaseWidth + kDevDockWidth) : kMaxBaseWidth;

    constrainer_.setMinimumSize (minW, kMinBaseHeight);
    constrainer_.setMaximumSize (maxW, kMaxBaseHeight);

    if (devOpen)
        constrainer_.setFixedAspectRatio (0.0);
    else
        constrainer_.setFixedAspectRatio (static_cast<double> (kDesignWidth) / static_cast<double> (kDesignHeight));
}

void MasterLimiterAudioProcessorEditor::toggleDevControls()
{
    const bool show = ! devPanel_.isVisible();

    if (show)
    {
        devPanel_.setVisible (true);
        updateConstrainer();
        setSize (getWidth() + kDevDockWidth, getHeight());
        resized();
        devPanel_.toFront (false);
    }
    else
    {
        devPanel_.setVisible (false);
        const int newW = juce::jmax (kMinBaseWidth, getWidth() - kDevDockWidth);
        updateConstrainer();
        setSize (newW, getHeight());
        resized();
    }
}
