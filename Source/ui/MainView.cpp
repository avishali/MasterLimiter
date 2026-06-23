#include "MainView.h"
#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"
#include "ui/meters/ClipBallistics.h"
#include "ui/PresetManager.h"

#include <cmath>
#include <iterator>

#ifndef MASTERLIMITER_GIT_SHA
 #define MASTERLIMITER_GIT_SHA "nogit"
#endif

#ifndef MASTERLIMITER_GIT_BRANCH
 #define MASTERLIMITER_GIT_BRANCH "nogit"
#endif

#ifndef MASTERLIMITER_BUILD_TIMESTAMP
 #define MASTERLIMITER_BUILD_TIMESTAMP "unknown"
#endif

namespace
{
constexpr int kUserPresetIdBase = 1000;
constexpr int kSaveUserPresetId = 9001;
constexpr int kDeleteUserPresetId = 9002;
constexpr int kRevealUserPresetsId = 9003;
constexpr int kLoadUserPresetFromFileId = 9004;

juce::String buildMarkerText()
{
    return "v" + juce::String (JucePlugin_VersionString) + " (beta) - Maximizer - "
           + juce::String (MASTERLIMITER_GIT_BRANCH)
           + "@"
           + juce::String (MASTERLIMITER_GIT_SHA)
           + " - built "
           + juce::String (MASTERLIMITER_BUILD_TIMESTAMP);
}

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

struct LevelScaleAnchor
{
    float db;
    float norm;
};

constexpr LevelScaleAnchor kOzoneFullRangeAnchors[] {
    { -120.0f, 0.00f },
    { -50.0f,  0.10f },
    { -40.0f,  0.25f },
    { -30.0f,  0.42f },
    { -20.0f,  0.59f },
    { -15.0f,  0.68f },
    { -10.0f,  0.77f },
    { -6.0f,   0.84f },
    { -3.0f,   0.89f },
    { 0.0f,    0.94f },
    { 6.0f,    1.00f }
};

float normaliseOzoneFullRangeDb (float db) noexcept
{
    if (! std::isfinite (db))
        db = kOzoneFullRangeAnchors[0].db;

    const float clamped = juce::jlimit (kOzoneFullRangeAnchors[0].db,
                                        kOzoneFullRangeAnchors[std::size (kOzoneFullRangeAnchors) - 1].db,
                                        db);

    for (std::size_t i = 1; i < std::size (kOzoneFullRangeAnchors); ++i)
    {
        const auto& lower = kOzoneFullRangeAnchors[i - 1];
        const auto& upper = kOzoneFullRangeAnchors[i];

        if (clamped <= upper.db)
        {
            const float t = (clamped - lower.db) / (upper.db - lower.db);
            return lower.norm + t * (upper.norm - lower.norm);
        }
    }

    return kOzoneFullRangeAnchors[std::size (kOzoneFullRangeAnchors) - 1].norm;
}

float normaliseMeterScaleDb (float db, MeterGroupComponent::ScaleMode mode) noexcept
{
    if (mode == MeterGroupComponent::ScaleMode::FullRange)
        return normaliseOzoneFullRangeDb (db);

    return mdsp_ui::meters::MeterRenderStateProvider::normaliseDb (db, mode);
}

juce::String pid (std::string_view sv)
{
    return { sv.data(), static_cast<size_t> (sv.size()) };
}

juce::String formatSignedDb1 (float v)
{
    if (! std::isfinite (v))
        v = 0.0f;

    return juce::String (v >= 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
}

juce::String formatPositiveBare (float v)
{
    if (! std::isfinite (v))
        v = 0.0f;

    return juce::String (juce::jmax (0.0f, v), 1);
}

juce::String formatClipReadout (float currentDb, float maxDb)
{
    return "Clip " + formatPositiveBare (currentDb) + " / " + formatPositiveBare (maxDb);
}

juce::String scaleLabel (MeterGroupComponent::ScaleMode mode)
{
    switch (mode)
    {
        case MeterGroupComponent::ScaleMode::FullRange: return "Full";
        case MeterGroupComponent::ScaleMode::Top48Db: return "48";
        case MeterGroupComponent::ScaleMode::Top24Db: return "24";
        case MeterGroupComponent::ScaleMode::Top12Db: return "12";
        case MeterGroupComponent::ScaleMode::Top6Db: return "6";
    }
    return "Full";
}

MeterGroupComponent::ScaleMode scaleFromIndex (int index) noexcept
{
    switch (juce::jlimit (0, 4, index))
    {
        case 1: return MeterGroupComponent::ScaleMode::Top48Db;
        case 2: return MeterGroupComponent::ScaleMode::Top24Db;
        case 3: return MeterGroupComponent::ScaleMode::Top12Db;
        case 4: return MeterGroupComponent::ScaleMode::Top6Db;
        default: return MeterGroupComponent::ScaleMode::FullRange;
    }
}

int scaleIndex (MeterGroupComponent::ScaleMode mode) noexcept
{
    switch (mode)
    {
        case MeterGroupComponent::ScaleMode::FullRange: return 0;
        case MeterGroupComponent::ScaleMode::Top48Db: return 1;
        case MeterGroupComponent::ScaleMode::Top24Db: return 2;
        case MeterGroupComponent::ScaleMode::Top12Db: return 3;
        case MeterGroupComponent::ScaleMode::Top6Db: return 4;
    }
    return 0;
}

void appendScaleTicks (juce::Array<float>& ticks, MeterGroupComponent::ScaleMode mode)
{
    switch (mode)
    {
        case MeterGroupComponent::ScaleMode::FullRange:
            ticks.addArray ({ 0.0f, -3.0f, -6.0f, -10.0f, -15.0f, -20.0f, -30.0f, -40.0f, -50.0f, -120.0f });
            break;
        case MeterGroupComponent::ScaleMode::Top48Db:
            ticks.addArray ({ 0.0f, -12.0f, -24.0f, -36.0f, -48.0f });
            break;
        case MeterGroupComponent::ScaleMode::Top24Db:
            ticks.addArray ({ 0.0f, -3.0f, -6.0f, -9.0f, -12.0f, -15.0f, -18.0f, -24.0f });
            break;
        case MeterGroupComponent::ScaleMode::Top12Db:
            ticks.addArray ({ 0.0f, -3.0f, -6.0f, -9.0f, -12.0f });
            break;
        case MeterGroupComponent::ScaleMode::Top6Db:
            ticks.addArray ({ 0.0f, -1.0f, -2.0f, -3.0f, -4.0f, -5.0f, -6.0f });
            break;
    }
}

juce::String tickLabel (float db)
{
    if (db <= -119.0f)
        return "-inf";
    if (std::abs (db) < 0.001f)
        return "0";
    if (db > 0.0f)
        return "+" + juce::String (db, 0);
    return juce::String (db, 0);
}

} // namespace

void MainView::CompensationBar::setColours (juce::Colour negative, juce::Colour positive, juce::Colour track) noexcept
{
    negative_ = negative;
    positive_ = positive;
    track_ = track;
}

void MainView::CompensationBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
        return;

    const auto track = bounds.withHeight (4.0f).withCentre (bounds.getCentre());
    g.setColour (track_);
    g.fillRoundedRectangle (track, 2.0f);

    const float v = processor_ != nullptr ? juce::jlimit (-12.0f, 12.0f, processor_->getCompGainDb()) : 0.0f;
    const float centre = track.getCentreX();
    const float half = track.getWidth() * 0.5f;
    const float pixels = half * (std::abs (v) / 12.0f);

    if (pixels > 0.5f)
    {
        auto fill = track;
        if (v < 0.0f)
            fill.setBounds (centre - pixels, track.getY(), pixels, track.getHeight());
        else
            fill.setBounds (centre, track.getY(), pixels, track.getHeight());

        g.setColour (v < 0.0f ? negative_ : positive_);
        g.fillRoundedRectangle (fill, 2.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float> { centre - 0.5f, track.getY() - 1.0f, 1.0f, track.getHeight() + 2.0f });
}

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

juce::Rectangle<int> MainView::ValueSlider::getValueLabelBounds() const
{
    auto bounds = getLocalBounds();

    if (valueLabelMode_ == ValueLabelMode::Hidden)
        return {};

    if (valueLabelMode_ == ValueLabelMode::Below)
        return bounds.removeFromBottom (20).reduced (1, 1);

    return bounds.withSizeKeepingCentre (juce::jmin (60, juce::jmax (42, bounds.getWidth() - 26)), 20);
}

void MainView::ValueSlider::paint (juce::Graphics& g)
{
    juce::Slider::paint (g);

    if (valueLabelMode_ != ValueLabelMode::Below)
        return;

    const auto valueArea = getValueLabelBounds();
    g.setColour (findColour (juce::Slider::textBoxTextColourId, true).withAlpha (isEnabled() ? 0.82f : 0.38f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (getTextFromValue (getValue()), valueArea, juce::Justification::centred, true);
}

void MainView::ValueSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown()
        && e.getNumberOfClicks() >= 2
        && (! getValueLabelBounds().isEmpty())
        && getValueLabelBounds().contains (e.getPosition())
        && onValueEditRequest != nullptr)
    {
        onValueEditRequest (*this);
        return;
    }

    juce::Slider::mouseDown (e);
}

void MainView::SegmentedChoice::setOptions (juce::StringArray options)
{
    options_ = options;
    selectedIndex_ = juce::jlimit (0, juce::jmax (0, options_.size() - 1), selectedIndex_);
    repaint();
}

void MainView::SegmentedChoice::setSelectedIndex (int index, juce::NotificationType notification)
{
    const int clamped = juce::jlimit (0, juce::jmax (0, options_.size() - 1), index);
    if (selectedIndex_ == clamped)
        return;

    selectedIndex_ = clamped;
    repaint();

    if (notification != juce::dontSendNotification && onChange != nullptr)
        onChange (selectedIndex_);
}

int MainView::SegmentedChoice::indexAt (juce::Point<int> p) const noexcept
{
    if (options_.isEmpty() || getWidth() <= 0)
        return 0;

    const float segmentW = static_cast<float> (getWidth()) / static_cast<float> (options_.size());
    return juce::jlimit (0, options_.size() - 1, static_cast<int> (std::floor (static_cast<float> (p.x) / segmentW)));
}

void MainView::SegmentedChoice::paint (juce::Graphics& g)
{
    if (options_.isEmpty())
        return;

    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const float radius = 7.0f;
    const auto alpha = isEnabled() ? 1.0f : 0.42f;

    g.setColour (palette::control);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (palette::border);
    g.drawRoundedRectangle (bounds, radius, 1.0f);

    const float segmentW = bounds.getWidth() / static_cast<float> (options_.size());
    auto selected = bounds;
    selected.setX (bounds.getX() + segmentW * static_cast<float> (selectedIndex_));
    selected.setWidth (segmentW);

    juce::ColourGradient selectedGradient (juce::Colour::fromRGB (0x1f, 0x2d, 0x2f),
                                           selected.getCentreX(),
                                           selected.getY(),
                                           juce::Colour::fromRGB (0x18, 0x24, 0x26),
                                           selected.getCentreX(),
                                           selected.getBottom(),
                                           false);
    g.setGradientFill (selectedGradient);
    g.fillRoundedRectangle (selected.reduced (1.0f), radius - 1.0f);
    g.setColour (palette::accent.withAlpha (0.18f * alpha));
    g.drawRoundedRectangle (selected.reduced (1.0f), radius - 1.0f, 1.0f);

    g.setColour (palette::border.withAlpha (0.8f));
    for (int i = 1; i < options_.size(); ++i)
    {
        const float x = bounds.getX() + segmentW * static_cast<float> (i);
        g.drawVerticalLine (static_cast<int> (std::round (x)), bounds.getY() + 3.0f, bounds.getBottom() - 3.0f);
    }

    const auto font = ui_ != nullptr ? ui_->type().labelFont().withHeight (10.5f).boldened()
                                     : juce::Font (juce::FontOptions (10.5f)).boldened();
    g.setFont (font);

    for (int i = 0; i < options_.size(); ++i)
    {
        auto textBounds = bounds;
        textBounds.setX (bounds.getX() + segmentW * static_cast<float> (i));
        textBounds.setWidth (segmentW);
        g.setColour ((i == selectedIndex_ ? palette::text : palette::textMuted).withAlpha (alpha));
        g.drawFittedText (options_[i], textBounds.toNearestInt().reduced (4, 1), juce::Justification::centred, 1);
    }
}

void MainView::SegmentedChoice::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;

    setSelectedIndex (indexAt (e.getPosition()), juce::sendNotificationSync);
}

//==============================================================================
MainView::MainView (mdsp_ui::UiContext& uiContext, MasterLimiterAudioProcessor& processor)
    : ui_ (uiContext),
      processor_ (processor),
      apvts_ (processor.getAPVTS()),
      tooltipWindow_ (this, 650),
      meterIn_ (ui_, processor, MeterGroupComponent::BusKind::Input),
      meterGr_ (ui_, processor),
      meterOut_ (ui_, processor, MeterGroupComponent::BusKind::Output),
      lufsPanel_ (ui_, processor),
      clipBallistics_ (master_limiter_ui::makeClipBallisticsState())
{
    addMouseListener (this, true);

    header_.setJustificationType (juce::Justification::centredLeft);
    header_.setFont (ui_.type().titleFont().withHeight (20.0f).boldened());
    header_.setColour (juce::Label::textColourId, palette::accentBright);
    addAndMakeVisible (header_);

    headerMode_.setJustificationType (juce::Justification::centredLeft);
    headerMode_.setText (buildMarkerText(), juce::dontSendNotification);
    headerMode_.setFont (ui_.type().labelFont().withHeight (9.5f));
    headerMode_.setColour (juce::Label::textColourId, palette::textMuted);
    addAndMakeVisible (headerMode_);

    presetMenu_.setJustificationType (juce::Justification::centredLeft);
    presetMenu_.setColour (juce::ComboBox::backgroundColourId, palette::control);
    presetMenu_.setColour (juce::ComboBox::textColourId, palette::text);
    presetMenu_.setColour (juce::ComboBox::outlineColourId, palette::border);
    presetMenu_.setColour (juce::ComboBox::focusedOutlineColourId, palette::accent.withAlpha (0.8f));
    presetMenu_.setTextWhenNothingSelected ("Presets");
    presetMenu_.onChange = [this] { handlePresetMenuSelection(); };
    refreshPresetMenu();
    addAndMakeVisible (presetMenu_);

    btnSavePreset_.onClick = [this] { showSaveUserPresetDialog(); };
    addAndMakeVisible (btnSavePreset_);
    btnCompareA_.setClickingTogglesState (false);
    btnCompareB_.setClickingTogglesState (false);
    btnCompareA_.onClick = [this]
    {
        processor_.switchCompareSlot (0);
        updateCompareButtons();
    };
    btnCompareB_.onClick = [this]
    {
        processor_.switchCompareSlot (1);
        updateCompareButtons();
    };
    btnCopyCompare_.onClick = [this]
    {
        processor_.copyActiveCompareSlotToOther();
        updateCompareButtons();
    };
    processor_.ensureCompareSlotsInitialized();
    updateCompareButtons();
    addAndMakeVisible (btnCompareA_);
    addAndMakeVisible (btnCompareB_);
    addAndMakeVisible (btnCopyCompare_);

    auto setupLabel = [&] (juce::Label& l)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setFont (ui_.type().labelFont().withHeight (11.0f));
        l.setColour (juce::Label::textColourId, palette::textMuted);
        addAndMakeVisible (l);
    };

    setupLabel (lblGainDrive_);
    setupLabel (lblGainDriveRange_);
    setupLabel (lblClipperDrive_);
    setupLabel (lblClipperReadout_);
    setupLabel (lblCeiling_);
    setupLabel (lblRelease_);
    setupLabel (lblReleaseAuto_);
    setupLabel (lblCeilingMode_);
    setupLabel (lblStereoLink_);
    setupLabel (lblBandColor_);
    setupLabel (lblCharacter_);
    setupLabel (lblGainMatchNote_);
    setupLabel (lblLearnInputLufs_);
    setupLabel (lblIoInputTrim_);
    setupLabel (lblIoInputReadout_);
    setupLabel (lblIoInputReadoutL_);
    setupLabel (lblIoInputReadoutR_);
    setupLabel (lblIoOutputTrim_);
    setupLabel (lblIoOutputReadout_);
    setupLabel (lblIoOutputReadoutL_);
    setupLabel (lblIoOutputReadoutR_);
    setupLabel (lblMeterScaleRange_);
    lblIoInputTrim_.setVisible (false);
    lblIoOutputTrim_.setVisible (false);
    lblIoInputReadout_.setVisible (false);
    lblIoOutputReadout_.setVisible (false);

    styleRotary (sldGainDrive_);
    styleRotary (sldClipperDrive_);
    styleRotary (sldCeiling_);
    styleRotary (sldRelease_);
    styleRotary (sldStereoLink_);
    styleRotary (sldBandColor_);
    styleIoTrimFader (sldIoInputTrimL_);
    styleIoTrimFader (sldIoInputTrimR_);
    styleIoTrimFader (sldIoOutputTrimL_);
    styleIoTrimFader (sldIoOutputTrimR_);

    sldGainDrive_.setRange (0.0, 24.0, 0.1);
    sldGainDrive_.setNumDecimalPlacesToDisplay (1);
    sldGainDrive_.setTextValueSuffix (" dB");
    sldGainDrive_.setValue (0.0, juce::dontSendNotification);

    addAndMakeVisible (sldGainDrive_);
    addAndMakeVisible (sldClipperDrive_);
    btnClipperActive_.setClickingTogglesState (true);
    addAndMakeVisible (btnClipperActive_);
    btnClipperMode_.setClickingTogglesState (false);
    addAndMakeVisible (btnClipperMode_);
    addAndMakeVisible (sldCeiling_);
    addAndMakeVisible (sldRelease_);
    btnReleaseAuto_.setClickingTogglesState (true);
    addAndMakeVisible (btnReleaseAuto_);
    segAutoReleaseMode_.setUiContext (&ui_);
    segAutoReleaseMode_.setOptions (juce::StringArray { "Transparent", "Balanced", "Reactive" });
    addAndMakeVisible (segAutoReleaseMode_);
    btnCeilingMode_.setClickingTogglesState (true);
    addAndMakeVisible (btnCeilingMode_);
    segCharacter_.setUiContext (&ui_);
    segCharacter_.setOptions (juce::StringArray { "Clean", "Tight", "Aggressive" });
    addAndMakeVisible (segCharacter_);
    addAndMakeVisible (sldStereoLink_);
    btnStereoMode_.setClickingTogglesState (false);
    addAndMakeVisible (btnStereoMode_);
    addAndMakeVisible (sldBandColor_);
    lblClipperReadout_.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    lblClipperReadout_.addMouseListener (this, false);

    btnGainCeilingLink_.setClickingTogglesState (true);
    btnGainCeilingLink_.setName ("GainCeilingLinkIcon");
    btnGainCeilingLink_.setButtonText ({});
    btnLimiterActive_.setClickingTogglesState (true);
    btnLimiterActive_.setName ("LimiterPower");
    btnClipperActive_.setName ("LimiterPower");
    btnClipperMode_.setName ("ClipperModeSegment");
    btnCeilingMode_.setName ("CeilingModeSegment");
    btnStereoMode_.setName ("StereoModeSegment");
    btnGainMatchAutoTrack_.setClickingTogglesState (true);
    btnIoInputLink_.setClickingTogglesState (true);
    btnIoInputLink_.setName ("IoInputLinkIcon");
    btnIoInputLink_.setButtonText ({});
    btnIoInputLink_.setToggleState (false, juce::dontSendNotification);
    btnIoOutputLink_.setClickingTogglesState (true);
    btnIoOutputLink_.setName ("IoOutputLinkIcon");
    btnIoOutputLink_.setButtonText ({});
    btnIoOutputLink_.setToggleState (false, juce::dontSendNotification);
    btnBypass_.setClickingTogglesState (true);
    btnBypass_.setName ("BypassButton");
    btnMeterRms_.setClickingTogglesState (true);
    btnMeterRms_.setToggleState (false, juce::dontSendNotification);
    btnMeterScaleMinus_.setName ("MeterScaleMinus");
    btnMeterScalePlus_.setName ("MeterScalePlus");
    compGainBar_.setProcessor (&processor_);
    compGainBar_.setColours (palette::warning.withAlpha (0.85f),
                             palette::accent.withAlpha (0.9f),
                             palette::grid.withAlpha (0.65f));
    addAndMakeVisible (btnGainCeilingLink_);
    addAndMakeVisible (btnLimiterActive_);
    addAndMakeVisible (btnGainMatchAutoTrack_);
    addAndMakeVisible (compGainBar_);
    addAndMakeVisible (btnLearnInputGain_);
    addAndMakeVisible (sldIoInputTrimL_);
    addAndMakeVisible (sldIoInputTrimR_);
    addAndMakeVisible (btnIoInputLink_);
    addAndMakeVisible (sldIoOutputTrimL_);
    addAndMakeVisible (sldIoOutputTrimR_);
    addAndMakeVisible (btnIoOutputLink_);
    addAndMakeVisible (btnBypass_);
    addAndMakeVisible (btnDev_);
    addAndMakeVisible (btnHistory_);
    addAndMakeVisible (btnMeterScaleMinus_);
    addAndMakeVisible (btnMeterScalePlus_);
    addAndMakeVisible (btnMeterRms_);

    attGainDrive_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::input_gain_db), sldGainDrive_);
    attLimiterActive_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::limiter_active), btnLimiterActive_);
    attPluginBypass_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::plugin_bypass), btnBypass_);
    attClipperDrive_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::clipper_drive_db), sldClipperDrive_);
    attClipperActive_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::clipper_active), btnClipperActive_);
    if (auto* clipperModeParam = apvts_.getParameter (pid (param::clipper_mode)))
    {
        attClipperMode_ = std::make_unique<juce::ParameterAttachment> (*clipperModeParam,
                                                                       [this] (float value)
                                                                       {
                                                                           updateClipperModeButton ((int) std::lround (value));
                                                                       },
                                                                       nullptr);
        attClipperMode_->sendInitialUpdate();
    }
    attCeiling_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::ceiling_db), sldCeiling_);
    attGainCeilingLink_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::gain_ceiling_link), btnGainCeilingLink_);
    attRelease_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::release_ms), sldRelease_);
    attReleaseAuto_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::release_auto), btnReleaseAuto_);
    if (auto* autoModeParam = apvts_.getParameter (pid (param::auto_release_mode)))
    {
        attAutoReleaseMode_ = std::make_unique<juce::ParameterAttachment> (*autoModeParam,
                                                                          [this] (float value)
                                                                          {
                                                                              updateAutoReleaseModeControl ((int) std::lround (value));
                                                                          },
                                                                          nullptr);
        attAutoReleaseMode_->sendInitialUpdate();
    }
    attBandColor_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::band_color), sldBandColor_);
    if (auto* characterParam = apvts_.getParameter (pid (param::character)))
    {
        attCharacter_ = std::make_unique<juce::ParameterAttachment> (*characterParam,
                                                                    [this] (float value)
                                                                    {
                                                                        updateCharacterModeControl ((int) std::lround (value));
                                                                    },
                                                                    nullptr);
        attCharacter_->sendInitialUpdate();
    }
    attGainMatchAutoTrack_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::gain_match_auto), btnGainMatchAutoTrack_);
    attIoInputTrimL_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::io_input_l_db), sldIoInputTrimL_);
    attIoInputTrimR_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::io_input_r_db), sldIoInputTrimR_);
    attIoInputLink_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::io_input_link), btnIoInputLink_);
    attIoOutputTrimL_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::io_output_l_db), sldIoOutputTrimL_);
    attIoOutputTrimR_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (param::io_output_r_db), sldIoOutputTrimR_);
    attIoOutputLink_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts_, pid (param::io_output_link), btnIoOutputLink_);

    auto useSliderTextboxFormat = [] (juce::Slider& slider, int numDecimals, const juce::String& suffix)
    {
        slider.textFromValueFunction = nullptr;
        slider.setNumDecimalPlacesToDisplay (numDecimals);
        slider.setTextValueSuffix (suffix);
    };

    useSliderTextboxFormat (sldGainDrive_, 1, " dB");
    useSliderTextboxFormat (sldClipperDrive_, 1, " dB");
    useSliderTextboxFormat (sldCeiling_, 1, " dB");
    useSliderTextboxFormat (sldRelease_, 0, " ms");
    useSliderTextboxFormat (sldStereoLink_, 0, juce::String());
    useSliderTextboxFormat (sldBandColor_, 0, juce::String());
    sldStereoLink_.textFromValueFunction = [] (double v) { return juce::String (v, 0) + "%"; };
    sldBandColor_.textFromValueFunction = [] (double v) { return juce::String (v, 0) + "%"; };
    useSliderTextboxFormat (sldIoInputTrimL_, 2, " dB");
    useSliderTextboxFormat (sldIoInputTrimR_, 2, " dB");
    useSliderTextboxFormat (sldIoOutputTrimL_, 2, " dB");
    useSliderTextboxFormat (sldIoOutputTrimR_, 2, " dB");
    setupValueEdit (sldGainDrive_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldClipperDrive_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldCeiling_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldRelease_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldStereoLink_, ValueSlider::ValueLabelMode::Centre);
    setupValueEdit (sldBandColor_, ValueSlider::ValueLabelMode::Centre);
    sldGainDrive_.setRange (sldGainDrive_.getMinimum(), sldGainDrive_.getMaximum(), 0.1);
    sldClipperDrive_.setRange (sldClipperDrive_.getMinimum(), sldClipperDrive_.getMaximum(), 0.1);
    sldCeiling_.setRange (sldCeiling_.getMinimum(), sldCeiling_.getMaximum(), 0.1);
    setupValueEdit (sldIoInputTrimL_, ValueSlider::ValueLabelMode::Hidden);
    setupValueEdit (sldIoInputTrimR_, ValueSlider::ValueLabelMode::Hidden);
    setupValueEdit (sldIoOutputTrimL_, ValueSlider::ValueLabelMode::Hidden);
    setupValueEdit (sldIoOutputTrimR_, ValueSlider::ValueLabelMode::Hidden);

    segAutoReleaseMode_.onChange = [this] (int index)
    {
        if (attAutoReleaseMode_ != nullptr)
            attAutoReleaseMode_->setValueAsCompleteGesture ((float) juce::jlimit (0, 2, index));
    };
    segCharacter_.onChange = [this] (int index)
    {
        if (attCharacter_ != nullptr)
            attCharacter_->setValueAsCompleteGesture ((float) juce::jlimit (0, 2, index));
    };

    addAndMakeVisible (meterIn_);
    addAndMakeVisible (meterGr_);
    addAndMakeVisible (meterOut_);
    addAndMakeVisible (lufsPanel_);

    lblTruePeak_.setJustificationType (juce::Justification::centredLeft);
    lblTruePeak_.setFont (ui_.type().labelFont().withHeight (11.0f));
    lblTruePeak_.setColour (juce::Label::textColourId, palette::textMuted);
    addAndMakeVisible (lblTruePeak_);
    lblTruePeak_.setVisible (false);

    btnResetPeaks_.onClick = [this]
    {
        resetPeakHolds();
    };
    addAndMakeVisible (btnResetPeaks_);

    btnLimiterActive_.onClick = [this]
    {
        updateLimiterActiveState();
    };
    btnClipperActive_.onClick = [this]
    {
        updateClipperActiveState();
    };
    btnBypass_.onClick = [this]
    {
        updateBypassButtonState();
    };
    btnHistory_.onClick = [this]
    {
        if (onToggleHistoryGraph)
            onToggleHistoryGraph();
    };
    btnDev_.onClick = [this]
    {
        if (onToggleDevControls)
            onToggleDevControls();
    };
    btnMeterScaleMinus_.onClick = [this] { stepMeterScale (-1); };
    btnMeterScalePlus_.onClick = [this] { stepMeterScale (1); };
    btnMeterRms_.onClick = [this] { setMeterShowRms (btnMeterRms_.getToggleState()); };
    btnReleaseAuto_.onClick = [this] { updateReleaseAutoControls(); };
    btnClipperMode_.onClick = [this]
    {
        if (attClipperMode_ != nullptr)
            attClipperMode_->setValueAsCompleteGesture (lastClipperModeIdx_ == 0 ? 1.0f : 0.0f);
    };

    sldIoInputTrimL_.onValueChange = [this] { syncLinkedFaders (sldIoInputTrimL_, sldIoInputTrimR_, btnIoInputLink_); };
    sldIoInputTrimR_.onValueChange = [this] { syncLinkedFaders (sldIoInputTrimR_, sldIoInputTrimL_, btnIoInputLink_); };
    sldIoOutputTrimL_.onValueChange = [this] { syncLinkedFaders (sldIoOutputTrimL_, sldIoOutputTrimR_, btnIoOutputLink_); };
    sldIoOutputTrimR_.onValueChange = [this] { syncLinkedFaders (sldIoOutputTrimR_, sldIoOutputTrimL_, btnIoOutputLink_); };
    btnIoInputLink_.onClick = [this] { syncLinkedFaders (sldIoInputTrimL_, sldIoInputTrimR_, btnIoInputLink_); };
    btnIoOutputLink_.onClick = [this] { syncLinkedFaders (sldIoOutputTrimL_, sldIoOutputTrimR_, btnIoOutputLink_); };
    btnStereoMode_.onClick = [this]
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::stereo_mode))))
        {
            const int nextIdx = c->getIndex() == 0 ? 1 : 0;
            c->beginChangeGesture();
            c->setValueNotifyingHost (nextIdx == 0 ? 0.0f : 1.0f);
            c->endChangeGesture();
            updateStereoModeControls();
        }
    };
    sldBandColor_.onValueChange = [this]
    {
        lblBandColor_.setText ("Color " + juce::String ((int) std::lround (sldBandColor_.getValue())) + "%", juce::dontSendNotification);
    };
    sldBandColor_.onDragEnd = [this]
    {
        constexpr double snapWindowPct = 3.0;
        for (double detent : { 0.0, 50.0, 100.0 })
        {
            if (std::abs (sldBandColor_.getValue() - detent) <= snapWindowPct)
            {
                sldBandColor_.setValue (detent, juce::sendNotificationAsync);
                break;
            }
        }
    };
    btnCeilingMode_.onClick = [this]
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::ceiling_mode))))
        {
            const int nextIdx = c->getIndex() == 0 ? 1 : 0;
            c->beginChangeGesture();
            c->setValueNotifyingHost (nextIdx == 0 ? 0.0f : 1.0f);
            c->endChangeGesture();
            updateCeilingModeButton (nextIdx);
        }
    };
    updateIoTrimReadouts();
    lblBandColor_.setText ("Color " + juce::String ((int) std::lround (sldBandColor_.getValue())) + "%", juce::dontSendNotification);
    updateLimiterActiveState();
    updateClipperActiveState();
    updateBypassButtonState();
    updateReleaseAutoControls (true);
    updateStereoModeControls();
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::ceiling_mode))))
        updateCeilingModeButton (c->getIndex());
    setMeterScaleMode (currentMeterScale_);
    setMeterShowRms (false);

    valueEditor_.setMultiLine (false);
    valueEditor_.setReturnKeyStartsNewLine (false);
    valueEditor_.setSelectAllWhenFocused (true);
    valueEditor_.setJustification (juce::Justification::centred);
    valueEditor_.setFont (ui_.type().labelFont().withHeight (11.0f));
    valueEditor_.setColour (juce::TextEditor::backgroundColourId, palette::control);
    valueEditor_.setColour (juce::TextEditor::outlineColourId, palette::accent.withAlpha (0.75f));
    valueEditor_.setColour (juce::TextEditor::focusedOutlineColourId, palette::accentBright);
    valueEditor_.setColour (juce::TextEditor::textColourId, palette::text);
    valueEditor_.onReturnKey = [this] { finishValueEdit (true); };
    valueEditor_.onEscapeKey = [this] { finishValueEdit (false); };
    valueEditor_.onFocusLost = [this] { finishValueEdit (true); };
    addChildComponent (valueEditor_);

    sldGainDrive_.setTooltip ("Drive into the limiter in dB.");
    btnLimiterActive_.setTooltip ("Turns the limiter section on or off while preserving I/O trims.");
    btnClipperActive_.setTooltip ("Toggle the clipper stage on/off.");
    sldClipperDrive_.setTooltip ("Sets the clipper threshold from -12 to 0 dB.");
    btnClipperMode_.setTooltip ("Toggles the clipper curve between Hard and Soft.");
    lblClipperReadout_.setTooltip ("Shows current and maximum clip reduction in dB; click to reset max.");
    btnGainCeilingLink_.setTooltip ("When enabled, Gain and Ceiling move inversely.");
    btnGainMatchAutoTrack_.setTooltip ("Continuously matches limiter output loudness to the learned reference.");
    btnLearnInputGain_.setTooltip ("Click to learn a 3 s LUFS reference. Right-click to clear it.");
    btnLearnInputGain_.addMouseListener (this, false);
    sldIoInputTrimL_.setTooltip ("Left input trim before the drive stage.");
    sldIoInputTrimR_.setTooltip ("Right input trim before the drive stage.");
    btnIoInputLink_.setTooltip ("When enabled, moving one Input fader mirrors the other.");
    sldIoOutputTrimL_.setTooltip ("Left output trim after the ceiling stage.");
    sldIoOutputTrimR_.setTooltip ("Right output trim after the ceiling stage.");
    btnIoOutputLink_.setTooltip ("When enabled, moving one Output fader mirrors the other.");
    btnBypass_.setTooltip ("Plugin bypass. When on, the limiter is bypassed but I/O trims and Gain-Match still apply.");
    btnSavePreset_.setTooltip ("Save the full current state as a user preset, including DEV controls.");
    btnCompareA_.setTooltip ("A/B compare slot A. Click inactive slot to capture the current slot and load A.");
    btnCompareB_.setTooltip ("A/B compare slot B. Click inactive slot to capture the current slot and load B.");
    btnCopyCompare_.setTooltip ("Copy the active A/B slot into the other slot.");
    btnDev_.setTooltip ("Open the temporary DEV tuning controls panel.");
    btnHistory_.setTooltip ("Open the scrolling gain-reduction and level history graph.");
    btnMeterScaleMinus_.setTooltip ("Zoom the I/O meter range out.");
    btnMeterScalePlus_.setTooltip ("Zoom the I/O meter range in.");
    btnMeterRms_.setTooltip ("Show RMS fill and RMS readout on the I/O meters.");
    sldCeiling_.setTooltip ("Output ceiling in dBFS.");
    sldRelease_.setTooltip ("Limiter release time in milliseconds.");
    btnReleaseAuto_.setTooltip ("When Auto is on, release time follows the program.");
    segAutoReleaseMode_.setTooltip ("Auto release response: Transparent, Balanced, or Reactive.");
    btnCeilingMode_.setTooltip ("Sample peak vs true-peak ceiling enforcement.");
    btnStereoMode_.setTooltip ("Click to toggle whether the wideband link operates in L/R stereo or M/S.");
    sldStereoLink_.setTooltip ("Link amount for the active stereo mode: 100% fully linked, 0% independent.");
    sldBandColor_.setTooltip ("Multiband Color: 0% glued/warm, 50% balanced, 100% open/bright.");
    // TEMP: Character is visually inert while the DEV Attack knob owns attack time.
    lblCharacter_.setEnabled (false);
    segCharacter_.setEnabled (false);
    segCharacter_.setTooltip ("Temporarily inactive while DEV Attack overrides Character.");
    presetMenu_.setTooltip ("Factory and user presets. User presets save/load full state including DEV controls.");
    lblTruePeak_.setTooltip ({});
    btnResetPeaks_.setTooltip ("Resets held peaks, maximum gain reduction, and clip maximums.");

    btnLearnInputGain_.onClick = [this]
    {
        processor_.armLearnReference();
        updateLearnStateDisplay();
    };

    updateLearnStateDisplay();
    updateCompensationReadout();
}

MainView::~MainView()
{
    removeMouseListener (this);
}

void MainView::styleRotary (ValueSlider& s) const
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setScrollWheelEnabled (false);
}

void MainView::styleIoTrimFader (ValueSlider& s) const
{
    s.setSliderStyle (juce::Slider::LinearVertical);
    s.setComponentID ("IoTrimFader");
    s.setSliderSnapsToMousePosition (false);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRange (-24.0, 24.0, 0.01);
    s.setNumDecimalPlacesToDisplay (2);
    s.setTextValueSuffix (" dB");
    s.setValue (0.0, juce::dontSendNotification);
    s.setScrollWheelEnabled (false);
}

void MainView::refreshPresetMenu (const juce::String& preferredUserPresetName)
{
    const juce::ScopedValueSetter<bool> guard (rebuildingPresetMenu_, true);

    if (preferredUserPresetName.isNotEmpty())
        activeUserPresetName_ = preferredUserPresetName;

    presetMenu_.clear (juce::dontSendNotification);
    presetMenu_.addSectionHeading ("Factory");

    for (int i = 0; i < master_limiter_ui::PresetManager::getNumPresets(); ++i)
        presetMenu_.addItem (master_limiter_ui::PresetManager::getPresetName (i), i + 1);

    userPresetFiles_ = master_limiter_ui::PresetManager::listUserPresets();

    if (! userPresetFiles_.isEmpty())
    {
        presetMenu_.addSeparator();
        presetMenu_.addSectionHeading ("User");

        for (int i = 0; i < userPresetFiles_.size(); ++i)
            presetMenu_.addItem (userPresetFiles_.getReference (i).getFileNameWithoutExtension(), kUserPresetIdBase + i);
    }

    presetMenu_.addSeparator();
    presetMenu_.addItem ("Save current as...", kSaveUserPresetId);
    presetMenu_.addItem ("Load from file...", kLoadUserPresetFromFileId);

    int selectedId = juce::jlimit (1, master_limiter_ui::PresetManager::getNumPresets(), activeFactoryPresetId_);
    int activeUserIndex = -1;

    if (activeUserPresetName_.isNotEmpty())
    {
        for (int i = 0; i < userPresetFiles_.size(); ++i)
        {
            if (userPresetFiles_.getReference (i).getFileNameWithoutExtension() == activeUserPresetName_)
            {
                activeUserIndex = i;
                selectedId = kUserPresetIdBase + i;
                break;
            }
        }

        if (activeUserIndex < 0)
            activeUserPresetName_.clear();
    }

    if (activeUserIndex >= 0)
        presetMenu_.addItem ("Delete \"" + activeUserPresetName_ + "\"", kDeleteUserPresetId);

    presetMenu_.addItem ("Reveal presets folder", kRevealUserPresetsId);
    presetMenu_.setSelectedId (selectedId, juce::dontSendNotification);
}

void MainView::handlePresetMenuSelection()
{
    if (rebuildingPresetMenu_)
        return;

    const int id = presetMenu_.getSelectedId();
    const int factoryCount = master_limiter_ui::PresetManager::getNumPresets();

    if (id >= 1 && id <= factoryCount)
    {
        activeFactoryPresetId_ = id;
        activeUserPresetName_.clear();
        processor_.applyPreset (id - 1);
        refreshPresetMenu();
        return;
    }

    if (id >= kUserPresetIdBase && id < kUserPresetIdBase + userPresetFiles_.size())
    {
        const int index = id - kUserPresetIdBase;
        const auto file = userPresetFiles_.getReference (index);

        if (master_limiter_ui::PresetManager::loadUserPreset (apvts_, file))
        {
            activeFactoryPresetId_ = 1;
            activeUserPresetName_ = file.getFileNameWithoutExtension();
        }
        else
        {
            showPresetMessage ("Preset Load Failed", "Could not load \"" + file.getFileName() + "\".");
        }

        refreshPresetMenu();
        return;
    }

    if (id == kSaveUserPresetId)
    {
        refreshPresetMenu();
        showSaveUserPresetDialog();
        return;
    }

    if (id == kDeleteUserPresetId)
    {
        refreshPresetMenu();
        showDeleteUserPresetDialog();
        return;
    }

    if (id == kLoadUserPresetFromFileId)
    {
        refreshPresetMenu();
        showLoadUserPresetFromFileDialog();
        return;
    }

    if (id == kRevealUserPresetsId)
    {
        refreshPresetMenu();
        master_limiter_ui::PresetManager::getUserPresetsDir().revealToUser();
    }
}

void MainView::showSaveUserPresetDialog()
{
    auto alert = std::make_shared<juce::AlertWindow> ("Save User Preset",
                                                      "Save the full current state, including DEV controls.",
                                                      juce::MessageBoxIconType::NoIcon,
                                                      this);
    alert->addTextEditor ("name", makeDefaultUserPresetName(), "Name");
    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    alert->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<MainView> (this), alert] (int result)
        {
            if (safe != nullptr && result == 1)
                safe->saveUserPresetNamed (alert->getTextEditorContents ("name"));
        }));
}

void MainView::saveUserPresetNamed (const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty())
    {
        showPresetMessage ("Preset Not Saved", "Please enter a preset name.");
        refreshPresetMenu();
        return;
    }

    if (! master_limiter_ui::PresetManager::saveUserPreset (apvts_, trimmed))
    {
        showPresetMessage ("Preset Save Failed", "Could not write the preset file.");
        refreshPresetMenu();
        return;
    }

    activeFactoryPresetId_ = 1;
    refreshPresetMenu (trimmed);
}

void MainView::showDeleteUserPresetDialog()
{
    if (activeUserPresetName_.isEmpty())
        return;

    juce::File activeFile;
    for (const auto& file : userPresetFiles_)
    {
        if (file.getFileNameWithoutExtension() == activeUserPresetName_)
        {
            activeFile = file;
            break;
        }
    }

    if (activeFile == juce::File())
        return;

    juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::WarningIcon,
        "Delete User Preset",
        "Delete \"" + activeUserPresetName_ + "\" from disk?",
        "Delete",
        "Cancel",
        this,
        juce::ModalCallbackFunction::create (
            [safe = juce::Component::SafePointer<MainView> (this), activeFile] (int result)
            {
                if (safe == nullptr || result != 1)
                    return;

                if (! master_limiter_ui::PresetManager::deleteUserPreset (activeFile))
                    safe->showPresetMessage ("Preset Delete Failed", "Could not delete \"" + activeFile.getFileName() + "\".");

                safe->activeUserPresetName_.clear();
                safe->activeFactoryPresetId_ = 1;
                safe->refreshPresetMenu();
            }));
}

void MainView::showLoadUserPresetFromFileDialog()
{
    presetFileChooser_ = std::make_unique<juce::FileChooser> ("Load preset",
                                                              master_limiter_ui::PresetManager::getUserPresetsDir(),
                                                              "*.mlpreset");
    presetFileChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                     [safe = juce::Component::SafePointer<MainView> (this)] (const juce::FileChooser& chooser)
                                     {
                                         if (safe == nullptr)
                                             return;

                                         const auto file = chooser.getResult();
                                         if (file == juce::File())
                                             return;

                                         if (master_limiter_ui::PresetManager::loadUserPreset (safe->apvts_, file))
                                         {
                                             safe->activeFactoryPresetId_ = 1;
                                             safe->activeUserPresetName_ = file.getFileNameWithoutExtension();
                                         }
                                         else
                                         {
                                             safe->showPresetMessage ("Preset Load Failed", "Could not load \"" + file.getFileName() + "\".");
                                         }

                                         safe->refreshPresetMenu();
                                     });
}

void MainView::updateCompareButtons()
{
    const int active = processor_.getActiveCompareSlot();
    btnCompareA_.setToggleState (active == 0, juce::dontSendNotification);
    btnCompareB_.setToggleState (active == 1, juce::dontSendNotification);
    btnCopyCompare_.setButtonText (active == 0 ? "A→B" : "B→A");
}

void MainView::showPresetMessage (const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle (title)
                                      .withMessage (message)
                                      .withButton ("OK")
                                      .withAssociatedComponent (this),
                                  nullptr);
}

juce::String MainView::makeDefaultUserPresetName() const
{
    for (int i = 1; i < 1000; ++i)
    {
        const auto candidate = "Voicing " + juce::String (i);
        bool exists = false;

        for (const auto& file : userPresetFiles_)
        {
            if (file.getFileNameWithoutExtension().equalsIgnoreCase (candidate))
            {
                exists = true;
                break;
            }
        }

        if (! exists)
            return candidate;
    }

    return "Voicing";
}

void MainView::setupValueEdit (ValueSlider& s, ValueSlider::ValueLabelMode mode)
{
    s.setValueLabelMode (mode);
    s.onValueEditRequest = [this] (ValueSlider& slider) { beginValueEdit (slider); };
}

void MainView::beginValueEdit (ValueSlider& s)
{
    if (! s.isEnabled())
        return;

    finishValueEdit (false);
    editingSlider_ = &s;
    ignoreNextEditorClickAway_ = true;
    valueEditor_.setText (s.getTextFromValue (s.getValue()), false);
    positionValueEditor();
    valueEditor_.setVisible (true);
    valueEditor_.toFront (true);
    valueEditor_.grabKeyboardFocus();
    valueEditor_.selectAll();
}

void MainView::positionValueEditor()
{
    if (editingSlider_ == nullptr)
        return;

    const auto labelBounds = editingSlider_->getValueLabelBounds()
                                 .translated (editingSlider_->getX(), editingSlider_->getY());
    valueEditor_.setBounds (labelBounds);
}

void MainView::finishValueEdit (bool commit)
{
    if (editingSlider_ == nullptr || finishingValueEdit_)
        return;

    const juce::ScopedValueSetter<bool> guard (finishingValueEdit_, true);
    auto* slider = editingSlider_;
    editingSlider_ = nullptr;

    if (commit)
    {
        const auto parsed = slider->getValueFromText (valueEditor_.getText());
        const auto clamped = juce::jlimit (slider->getMinimum(), slider->getMaximum(), parsed);
        slider->startedDragging();
        slider->setValue (clamped, juce::sendNotificationSync);
        slider->stoppedDragging();
    }

    valueEditor_.setVisible (false);
}

void MainView::syncLinkedFaders (juce::Slider& source, juce::Slider& target, juce::ToggleButton& link)
{
    if (adjustingIoFaders_)
        return;

    const juce::ScopedValueSetter<bool> guard (adjustingIoFaders_, true);

    if (link.getToggleState())
        target.setValue (source.getValue(), juce::sendNotificationSync);

    updateIoTrimReadouts();
}

void MainView::updateIoTrimReadouts()
{
    lblIoInputReadoutL_.setText (juce::String (sldIoInputTrimL_.getValue(), 1), juce::dontSendNotification);
    lblIoInputReadoutR_.setText (juce::String (sldIoInputTrimR_.getValue(), 1), juce::dontSendNotification);
    lblIoOutputReadoutL_.setText (juce::String (sldIoOutputTrimL_.getValue(), 1), juce::dontSendNotification);
    lblIoOutputReadoutR_.setText (juce::String (sldIoOutputTrimR_.getValue(), 1), juce::dontSendNotification);
}

void MainView::updateCeilingModeButton (int ceilingIdx)
{
    const bool isTruePeak = ceilingIdx == 1;
    btnCeilingMode_.setButtonText (isTruePeak ? "TP" : "SP");
    btnCeilingMode_.setToggleState (isTruePeak, juce::dontSendNotification);
}

void MainView::updateStereoModeControls()
{
    int stereoIdx = 0;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::stereo_mode))))
        stereoIdx = c->getIndex();

    const bool msMode = stereoIdx == 1;
    btnStereoMode_.setButtonText (msMode ? "M/S" : "Stereo");
    btnStereoMode_.setToggleState (msMode, juce::dontSendNotification);

    if (stereoIdx == lastStereoModeIdx_)
        return;

    lastStereoModeIdx_ = stereoIdx;
    const auto activeParam = msMode ? param::ms_link_pct : param::stereo_link_pct;
    attLink_.reset();
    attLink_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts_, pid (activeParam), sldStereoLink_);
    repaint (btnStereoMode_.getBounds().getUnion (sldStereoLink_.getBounds()).expanded (8, 8));
}

void MainView::updateClipperModeButton (int clipperIdx)
{
    const bool soft = clipperIdx >= 1;
    lastClipperModeIdx_ = soft ? 1 : 0;
    btnClipperMode_.setButtonText (soft ? "Soft" : "Hard");
    btnClipperMode_.setToggleState (soft, juce::dontSendNotification);
    repaint (btnClipperMode_.getBounds().expanded (4, 4));
}

void MainView::updateClipperActiveState()
{
    const bool active = btnClipperActive_.getToggleState();
    lastClipperActive_ = active;

    lblClipperDrive_.setEnabled (active);
    sldClipperDrive_.setEnabled (active);
    btnClipperMode_.setEnabled (active);

    repaint (lblClipperDrive_.getBounds()
                 .getUnion (sldClipperDrive_.getBounds())
                 .getUnion (btnClipperMode_.getBounds())
                 .getUnion (btnClipperActive_.getBounds())
                 .expanded (8, 8));
}

void MainView::updateCharacterModeControl (int characterIdx)
{
    lastCharacterModeIdx_ = juce::jlimit (0, 2, characterIdx);
    segCharacter_.setSelectedIndex (lastCharacterModeIdx_, juce::dontSendNotification);
    repaint (segCharacter_.getBounds().expanded (4, 4));
}

void MainView::updateAutoReleaseModeControl (int modeIdx)
{
    lastAutoReleaseModeIdx_ = juce::jlimit (0, 2, modeIdx);
    segAutoReleaseMode_.setSelectedIndex (lastAutoReleaseModeIdx_, juce::dontSendNotification);
    repaint (segAutoReleaseMode_.getBounds().expanded (4, 4));
}

void MainView::updateReleaseAutoControls (bool forceRepaint)
{
    const bool autoRelease = btnReleaseAuto_.getToggleState();
    const bool changed = autoRelease != lastReleaseAuto_;
    lastReleaseAuto_ = autoRelease;

    sldRelease_.setEnabled (! autoRelease);
    lblRelease_.setEnabled (! autoRelease);
    segAutoReleaseMode_.setEnabled (autoRelease);

    if (changed || forceRepaint)
        repaint (sldRelease_.getBounds()
                     .getUnion (lblRelease_.getBounds())
                     .getUnion (btnReleaseAuto_.getBounds())
                     .getUnion (segAutoReleaseMode_.getBounds())
                     .expanded (8, 8));
}

void MainView::updateLimiterActiveState()
{
    const bool active = btnLimiterActive_.getToggleState();
    btnLimiterActive_.setButtonText (active ? "Lim On" : "Lim Off");
    lastLimiterActive_ = active;
    repaint (maximizerPanelArea_);
}

void MainView::updateBypassButtonState()
{
    const bool bypassed = btnBypass_.getToggleState();
    btnBypass_.setButtonText (bypassed ? "Bypassed" : "Bypass");
    if (bypassed)
        btnBypass_.setColour (juce::TextButton::buttonColourId, palette::warning.withAlpha (0.85f));
    else
        btnBypass_.removeColour (juce::TextButton::buttonColourId);
    repaint (btnBypass_.getBounds().expanded (4, 4));
}

void MainView::updateLearnStateDisplay()
{
    const float ref = processor_.getLearnedRefLufs();
    const bool hasRef = std::isfinite (ref);

    switch (processor_.getLearnState())
    {
        case MasterLimiterAudioProcessor::LearnState::Armed:
        {
            const auto tick = juce::Time::getMillisecondCounter() % 1000u;
            const float phase = static_cast<float> (tick) / 1000.0f;
            const float alpha = 0.35f + 0.35f * (0.5f + 0.5f * std::sin (phase * juce::MathConstants<float>::twoPi));
            btnLearnInputGain_.setButtonText ("Listening...");
            btnLearnInputGain_.setColour (juce::TextButton::buttonColourId, palette::accent.withAlpha (alpha));
            lblLearnInputLufs_.setText ("Listening...", juce::dontSendNotification);
            break;
        }

        case MasterLimiterAudioProcessor::LearnState::Captured:
            btnLearnInputGain_.setButtonText ("Learned");
            btnLearnInputGain_.setColour (juce::TextButton::buttonColourId, palette::accent.withAlpha (0.55f));
            lblLearnInputLufs_.setText (hasRef ? (juce::String (ref, 1) + " LUFS") : "No ref", juce::dontSendNotification);
            break;

        case MasterLimiterAudioProcessor::LearnState::Idle:
        default:
            btnLearnInputGain_.setButtonText ("Learn");
            btnLearnInputGain_.removeColour (juce::TextButton::buttonColourId);
            lblLearnInputLufs_.setText (hasRef ? (juce::String (ref, 1) + " LUFS") : "No ref", juce::dontSendNotification);
            break;
    }

    repaint (btnLearnInputGain_.getBounds().getUnion (lblLearnInputLufs_.getBounds()).expanded (8, 2));
}

void MainView::updateCompensationReadout()
{
    lblGainMatchNote_.setText (formatSignedDb1 (processor_.getCompGainDb()), juce::dontSendNotification);
    compGainBar_.repaint();
}

void MainView::setMeterScaleMode (MeterGroupComponent::ScaleMode mode)
{
    currentMeterScale_ = mode;
    meterIn_.setScaleMode (currentMeterScale_);
    meterOut_.setScaleMode (currentMeterScale_);
    updateMeterScaleControls();
    repaint (meterScaleColumnArea_.expanded (4, 2));
}

void MainView::stepMeterScale (int delta)
{
    setMeterScaleMode (scaleFromIndex (scaleIndex (currentMeterScale_) + delta));
}

void MainView::updateMeterScaleControls()
{
    const int idx = scaleIndex (currentMeterScale_);
    btnMeterScaleMinus_.setEnabled (idx > 0);
    btnMeterScalePlus_.setEnabled (idx < 4);
    lblMeterScaleRange_.setText (scaleLabel (currentMeterScale_), juce::dontSendNotification);

    btnMeterScaleMinus_.setColour (juce::TextButton::buttonColourId,
                                   (idx > 0 ? palette::control.brighter (0.06f) : palette::panel.darker (0.08f)));
    btnMeterScalePlus_.setColour (juce::TextButton::buttonColourId,
                                  (idx < 4 ? palette::control.brighter (0.06f) : palette::panel.darker (0.08f)));
}

void MainView::setMeterShowRms (bool shouldShow)
{
    btnMeterRms_.setToggleState (shouldShow, juce::dontSendNotification);
    meterIn_.setShowRms (shouldShow);
    meterOut_.setShowRms (shouldShow);

    if (shouldShow)
        btnMeterRms_.setColour (juce::TextButton::buttonColourId, palette::accent.withAlpha (0.48f));
    else
        btnMeterRms_.setColour (juce::TextButton::buttonColourId, palette::control.brighter (0.04f));
}

void MainView::paintMeterScaleColumn (juce::Graphics& g)
{
    if (meterScaleColumnArea_.isEmpty())
        return;

    const auto& type = ui_.type();
    const auto area = meterScaleColumnArea_.toFloat();
    const float yMax = area.getBottom();
    const float h = area.getHeight();
    const float xLeft = area.getX();
    const float xRight = area.getRight();
    const float labelW = juce::jmin (20.0f, area.getWidth() * 0.62f);
    const float labelRight = xLeft + labelW;

    g.setColour (palette::grid.withAlpha (0.22f));
    g.drawVerticalLine (static_cast<int> (std::round (labelRight + 2.0f)), area.getY(), area.getBottom());
    g.setFont (type.labelFont().withHeight (9.0f));

    juce::Array<float> ticks;
    appendScaleTicks (ticks, currentMeterScale_);

    for (const auto db : ticks)
    {
        const float norm = normaliseMeterScaleDb (db, currentMeterScale_);
        const float y = yMax - (norm * h);
        if (y < area.getY() - 0.5f || y > area.getBottom() + 0.5f)
            continue;

        const bool zero = std::abs (db) < 0.001f;
        const auto tickColour = palette::textMuted.withAlpha (zero ? 0.58f : 0.42f);
        g.setColour (tickColour);
        g.drawLine (labelRight + 4.0f,
                    y,
                    xRight - 1.0f,
                    y,
                    zero ? 1.45f : 0.8f);

        g.setColour ((db >= 0.0f ? palette::warning : palette::textMuted).withAlpha (zero ? 0.72f : 0.58f));
        g.drawText (tickLabel (db),
                    juce::Rectangle<float> (xLeft, y - 5.0f, labelW, 10.0f),
                    juce::Justification::centredRight);
    }
}

void MainView::mouseDown (const juce::MouseEvent& e)
{
    if (valueEditor_.isVisible())
    {
        if (ignoreNextEditorClickAway_)
        {
            ignoreNextEditorClickAway_ = false;
        }
        else if (! valueEditor_.getBounds().contains (e.getEventRelativeTo (this).getPosition()))
        {
            finishValueEdit (true);
        }
    }

    const auto eventOnLearn = e.eventComponent == &btnLearnInputGain_
                           || btnLearnInputGain_.getBounds().contains (e.getEventRelativeTo (this).getPosition());
    if (eventOnLearn && e.mods.isRightButtonDown())
    {
        processor_.clearLearnedReference();
        updateLearnStateDisplay();
        updateCompensationReadout();
        return;
    }

    const auto eventOnClipReadout = e.eventComponent == &lblClipperReadout_
                                 || lblClipperReadout_.getBounds().contains (e.getEventRelativeTo (this).getPosition());
    if (! eventOnClipReadout)
        return;

    processor_.resetMaxClip();
    master_limiter_ui::resetClipBallistics (*clipBallistics_);
    clipLedLevel_ = 0.0f;
    lblClipperReadout_.setText (formatClipReadout (master_limiter_ui::getClipReadoutCurrent (*clipBallistics_), 0.0f), juce::dontSendNotification);
    repaint (lblClipperReadout_.getBounds().expanded (12, 2));
}

void MainView::paint (juce::Graphics& g)
{
    const auto& type = ui_.type();
    const auto& m = ui_.metrics();

    g.fillAll (palette::bgDeep);

    if (! headerArea_.isEmpty())
    {
        g.setColour (palette::bgDeep.brighter (0.04f));
        g.fillRect (headerArea_);
    }

    auto drawPanel = [&] (juce::Rectangle<int> area, const juce::String& title)
    {
        if (area.isEmpty())
            return;

        const auto panel = area.toFloat();
        g.setColour (palette::panel);
        g.fillRoundedRectangle (panel, m.rLarge);
        g.setColour (palette::border);
        g.drawRoundedRectangle (panel.reduced (0.5f), m.rLarge, m.strokeThin);

        auto titleArea = area.reduced (18, 10).removeFromTop (18);
        const bool limiterOffTitle = title == "M A X I M I Z E R" && ! btnLimiterActive_.getToggleState();
        g.setColour (limiterOffTitle ? palette::warning.withAlpha (0.78f)
                                      : palette::textMuted.withAlpha (0.8f));
        g.setFont (type.labelFont().withHeight (13.0f).boldened());
        g.drawText (title, titleArea, title == "I/O" ? juce::Justification::centred : juce::Justification::centredLeft, true);
    };

    drawPanel (maximizerPanelArea_, "M A X I M I Z E R");
    drawPanel (ioPanelArea_, "I/O");

    if (! gainMatchLabelArea_.isEmpty())
    {
        g.setColour (palette::textMuted.withAlpha (0.75f));
        g.setFont (type.labelFont().withHeight (12.0f).boldened());
        g.drawText ("G A I N  M A T C H", gainMatchLabelArea_, juce::Justification::centred, true);
    }

    if (! sldBandColor_.getBounds().isEmpty())
    {
        const auto b = sldBandColor_.getBounds();
        const int y = b.getBottom() + 2;
        const int x0 = b.getX() + 8;
        const int x1 = b.getCentreX();
        const int x2 = b.getRight() - 8;

        g.setColour (palette::textMuted.withAlpha (0.55f));
        for (int x : { x0, x1, x2 })
            g.drawVerticalLine (x, (float) y, (float) y + 5.0f);

        g.setFont (type.labelFont().withHeight (9.0f));
        g.drawText ("Glued", x0 - 22, y + 7, 44, 12, juce::Justification::centred, true);
        g.drawText ("Bal", x1 - 18, y + 7, 36, 12, juce::Justification::centred, true);
        g.drawText ("Open", x2 - 22, y + 7, 44, 12, juce::Justification::centred, true);
    }

    paintMeterScaleColumn (g);

    if (clipLedLevel_ > 0.001f)
    {
        const auto led = lblClipperDrive_.getBounds().removeFromRight (12).withSizeKeepingCentre (8, 8).toFloat();
        g.setColour (palette::warning.withAlpha (juce::jlimit (0.0f, 1.0f, clipLedLevel_)));
        g.fillEllipse (led);
    }

    if (! footerArea_.isEmpty())
    {
        g.setColour (palette::bgDeep.brighter (0.02f));
        g.fillRect (footerArea_);
        g.setColour (palette::grid.withAlpha (0.45f));
        auto footerTopLine = footerArea_;
        g.fillRect (footerTopLine.removeFromTop (1));
    }
}

void MainView::resized()
{
    headerArea_ = { 0, 0, 1100, 52 };
    maximizerPanelArea_ = { 16, 64, 736, 500 };
    ioPanelArea_ = { 768, 64, 316, 500 };
    footerArea_ = {};

    header_.setBounds (24, 8, 150, 34);
    headerMode_.setBounds (184, 12, 348, 24);
    presetMenu_.setBounds (542, 12, 160, 28);
    btnSavePreset_.setBounds (710, 12, 48, 28);
    btnCompareA_.setBounds (766, 12, 30, 28);
    btnCompareB_.setBounds (802, 12, 30, 28);
    btnCopyCompare_.setBounds (838, 12, 48, 28);
    btnDev_.setBounds (894, 12, 48, 28);
    btnHistory_.setBounds (950, 12, 56, 28);
    btnBypass_.setBounds (1014, 12, 76, 28);
    btnLimiterActive_.setBounds (232, 126, 34, 34);

    lblGainDrive_.setBounds (48, 116, 140, 18);
    sldGainDrive_.setBounds (40, 134, 156, 136);
    lblGainDriveRange_.setBounds (42, 270, 154, 18);
    btnGainCeilingLink_.setBounds (224, 166, 50, 22);

    lblCeiling_.setBounds (300, 116, 156, 18);
    sldCeiling_.setBounds (300, 134, 156, 136);
    lblCeilingMode_.setBounds (0, 0, 0, 0);
    btnCeilingMode_.setBounds (206, 194, 86, 22);
    lblTruePeak_.setBounds (0, 0, 0, 0);

    lblCharacter_.setBounds (34, 314, 128, 18);
    segCharacter_.setBounds (34, 336, 400, 24);

    const int knobY = 372;
    const int knobW = 78;
    const int knobH = 86;
    lblRelease_.setBounds (62, knobY, knobW, 18);
    sldRelease_.setBounds (62, knobY + 18, knobW, knobH);
    lblReleaseAuto_.setBounds (174, knobY, 86, 18);
    btnReleaseAuto_.setBounds (184, knobY + 22, 76, 24);
    segAutoReleaseMode_.setBounds (146, knobY + 56, 154, 22);
    btnStereoMode_.setBounds (316, knobY + 48, 86, 24);
    lblStereoLink_.setBounds (414, knobY, knobW, 18);
    sldStereoLink_.setBounds (414, knobY + 18, knobW, knobH);
    lblClipperDrive_.setBounds (495, 116, 140, 18);
    btnClipperActive_.setBounds (450, 134, 34, 34);
    sldClipperDrive_.setBounds (495, 134, 140, 120);
    btnClipperMode_.setBounds (515, 260, 100, 22);
    lblClipperReadout_.setBounds (495, 286, 140, 18);
    lblBandColor_.setBounds (526, 314, knobW, 18);
    sldBandColor_.setBounds (526, 332, knobW, knobH);

    btnGainMatchAutoTrack_.setBounds (152, 528, 126, 30);
    gainMatchLabelArea_ = btnGainMatchAutoTrack_.getBounds().translated (8, 0).withY (506).withHeight (18);
    lblGainMatchNote_.setBounds (288, 528, 76, 30);
    compGainBar_.setBounds (372, 540, 48, 8);
    // Slice 11b2.1: Learn sits with Auto/Track so the LUFS feature reads as one group.
    btnLearnInputGain_.setBounds (432, 528, 84, 30);
    lblLearnInputLufs_.setBounds (524, 528, 96, 30);

    meterGr_.setBounds (650, 104, 88, 354);

    meterIn_.setBounds (790, 112, 116, 358);
    meterOut_.setBounds (948, 112, 116, 358);

    lblIoInputTrim_.setBounds (0, 0, 0, 0);
    sldIoInputTrimL_.setBounds (800, 204, 42, 252);
    sldIoInputTrimR_.setBounds (856, 204, 42, 252);
    lblIoInputReadoutL_.setBounds (800, 472, 42, 18);
    lblIoInputReadoutR_.setBounds (856, 472, 42, 18);
    btnIoInputLink_.setBounds (826, 498, 44, 20);
    lblIoInputReadout_.setBounds (0, 0, 0, 0);

    lblIoOutputTrim_.setBounds (0, 0, 0, 0);
    sldIoOutputTrimL_.setBounds (960, 204, 42, 252);
    sldIoOutputTrimR_.setBounds (1014, 204, 42, 252);
    lblIoOutputReadoutL_.setBounds (960, 472, 42, 18);
    lblIoOutputReadoutR_.setBounds (1014, 472, 42, 18);
    btnIoOutputLink_.setBounds (984, 498, 44, 20);
    lblIoOutputReadout_.setBounds (0, 0, 0, 0);

    const auto inputScaleRef = meterIn_.getScaleReferenceBoundsInParent();
    const int scaleX = meterIn_.getRight() + 4;
    const int scaleW = juce::jmax (24, meterOut_.getX() - meterIn_.getRight() - 8);
    meterScaleColumnArea_ = { scaleX, inputScaleRef.getY(), scaleW, inputScaleRef.getHeight() };
    lblMeterScaleRange_.setBounds (scaleX, 474, scaleW, 16);
    btnMeterScaleMinus_.setBounds (scaleX, 492, 17, 17);
    btnMeterScalePlus_.setBounds (scaleX + scaleW - 17, 492, 17, 17);
    btnMeterRms_.setBounds (scaleX, 514, scaleW, 20);

    lufsPanel_.setBounds (790, 536, 160, 68);
    btnResetPeaks_.setBounds (962, 556, 100, 26);

    sldIoInputTrimL_.toFront (false);
    sldIoInputTrimR_.toFront (false);
    sldIoOutputTrimL_.toFront (false);
    sldIoOutputTrimR_.toFront (false);
    btnIoInputLink_.toFront (false);
    btnIoOutputLink_.toFront (false);
    lblIoInputReadout_.toFront (false);
    lblIoOutputReadout_.toFront (false);
    lblIoInputReadoutL_.toFront (false);
    lblIoInputReadoutR_.toFront (false);
    lblIoOutputReadoutL_.toFront (false);
    lblIoOutputReadoutR_.toFront (false);
    btnMeterScaleMinus_.toFront (false);
    btnMeterScalePlus_.toFront (false);
    btnMeterRms_.toFront (false);
    lblMeterScaleRange_.toFront (false);
    btnLimiterActive_.toFront (false);
    btnClipperActive_.toFront (false);
    presetMenu_.toFront (false);
    btnSavePreset_.toFront (false);
    btnCompareA_.toFront (false);
    btnCompareB_.toFront (false);
    btnCopyCompare_.toFront (false);
    btnDev_.toFront (false);
    btnHistory_.toFront (false);
    btnBypass_.toFront (false);
    btnClipperMode_.toFront (false);
    segCharacter_.toFront (false);
    segAutoReleaseMode_.toFront (false);
    compGainBar_.toFront (false);
    valueEditor_.toFront (false);

    meterStripArea_ = meterGr_.getBounds().getUnion (meterIn_.getBounds())
                                      .getUnion (meterOut_.getBounds())
                                      .getUnion (lufsPanel_.getBounds())
                                      .getUnion (meterScaleColumnArea_)
                                      .getUnion (btnMeterScaleMinus_.getBounds())
                                      .getUnion (btnMeterScalePlus_.getBounds())
                                      .getUnion (btnMeterRms_.getBounds())
                                      .getUnion (lblMeterScaleRange_.getBounds());

    positionValueEditor();
}

void MainView::syncMetersFromProcessor()
{
    const double sr = processor_.getSampleRate();
    constexpr float dt = 1.0f / 30.0f;

    meterIn_.sync (sr, dt);
    meterGr_.sync (dt);
    meterOut_.sync (sr, dt);
    lufsPanel_.refresh();
    updateLearnStateDisplay();
    updateCompensationReadout();
    updateBypassButtonState();
    updateReleaseAutoControls();

    int ceilingIdx = 0;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts_.getParameter (pid (param::ceiling_mode))))
        ceilingIdx = c->getIndex();
    updateCeilingModeButton (ceilingIdx);
    updateStereoModeControls();

    if (btnLimiterActive_.getToggleState() != lastLimiterActive_)
        updateLimiterActiveState();
    if (btnClipperActive_.getToggleState() != lastClipperActive_)
        updateClipperActiveState();

    const float clipDb = processor_.getCurrentClipDb();
    const float maxClipDb = processor_.getMaxClipSinceResetDb();
    master_limiter_ui::processClipReadout (*clipBallistics_, clipDb, dt);
    const float prevLed = clipLedLevel_;
    clipLedLevel_ = master_limiter_ui::processClipLed (*clipBallistics_, clipDb > 0.0f, dt);
    lblClipperReadout_.setText (formatClipReadout (master_limiter_ui::getClipReadoutCurrent (*clipBallistics_), maxClipDb), juce::dontSendNotification);
    repaint (lblClipperReadout_.getBounds().expanded (12, 2));
    if (clipLedLevel_ > 0.001f || prevLed > 0.001f)
        repaint (lblClipperDrive_.getBounds().removeFromRight (12));

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
    processor_.resetMaxGr();
    processor_.resetMaxClip();
    master_limiter_ui::resetClipBallistics (*clipBallistics_);
    clipLedLevel_ = 0.0f;
    lufsPanel_.refresh();
}
