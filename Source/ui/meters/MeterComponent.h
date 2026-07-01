#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/meters/MeterRenderState.h>

class MeterComponent : public juce::Component
{
public:
    using MeterRenderState = mdsp_ui::meters::MeterRenderState;
    using Callback = void (*) (void*) noexcept;

    enum class Kind
    {
        Level = 0,
        GainReduction = 1
    };

    MeterComponent (mdsp_ui::UiContext& ui, juce::String labelText);
    ~MeterComponent() override = default;

    void setKind (Kind kind) noexcept;
    Kind getKind() const noexcept { return kind_; }

    void setLabelText (juce::String labelText);
    void setRenderState (const MeterRenderState& state);

    void setRangeDb (float minDb, float maxDb) noexcept;
    void setDrawInternalScale (bool shouldDraw) noexcept;
    void setShowRms (bool shouldShow) noexcept;
    juce::Rectangle<int> getMeterArea() const noexcept { return meterArea_; }

    void setGrDb (float db) noexcept;
    void setStereoGrDb (float l, float r) noexcept;

    /** When Kind::GainReduction, draw one bar per component (L/R group uses two instances). */
    void setGrSingleBarMode (bool single) noexcept;

    void setNumericReadoutOverride (bool active, juce::String line1, juce::String line2) noexcept;
    void setNumericReadoutOverride (bool active, juce::String peak, juce::String maxPeak, juce::String rms) noexcept;
    void setTruePeakReadout (bool active, juce::String text, bool over) noexcept;

    /** When true, parent draws TP/SP/MAX/RMS captions in the center gutter. */
    void setExternalReadoutCaptions (bool external) noexcept;
    /** Level readout values align toward the stereo center gutter. */
    void setReadoutAlignTowardCenter (bool fromRight) noexcept;

    juce::Rectangle<int> getNumericArea() const noexcept { return numericArea_; }

    void setClipResetCallback (Callback cb, void* ctx) noexcept;
    void setPeakResetCallback (Callback cb, void* ctx) noexcept;

    void mouseDown (const juce::MouseEvent&) override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static float dbToNormForScale (float db, mdsp_ui::meters::MeterScaleMode mode) noexcept;
    static float grToNorm (float grDb, float minDb, float maxDb) noexcept;

    void paintLevel (juce::Graphics& g);
    void paintGainReduction (juce::Graphics& g);

    mdsp_ui::UiContext& ui_;
    MeterRenderState renderState_ {};

    Kind kind_ { Kind::Level };
    float grDbL_ { 0.0f };
    float grDbR_ { 0.0f };
    float grMinDb_ { 0.0f };
    float grMaxDb_ { 20.0f };
    bool grSingleBar_ { true };
    bool drawInternalScale_ { true };
    bool showRms_ { false };

    juce::String label_;
    juce::String numericTextPeak_ { "-inf" };
    juce::String numericTextRms_ { "-inf" };
    bool numericOverrideActive_ { false };
    juce::String numericOverridePeak_;
    juce::String numericOverrideMax_;
    juce::String numericOverrideRms_;
    bool truePeakReadoutActive_ { false };
    bool truePeakReadoutOver_ { false };
    juce::String truePeakReadoutText_;
    bool externalReadoutCaptions_ { false };
    bool readoutAlignFromRight_ { true };

    Callback onClipReset_ = nullptr;
    Callback onPeakReset_ = nullptr;
    void* onClipResetCtx_ = nullptr;
    void* onPeakResetCtx_ = nullptr;

    juce::Rectangle<int> labelArea_;
    juce::Rectangle<int> ledArea_;
    juce::Rectangle<int> meterArea_;
    juce::Rectangle<int> numericArea_;
};
