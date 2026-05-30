#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/meters/MeterRenderState.h>
#include <mdsp_ui/meters/MeterRenderStateProvider.h>

#include "MeterComponent.h"
#include "PluginProcessor.h"

class MeterGroupComponent : public juce::Component
{
public:
    using DisplayMode = mdsp_ui::meters::MeterDisplayMode;
    using ScaleMode = mdsp_ui::meters::MeterScaleMode;

    enum class BusKind
    {
        Input = 0,
        Output = 1,
        GainReduction = 2
    };

    MeterGroupComponent (mdsp_ui::UiContext& ui, MasterLimiterAudioProcessor& processor, BusKind kind);
    ~MeterGroupComponent() override = default;

    int getPreferredWidth() const noexcept;

    /** Editor timer (~30 Hz). `dtSec` is 1/updateRate. */
    void sync (double hostSampleRate, float dtSec);

    void resetPeakHolds() noexcept;
    void setScaleMode (ScaleMode mode) noexcept;
    void setShowRms (bool shouldShow) noexcept;
    juce::Rectangle<int> getScaleReferenceBoundsInParent() const noexcept;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static void peakResetThunk (void* ctx) noexcept;

    void handlePeakReset() noexcept;
    void pushLevelRenderStates();
    void layoutLevelMeters();

    struct PeakNumericSmoother
    {
        float held { -200.0f };
        float duty { -200.0f };
        int holdTicksLeft { 0 };

        void reset() noexcept;
        void tick (float raw, float dtSec, int holdTicksAt30Hz) noexcept;
        static juce::String formatDb (float v) noexcept;
    };

    struct GrNumericSmoother
    {
        float v { 0.0f };

        void reset() noexcept;
        void tick (float raw, float dtSec) noexcept;
        static juce::String formatDb (float g) noexcept;
    };

    struct DisplayLevelSmoother
    {
        float peakDb { -200.0f };
        float rmsDb { -200.0f };

        void reset (float peak, float rms) noexcept;
        void tick (float peak, float rms, float dtSec) noexcept;
    };

    PeakNumericSmoother peakSmooth0_ {};
    PeakNumericSmoother peakSmooth1_ {};
    PeakNumericSmoother rmsSmooth0_ {};
    PeakNumericSmoother rmsSmooth1_ {};
    DisplayLevelSmoother displaySmooth0_ {};
    DisplayLevelSmoother displaySmooth1_ {};
    GrNumericSmoother grSmooth_ {};

    mdsp_ui::UiContext& ui_;
    MasterLimiterAudioProcessor& processor_;
    const BusKind kind_;

    int channelCount_ = 2;
    ScaleMode scaleMode_ { ScaleMode::FullRange };
    bool showRms_ { false };

    std::unique_ptr<MeterComponent> meter0_;
    std::unique_ptr<MeterComponent> meter1_;
    mdsp_ui::meters::MeterRenderStateProvider provider0_;
    mdsp_ui::meters::MeterRenderStateProvider provider1_;
    mdsp_ui::meters::MeterRenderState renderState0_ {};
    mdsp_ui::meters::MeterRenderState renderState1_ {};

    juce::Rectangle<int> headerArea_;
    juce::Rectangle<int> labelArea_;
    juce::Rectangle<int> metersArea_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterGroupComponent)
};
