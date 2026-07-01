#include "MeterComponent.h"

#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/meters/MeterRenderStateProvider.h>

#include <cmath>
#include <iterator>
#include <utility>

namespace
{
constexpr float kDbScaleFontHeight = 10.0f;
constexpr float kDbScaleLineThin = 1.25f;
constexpr float kDbScaleLine0Db = 1.75f;
constexpr float kDbScaleLineDense = 0.75f;

struct LevelScaleAnchor
{
    float db;
    float norm;
};

// Ozone-style FullRange tick anchors for the painted dB scale. Bar peak mapping uses the
// same table via dbToNormForScale(); shared MeterRenderStateProvider::normaliseDb uses a
// different perceptual anchor set — unification deferred to avoid scale/bar regression.
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

static bool nearTick (float value, float target) noexcept
{
    return std::abs (value - target) < 0.001f;
}

float normaliseFromAnchors (float db) noexcept
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
} // namespace

MeterComponent::MeterComponent (mdsp_ui::UiContext& ui, juce::String labelText)
    : ui_ (ui),
      label_ (std::move (labelText))
{
    setOpaque (false);
}

void MeterComponent::setKind (Kind kind) noexcept
{
    if (kind_ == kind)
        return;

    kind_ = kind;
    repaint();
}

void MeterComponent::setLabelText (juce::String labelText)
{
    if (label_ == labelText)
        return;

    label_ = std::move (labelText);
    repaint();
}

void MeterComponent::setRenderState (const MeterRenderState& state)
{
    renderState_ = state;
    numericTextPeak_ = renderState_.peakText.data();
    numericTextRms_ = renderState_.rmsText.data();
    repaint();
}

void MeterComponent::setRangeDb (float minDb, float maxDb) noexcept
{
    grMinDb_ = minDb;
    grMaxDb_ = juce::jmax (minDb + 0.001f, maxDb);
}

void MeterComponent::setDrawInternalScale (bool shouldDraw) noexcept
{
    if (drawInternalScale_ == shouldDraw)
        return;

    drawInternalScale_ = shouldDraw;
    repaint();
}

void MeterComponent::setShowRms (bool shouldShow) noexcept
{
    if (showRms_ == shouldShow)
        return;

    showRms_ = shouldShow;
    repaint();
}

void MeterComponent::setGrDb (float db) noexcept
{
    setStereoGrDb (db, db);
}

void MeterComponent::setStereoGrDb (float l, float r) noexcept
{
    if (! std::isfinite (l))
        l = 0.0f;
    if (! std::isfinite (r))
        r = 0.0f;

    grDbL_ = juce::jmax (0.0f, l);
    grDbR_ = juce::jmax (0.0f, r);
    repaint();
}

void MeterComponent::setGrSingleBarMode (bool single) noexcept
{
    grSingleBar_ = single;
    repaint();
}

void MeterComponent::setNumericReadoutOverride (bool active, juce::String line1, juce::String line2) noexcept
{
    numericOverrideActive_ = active;
    numericOverridePeak_ = std::move (line1);
    numericOverrideMax_ = {};
    numericOverrideRms_ = std::move (line2);
    repaint();
}

void MeterComponent::setNumericReadoutOverride (bool active, juce::String peak, juce::String maxPeak, juce::String rms) noexcept
{
    numericOverrideActive_ = active;
    numericOverridePeak_ = std::move (peak);
    numericOverrideMax_ = std::move (maxPeak);
    numericOverrideRms_ = std::move (rms);
    repaint();
}

void MeterComponent::setTruePeakReadout (bool active, juce::String text, bool over) noexcept
{
    truePeakReadoutActive_ = active;
    truePeakReadoutText_ = std::move (text);
    truePeakReadoutOver_ = over;
    repaint();
}

void MeterComponent::setExternalReadoutCaptions (bool external) noexcept
{
    if (externalReadoutCaptions_ == external)
        return;

    externalReadoutCaptions_ = external;
    repaint();
}

void MeterComponent::setReadoutAlignTowardCenter (bool fromRight) noexcept
{
    if (readoutAlignFromRight_ == fromRight)
        return;

    readoutAlignFromRight_ = fromRight;
    repaint();
}

void MeterComponent::setClipResetCallback (Callback cb, void* ctx) noexcept
{
    onClipReset_ = cb;
    onClipResetCtx_ = ctx;
}

void MeterComponent::setPeakResetCallback (Callback cb, void* ctx) noexcept
{
    onPeakReset_ = cb;
    onPeakResetCtx_ = ctx;
}

float MeterComponent::dbToNormForScale (float db, mdsp_ui::meters::MeterScaleMode mode) noexcept
{
    if (mode == mdsp_ui::meters::MeterScaleMode::FullRange)
        return normaliseFromAnchors (db);

    if (mode == mdsp_ui::meters::MeterScaleMode::Top48Db)
    {
        const float clamped = juce::jlimit (-48.0f, 6.0f, std::isfinite (db) ? db : -48.0f);
        if (clamped <= 0.0f)
            return 0.9f * (clamped + 48.0f) / 48.0f;

        return 0.9f + 0.1f * (clamped / 6.0f);
    }

    return mdsp_ui::meters::MeterRenderStateProvider::normaliseDb (db, mode);
}

float MeterComponent::grToNorm (float grDb, float minDb, float maxDb) noexcept
{
    const float span = juce::jmax (0.001f, maxDb - minDb);
    return juce::jlimit (0.0f, 1.0f, (grDb - minDb) / span);
}

void MeterComponent::mouseDown (const juce::MouseEvent& e)
{
    if (kind_ == Kind::GainReduction)
    {
        if (onPeakReset_ != nullptr)
            onPeakReset_ (onPeakResetCtx_);
        return;
    }

    if (ledArea_.contains (e.getPosition()))
    {
        if (onClipReset_ != nullptr)
            onClipReset_ (onClipResetCtx_);
        return;
    }

    if (onPeakReset_ != nullptr)
        onPeakReset_ (onPeakResetCtx_);
}

void MeterComponent::resized()
{
    auto b = getLocalBounds();

    if (kind_ == Kind::Level)
    {
        numericArea_ = b.removeFromTop (66).reduced (2, 2);
        labelArea_ = {};
        ledArea_ = {};
        meterArea_ = b.reduced (6, 2);
        return;
    }

    labelArea_ = b.removeFromTop (16);
    numericArea_ = b.removeFromBottom (20).reduced (2, 2);
    auto ledRow = labelArea_;
    ledArea_ = ledRow.removeFromRight (14).withSizeKeepingCentre (10, 10);
    meterArea_ = b.reduced (6, 2);
}

void MeterComponent::paint (juce::Graphics& g)
{
    if (kind_ == Kind::GainReduction)
        paintGainReduction (g);
    else
        paintLevel (g);
}

void MeterComponent::paintGainReduction (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& m = ui_.metrics();

    g.setColour (theme.panel.withAlpha (0.9f));
    g.fillRoundedRectangle (meterArea_.toFloat(), m.rSmall);

    const float yTop = static_cast<float> (meterArea_.getY());
    const float h = static_cast<float> (meterArea_.getHeight());
    const float xLeft = static_cast<float> (meterArea_.getX());
    const float xRight = static_cast<float> (meterArea_.getRight());
    const float width = static_cast<float> (meterArea_.getWidth());

    g.setColour (theme.background.withAlpha (0.65f));
    g.drawRoundedRectangle (meterArea_.toFloat(), m.rSmall, m.strokeThin);

    auto paintOneBar = [&] (float grDb, float xl, float xr)
    {
        const float norm = grToNorm (grDb, grMinDb_, grMaxDb_);
        const float fillH = norm * h;
        const float fillTop = yTop;

        if (fillH > 0.5f)
        {
            juce::ColourGradient glow (theme.seriesSide.withAlpha (0.34f),
                                      xl + (xr - xl) * 0.5f,
                                      fillTop,
                                      theme.seriesSide.withAlpha (0.05f),
                                      xl + (xr - xl) * 0.5f,
                                      fillTop + fillH,
                                      false);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (juce::Rectangle<float> (xl, fillTop, xr - xl, fillH).expanded (0.5f, 0.0f),
                                    m.rSmall);

            g.setColour (theme.seriesSide.withAlpha (0.88f));
            g.fillRoundedRectangle (juce::Rectangle<float> (xl, fillTop, xr - xl, fillH), m.rSmall);

            const float edgeY = fillTop + fillH;
            g.setColour (theme.seriesSide.brighter (0.18f).withAlpha (0.72f));
            g.drawLine (xl + 1.0f,
                        edgeY - 0.5f,
                        xr - 1.0f,
                        edgeY - 0.5f,
                        m.strokeThin);
        }
    };

    if (grSingleBar_)
    {
        paintOneBar (grDbL_, xLeft + 3.0f, xRight - 3.0f);
    }
    else
    {
        const float midX = xLeft + width * 0.5f;
        const float halfW = juce::jmax (4.0f, (width - 6.0f) * 0.5f);
        const float barLRight = midX - 3.0f;
        const float barLLeft = barLRight - halfW;
        const float barRLeft = midX + 3.0f;
        const float barRRight = barRLeft + halfW;
        paintOneBar (grDbL_, barLLeft, barLRight);
        paintOneBar (grDbR_, barRLeft, barRRight);
    }

    g.setFont (ui_.type().labelFont().withHeight (kDbScaleFontHeight));

    for (const float grTick : { 3.0f, 6.0f, 12.0f })
    {
        if (grTick > grMaxDb_)
            continue;

        const float norm = grToNorm (grTick, grMinDb_, grMaxDb_);
        const float y = yTop + (norm * h);
        g.setColour (theme.text.withAlpha (0.28f));
        g.drawLine (xLeft, y, xRight, y, kDbScaleLineDense);

        g.setColour (theme.text.withAlpha (0.42f));
        g.drawText (juce::String (grTick, 0),
                    juce::Rectangle<float> (xLeft, y - 5.0f, width, 10.0f),
                    juce::Justification::centred);
    }

    g.setColour (theme.text.withAlpha (0.9f));
    g.setFont (ui_.type().labelFont());
    g.drawText (label_, labelArea_, juce::Justification::centred);

    const auto ledColour = theme.textMuted.withAlpha (0.22f);
    g.setColour (ledColour);
    g.fillEllipse (ledArea_.toFloat());
    g.setColour (theme.background.withAlpha (0.7f));
    g.drawEllipse (ledArea_.toFloat(), m.strokeThin);

    const auto boxR = numericArea_.toFloat();
    g.setColour (theme.background.withAlpha (0.55f));
    g.fillRoundedRectangle (boxR, m.rMed);
    g.setColour (theme.grid.withAlpha (0.35f));
    g.drawRoundedRectangle (boxR, m.rMed, m.strokeThin);

    g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));

    const juce::String peakLine = numericOverrideActive_ ? numericOverridePeak_ : numericTextPeak_;
    const juce::String rmsLine = numericOverrideActive_ ? numericOverrideRms_ : numericTextRms_;

    auto numBounds = numericArea_;
    auto peakBounds = numBounds.removeFromTop (numBounds.getHeight() / 2);
    auto rmsBounds = numBounds;

    g.setColour (theme.seriesSide.withAlpha (0.92f));
    g.drawText (peakLine, peakBounds, juce::Justification::centred);

    g.setColour (theme.textMuted.withAlpha (0.75f));
    g.drawText (rmsLine, rmsBounds, juce::Justification::centred);
}

void MeterComponent::paintLevel (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& m = ui_.metrics();

    g.setColour (theme.panel.withAlpha (0.9f));
    g.fillRoundedRectangle (meterArea_.toFloat(), m.rSmall);

    const float norm0 = dbToNormForScale (0.0f, renderState_.scaleMode);
    const float y0 = static_cast<float> (meterArea_.getBottom()) - (norm0 * static_cast<float> (meterArea_.getHeight()));
    const float yTop = static_cast<float> (meterArea_.getY());

    if (y0 > yTop)
    {
        const float capY = juce::jlimit (yTop + 1.0f, static_cast<float> (meterArea_.getBottom()) - 1.0f, y0);
        g.setColour (theme.danger.withAlpha (0.72f));
        g.drawLine (static_cast<float> (meterArea_.getX()) + 1.0f,
                    capY,
                    static_cast<float> (meterArea_.getRight()) - 1.0f,
                    capY,
                    1.4f);
    }

    g.setColour (theme.background.withAlpha (0.65f));
    g.drawRoundedRectangle (meterArea_.toFloat(), m.rSmall, m.strokeThin);

    const float yMax = static_cast<float> (meterArea_.getBottom());
    const float h = static_cast<float> (meterArea_.getHeight());
    const float xLeft = static_cast<float> (meterArea_.getX());
    const float xRight = static_cast<float> (meterArea_.getRight());
    const float width = static_cast<float> (meterArea_.getWidth());
    const auto peakColour = theme.accent.brighter (0.55f);

    const float peakNorm = dbToNormForScale (renderState_.peakDb, renderState_.scaleMode);
    const float rmsNorm = dbToNormForScale (renderState_.rmsDb, renderState_.scaleMode);
    const float maxPeakNorm = dbToNormForScale (renderState_.maxPeakDb, renderState_.scaleMode);

    const float peakH = peakNorm * h;
    const float peakTop = yMax - peakH;
    const bool hasPeakSignal = peakNorm > 0.002f;

    if (hasPeakSignal && peakH > 0.5f)
    {
        auto peakRect = meterArea_.withTop (static_cast<int> (std::round (peakTop)));
        const auto peakRectF = peakRect.toFloat();

        juce::ColourGradient signalGlow (peakColour.withAlpha (0.34f),
                                         xLeft + (width * 0.5f),
                                         peakTop,
                                         peakColour.withAlpha (0.07f),
                                         xLeft + (width * 0.5f),
                                         yMax,
                                         false);
        g.setGradientFill (signalGlow);
        g.fillRoundedRectangle (peakRectF.expanded (1.0f, 0.0f), m.rSmall);

        g.setColour (peakColour.withAlpha (0.88f));
        g.fillRoundedRectangle (peakRectF, m.rSmall);
    }

    const float rmsH = rmsNorm * h;
    const float rmsTop = yMax - rmsH;
    if (showRms_ && rmsNorm > 0.002f && rmsH > 0.5f)
    {
        auto rmsRect = meterArea_.withTop (static_cast<int> (std::round (rmsTop)));
        g.setColour (theme.accent.withAlpha (0.74f));
        g.fillRoundedRectangle (rmsRect.toFloat().reduced (2.0f, 0.0f), m.rSmall);
    }

    if (peakNorm > 0.002f)
    {
        g.setColour (peakColour.withAlpha (0.95f));
        g.drawLine (xLeft + m.strokeThick,
                    peakTop,
                    xRight - m.strokeThick,
                    peakTop,
                    m.strokeMed);
    }

    if (maxPeakNorm > 0.001f)
    {
        const float maxPeakY = yMax - (maxPeakNorm * h);
        g.setColour (theme.warning.withAlpha (0.9f));
        g.drawLine (xLeft + 1.0f,
                    maxPeakY,
                    xRight - 1.0f,
                    maxPeakY,
                    1.5f);
    }

    if (drawInternalScale_)
    {
        g.setFont (ui_.type().labelFont().withHeight (kDbScaleFontHeight));

        if (renderState_.scaleMode == mdsp_ui::meters::MeterScaleMode::Top24Db)
        {
            for (int i = 0; i <= 18; ++i)
            {
                const float db = -static_cast<float> (i);
                const float norm = dbToNormForScale (db, renderState_.scaleMode);
                const float y = yMax - (norm * h);
                const bool isZero = (i == 0);
                g.setColour (isZero ? theme.text.withAlpha (0.50f) : theme.text.withAlpha (0.26f));
                g.drawLine (xLeft, y, xRight, y, isZero ? kDbScaleLine0Db : kDbScaleLineDense);

                if ((i % 3) == 0)
                {
                    const char* label = nullptr;
                    switch (i)
                    {
                        case 0: label = "0"; break;
                        case 3: label = "-3"; break;
                        case 6: label = "-6"; break;
                        case 9: label = "-9"; break;
                        case 12: label = "-12"; break;
                        case 15: label = "-15"; break;
                        case 18: label = "-18"; break;
                        default: break;
                    }
                    if (label != nullptr)
                    {
                        g.setColour (theme.text.withAlpha (0.44f));
                        g.drawText (label,
                                    juce::Rectangle<float> (xLeft, y - 5.0f, width, 10.0f),
                                    juce::Justification::centred);
                    }
                }
            }
        }
        else if (renderState_.scaleMode == mdsp_ui::meters::MeterScaleMode::Top12Db)
        {
            for (int i = 0; i <= 12; ++i)
            {
                const float db = -static_cast<float> (i) * 0.5f;
                const float norm = dbToNormForScale (db, renderState_.scaleMode);
                const float y = yMax - (norm * h);
                const bool isZero = (i == 0);
                g.setColour (isZero ? theme.text.withAlpha (0.50f) : theme.text.withAlpha (0.26f));
                g.drawLine (xLeft, y, xRight, y, isZero ? kDbScaleLine0Db : kDbScaleLineDense);

                if ((i % 2) == 0)
                {
                    const char* label = nullptr;
                    switch (i)
                    {
                        case 0: label = "0"; break;
                        case 2: label = "-1"; break;
                        case 4: label = "-2"; break;
                        case 6: label = "-3"; break;
                        case 8: label = "-4"; break;
                        case 10: label = "-5"; break;
                        case 12: label = "-6"; break;
                        default: break;
                    }
                    if (label != nullptr)
                    {
                        g.setColour (theme.text.withAlpha (0.44f));
                        g.drawText (label,
                                    juce::Rectangle<float> (xLeft, y - 5.0f, width, 10.0f),
                                    juce::Justification::centred);
                    }
                }
            }
        }
        else
        {
            static constexpr float kTicksFull[] = { 0.0f, -3.0f, -6.0f, -10.0f, -15.0f, -20.0f, -30.0f, -40.0f, -50.0f, -120.0f };
            for (const auto db : kTicksFull)
            {
                const float norm = dbToNormForScale (db, renderState_.scaleMode);
                const float y = yMax - (norm * h);
                const float lineW = nearTick (db, 0.0f) ? kDbScaleLine0Db : kDbScaleLineThin;
                if (nearTick (db, 0.0f))
                {
                    g.setColour (theme.text.withAlpha (0.55f));
                    g.drawLine (xLeft, y, xRight, y, lineW);
                }
                else if (db > 0.0f)
                {
                    g.setColour (theme.danger.withAlpha (0.85f));
                    g.drawLine (xLeft, y, xRight, y, lineW);
                }
                else
                {
                    g.setColour (theme.text.withAlpha (0.32f));
                    g.drawLine (xLeft, y, xRight, y, lineW);
                }

                const char* label = nullptr;
                if (nearTick (db, 0.0f)) label = "0";
                else if (nearTick (db, -3.0f)) label = "-3";
                else if (nearTick (db, -6.0f)) label = "-6";
                else if (nearTick (db, -10.0f)) label = "-10";
                else if (nearTick (db, -15.0f)) label = "-15";
                else if (nearTick (db, -20.0f)) label = "-20";
                else if (nearTick (db, -30.0f)) label = "-30";
                else if (nearTick (db, -40.0f)) label = "-40";
                else if (nearTick (db, -50.0f)) label = "-50";
                else if (nearTick (db, -120.0f)) label = "-inf";

                if (label != nullptr)
                {
                    g.setColour (db >= 0.0f ? theme.danger.withAlpha (0.72f) : theme.text.withAlpha (0.46f));
                    g.drawText (label,
                                juce::Rectangle<float> (xLeft, y - 5.0f, width, 10.0f),
                                juce::Justification::centred);
                }
            }
        }
    }

    if (! ledArea_.isEmpty())
    {
        const auto ledColour = renderState_.clipLatched ? theme.danger : theme.textMuted.withAlpha (0.25f);
        g.setColour (ledColour);
        g.fillEllipse (ledArea_.toFloat());
        g.setColour (theme.background.withAlpha (0.7f));
        g.drawEllipse (ledArea_.toFloat(), m.strokeThin);
    }

    const auto boxR = numericArea_.toFloat();
    g.setColour (theme.background.withAlpha (0.55f));
    g.fillRoundedRectangle (boxR, m.rMed);
    g.setColour (theme.grid.withAlpha (0.35f));
    g.drawRoundedRectangle (boxR, m.rMed, m.strokeThin);

    const juce::String peakLine = numericOverrideActive_ ? numericOverridePeak_ : numericTextPeak_;
    const juce::String maxLine = numericOverrideActive_ ? numericOverrideMax_ : juce::String();
    const juce::String rmsLine = numericOverrideActive_ ? numericOverrideRms_ : numericTextRms_;

    auto numBounds = numericArea_.reduced (3, 3);
    juce::Rectangle<int> tpBounds;
    if (truePeakReadoutActive_)
        tpBounds = numBounds.removeFromTop (13);

    const int rowH = numBounds.getHeight() / 3;
    auto peakBounds = numBounds.removeFromTop (rowH);
    auto peakMaxBounds = numBounds.removeFromTop (rowH);
    auto rmsBounds = numBounds;

    const auto valueJustification = readoutAlignFromRight_ ? juce::Justification::centredRight
                                                           : juce::Justification::centredLeft;

    if (truePeakReadoutActive_)
    {
        const auto tpColour = truePeakReadoutOver_ ? theme.danger : theme.textMuted.withAlpha (0.78f);
        g.setFont (juce::Font (juce::FontOptions().withHeight (9.4f)).boldened());
        g.setColour (tpColour);
        g.drawText (truePeakReadoutText_, tpBounds, valueJustification);
    }

    g.setFont (juce::Font (juce::FontOptions().withHeight (9.4f)).boldened());

    g.setColour (peakColour.withAlpha (0.9f));
    g.drawText (peakLine, peakBounds, valueJustification);

    g.setColour (theme.warning.withAlpha (0.82f));
    g.drawText (maxLine, peakMaxBounds, valueJustification);

    g.setColour (theme.text.withAlpha (0.68f));
    g.drawText (rmsLine, rmsBounds, valueJustification);

    if (renderState_.bypassed)
    {
        g.setColour (theme.background.withAlpha (0.7f));
        g.fillRoundedRectangle (meterArea_.toFloat(), m.rSmall);

        g.setColour (theme.danger);
        g.setFont (ui_.type().labelFont().withHeight (10.0f).boldened());
        g.drawText ("BYPASS", meterArea_, juce::Justification::centred);
    }
}
