#pragma once

#include <atomic>
#include <limits>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <mdsp_dsp/dynamics/FinalCeilingLimiter.h>
#include <mdsp_dsp/dynamics/LimiterEnvelope.h>
#include <mdsp_dsp/dynamics/LookaheadDelay.h>
#include <mdsp_dsp/dynamics/PeakDetector.h>
#include <mdsp_dsp/dynamics/TruePeakDetector.h>
#include <mdsp_dsp/filters/LinearPhaseCrossover.h>
#include <mdsp_dsp/filters/HalfbandPolyphaseOS.h>
#include <mdsp_dsp/loudness/LoudnessAnalyzer.h>

#ifndef MDSP_BAND_HEADROOM_DB
 // Default was 2.0f in ADR-0009; 0.0f keeps the default limiter wideband-flat,
 // with multiband character introduced by Color instead of always-on pre-shave.
 #define MDSP_BAND_HEADROOM_DB 0.0f
#endif

//==============================================================================
class MasterLimiterAudioProcessor : public juce::AudioProcessor,
                                    private juce::AudioProcessorValueTreeState::Listener,
                                    private juce::AudioProcessorParameter::Listener,
                                    private juce::AsyncUpdater,
                                    private juce::Timer
{
public:
    enum class LearnState : int { Idle = 0, Armed = 1, Captured = 2 };

    MasterLimiterAudioProcessor();
    ~MasterLimiterAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getAPVTS() const noexcept { return apvts; }

    struct HistoryFrame
    {
        float grDb = 0.0f;
        float outDb = -120.0f;
        float inDb = -120.0f;
        float clipDb = 0.0f;
        float grLowDb = 0.0f;
        float grMidDb = 0.0f;
        float grHighDb = 0.0f;
    };

    int readHistorySince (uint32_t& inOutCursor, HistoryFrame* out, int maxOut) const noexcept;
    uint32_t getHistoryWriteIndex() const noexcept { return historyWriteIdx_.load (std::memory_order_acquire); }
    double getHistorySampleRate() const noexcept;
    int getHistoryFrameSamples() const noexcept { return historyFrameSamples_; }
    float getCeilingDbForGraph() const noexcept;
    float getClipThresholdDbForGraph() const noexcept;

    float getCurrentGrDb() const noexcept { return currentGrDb_.load (std::memory_order_relaxed); }
    float getCurrentGrLDb() const noexcept { return currentGrLDb_.load (std::memory_order_relaxed); }
    float getCurrentGrRDb() const noexcept { return currentGrRDb_.load (std::memory_order_relaxed); }
    float getCurrentGrLowLDb() const noexcept { return currentGrLowLDb_.load (std::memory_order_relaxed); }
    float getCurrentGrLowRDb() const noexcept { return currentGrLowRDb_.load (std::memory_order_relaxed); }
    float getCurrentGrMidLDb() const noexcept { return currentGrMidLDb_.load (std::memory_order_relaxed); }
    float getCurrentGrMidRDb() const noexcept { return currentGrMidRDb_.load (std::memory_order_relaxed); }
    float getCurrentGrHighLDb() const noexcept { return currentGrHighLDb_.load (std::memory_order_relaxed); }
    float getCurrentGrHighRDb() const noexcept { return currentGrHighRDb_.load (std::memory_order_relaxed); }
    float getCurrentClipDb() const noexcept { return currentClipDb_.load (std::memory_order_relaxed); }
    float getMaxGrSinceResetDb() const noexcept { return maxGrSinceResetDb_.load (std::memory_order_relaxed); }
    float getMaxGrLowLDb() const noexcept { return maxGrLowLDb_.load (std::memory_order_relaxed); }
    float getMaxGrLowRDb() const noexcept { return maxGrLowRDb_.load (std::memory_order_relaxed); }
    float getMaxGrMidLDb() const noexcept { return maxGrMidLDb_.load (std::memory_order_relaxed); }
    float getMaxGrMidRDb() const noexcept { return maxGrMidRDb_.load (std::memory_order_relaxed); }
    float getMaxGrHighLDb() const noexcept { return maxGrHighLDb_.load (std::memory_order_relaxed); }
    float getMaxGrHighRDb() const noexcept { return maxGrHighRDb_.load (std::memory_order_relaxed); }
    float getMaxClipSinceResetDb() const noexcept { return maxClipSinceResetDb_.load (std::memory_order_relaxed); }
    float getCurrentMsClampDb() const noexcept { return currentMsClampDb_.load (std::memory_order_relaxed); }
    float getMaxMsClampDb() const noexcept { return maxMsClampDb_.load (std::memory_order_relaxed); }
    float getCurrentFinalCeilingDb() const noexcept { return currentFinalCeilingDb_.load (std::memory_order_relaxed); }
    float getMaxFinalCeilingDb() const noexcept { return maxFinalCeilingDb_.load (std::memory_order_relaxed); }

    void resetMaxGr() noexcept
    {
        maxGrSinceResetDb_.store (0.0f, std::memory_order_relaxed);
        maxGrLowLDb_.store (0.0f, std::memory_order_relaxed);
        maxGrLowRDb_.store (0.0f, std::memory_order_relaxed);
        maxGrMidLDb_.store (0.0f, std::memory_order_relaxed);
        maxGrMidRDb_.store (0.0f, std::memory_order_relaxed);
        maxGrHighLDb_.store (0.0f, std::memory_order_relaxed);
        maxGrHighRDb_.store (0.0f, std::memory_order_relaxed);
    }
    void resetMaxClip() noexcept { maxClipSinceResetDb_.store (0.0f, std::memory_order_relaxed); }
    void resetMaxDevClampReadouts() noexcept
    {
        maxMsClampDb_.store (0.0f, std::memory_order_relaxed);
        maxFinalCeilingDb_.store (0.0f, std::memory_order_relaxed);
    }

    float getInputPeakLDb() const noexcept { return inputPeakLDb_.load (std::memory_order_relaxed); }
    float getInputPeakRDb() const noexcept { return inputPeakRDb_.load (std::memory_order_relaxed); }
    float getInputRmsLDb() const noexcept { return inputRmsLDb_.load (std::memory_order_relaxed); }
    float getInputRmsRDb() const noexcept { return inputRmsRDb_.load (std::memory_order_relaxed); }
    float getInputTruePeakLDb() const noexcept { return inputTruePeakLDb_.load (std::memory_order_relaxed); }
    float getInputTruePeakRDb() const noexcept { return inputTruePeakRDb_.load (std::memory_order_relaxed); }
    float getMaxInputTruePeakLDb() const noexcept { return maxInputTruePeakLDb_.load (std::memory_order_relaxed); }
    float getMaxInputTruePeakRDb() const noexcept { return maxInputTruePeakRDb_.load (std::memory_order_relaxed); }
    float getOutputPeakLDb() const noexcept { return outputPeakLDb_.load (std::memory_order_relaxed); }
    float getOutputPeakRDb() const noexcept { return outputPeakRDb_.load (std::memory_order_relaxed); }
    float getOutputRmsLDb() const noexcept { return outputRmsLDb_.load (std::memory_order_relaxed); }
    float getOutputRmsRDb() const noexcept { return outputRmsRDb_.load (std::memory_order_relaxed); }
    float getOutputTpDb() const noexcept { return outputTpDb_.load (std::memory_order_relaxed); }
    float getOutputTruePeakLDb() const noexcept { return outputTruePeakLDb_.load (std::memory_order_relaxed); }
    float getOutputTruePeakRDb() const noexcept { return outputTruePeakRDb_.load (std::memory_order_relaxed); }
    float getMaxOutputTruePeakLDb() const noexcept { return maxOutputTruePeakLDb_.load (std::memory_order_relaxed); }
    float getMaxOutputTruePeakRDb() const noexcept { return maxOutputTruePeakRDb_.load (std::memory_order_relaxed); }
    void resetInputTruePeakHolds() noexcept;
    void resetOutputTruePeakHolds() noexcept;

    mdsp_dsp::LoudnessAnalyzer& getLoudnessAnalyzer() noexcept { return loudness_; }
    const mdsp_dsp::LoudnessAnalyzer& getLoudnessAnalyzer() const noexcept { return loudness_; }

    LearnState getLearnState() const noexcept { return static_cast<LearnState> (learnState_.load (std::memory_order_relaxed)); }
    float getLearnedRefLufs() const noexcept { return learnedRefLufs_.load (std::memory_order_relaxed); }
    float getCompGainDb() const noexcept { return compGainDbMirror_.load (std::memory_order_relaxed); }

    void armLearnReference() noexcept;
    void clearLearnedReference();
    bool applyPreset (int presetIndex);
    void ensureCompareSlotsInitialized();
    int getActiveCompareSlot() const noexcept { return activeCompareSlot_; }
    void switchCompareSlot (int slotIndex);
    void copyActiveCompareSlotToOther();

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void commitHeavyControls();
    void cacheGainCeilingLinkParameters();
    void refreshGainCeilingLinkBaseline();
    void applyIoInputGain (juce::AudioBuffer<float>& buffer, int numSamples, int numChannels);
    void applyIoOutputGain (juce::AudioBuffer<float>& buffer, int numSamples, int numChannels);
    void processLearnResetRequest();
    void updateLearnCapture (int numSamples);
    float updateCompensationGainDb (float liveLufs);
    float updateDryCompensationGainDb (float liveDryLufs);
    void applyCompensationGain (juce::AudioBuffer<float>& buffer, int numSamples, int numChannels, float compGainDb) const;
    void processCore (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool forceBypass);
    mdsp_dsp::LinearPhaseCrossover::Spec readCrossoverSpecFromParams() const;
    void prepareCrossoverBanks (double osSampleRate);
    void rebuildCrossoverKernels();
    bool tryLockCrossoverBank() noexcept { return ! crossoverBankLock_.test_and_set (std::memory_order_acquire); }
    void lockCrossoverBankSpin() noexcept { while (crossoverBankLock_.test_and_set (std::memory_order_acquire)) {} }
    void unlockCrossoverBank() noexcept { crossoverBankLock_.clear (std::memory_order_release); }
    void commitLearnedRef();
    void captureCurrentStateToActiveCompareSlot();
    void replaceLiveStateFromCompareSlot (int slotIndex);
    juce::ValueTree createCompareStateTree() const;
    void restoreCompareStateFromTree (const juce::ValueTree& tree);

    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree compareSlotA_;
    juce::ValueTree compareSlotB_;
    int activeCompareSlot_ = 0;

    // Multiband pre-shave (ADR-0009): fixed 2-band split inside the 4x OS region.
    static constexpr float kBandHeadroomDb = MDSP_BAND_HEADROOM_DB;
    static constexpr float kDevXoverCutoffDefault = 120.0f;
    static constexpr float kDevXoverTransitionDefault = 120.0f;
    static constexpr float kDevXoverAttenDefault = 60.0f;
    static constexpr float kDevXoverTransitionMin = 60.0f;
    static constexpr float kDevXoverAttenMax = 72.0f;
    static constexpr float kLookaheadMs = 5.0f;
    static constexpr float kMaxLookaheadMs = 6.0f;
    static constexpr float kLowBandAutoReleaseScale = 3.0f;
    static constexpr float kHighBandAutoReleaseScale = 1.0f;
    static constexpr float kAutoSigmaAttackMs = 5.0f;
    static constexpr float kAutoSigmaDecayScale = 1.0f;

    mdsp_dsp::LookaheadDelay<float> lookahead_;
    mdsp_dsp::LookaheadDelay<float> lookaheadWide_;
    mdsp_dsp::LookaheadDelay<float> lookaheadPad_;
    mdsp_dsp::PeakDetector peakDetector_;
    mdsp_dsp::LimiterEnvelope envelope_;
    mdsp_dsp::LimiterEnvelope envelope_R_;
    mdsp_dsp::LinearPhaseCrossover detectCrossover_[2];
    mdsp_dsp::LinearPhaseCrossover applyCrossover_[2];
    std::atomic<int>  activeCrossoverBank_ { 0 };
    std::atomic<bool> crossoverSwapReady_ { false };
    int               crossoverPendingBank_ = 1;
    std::atomic_flag  crossoverBankLock_ = ATOMIC_FLAG_INIT;
    int               xoDuckPhase_      = 0;
    int               xoDuckPos_        = 0;
    int               xoFadeOutSamples_ = 1;
    int               xoFadeInSamples_  = 1;
    std::atomic<bool>          heavyCrossoverDirty_ { false };
    std::atomic<bool>          heavyLookaheadDirty_ { false };
    std::atomic<juce::uint32>  lastHeavyChangeMs_   { 0 };
    std::atomic<float>         committedLookaheadBandMs_ { 0.0f };
    std::atomic<float>         committedLookaheadWideMs_ { 0.0f };
    std::atomic<int>           heavyGestureActive_       { 0 };
    std::atomic<bool>          heavyGestureCommitPending_ { false };
    static constexpr int kHeavyDebounceMs = 120;
    static constexpr int kHeavyPollMs     = 30;
    mdsp_dsp::LimiterEnvelope envelopeLow_;
    mdsp_dsp::LimiterEnvelope envelopeHigh_;
    mdsp_dsp::LimiterEnvelope envelopeLowR_;
    mdsp_dsp::LimiterEnvelope envelopeHighR_;
    mdsp_dsp::HalfbandPolyphaseOS limiterOversampler_;
    mdsp_dsp::HalfbandPolyphaseOS clipperOversampler_;   // 1-stage (2×), runs inside the 4× domain
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> clipperOsAlignDelay_ { 4096 };

    juce::AudioBuffer<float> peakBuf_;
    juce::AudioBuffer<float> gainBuf_;
    juce::AudioBuffer<float> peakBufR_;
    juce::AudioBuffer<float> gainBufR_;
    juce::AudioBuffer<float> peakLowBuf_;
    juce::AudioBuffer<float> peakLowLBuf_;
    juce::AudioBuffer<float> peakLowRBuf_;
    juce::AudioBuffer<float> gainLowBuf_;
    juce::AudioBuffer<float> gainLowLBuf_;
    juce::AudioBuffer<float> gainLowRBuf_;
    juce::AudioBuffer<float> peakHighBuf_;
    juce::AudioBuffer<float> peakHighLBuf_;
    juce::AudioBuffer<float> peakHighRBuf_;
    juce::AudioBuffer<float> gainHighBuf_;
    juce::AudioBuffer<float> gainHighLBuf_;
    juce::AudioBuffer<float> gainHighRBuf_;
    juce::AudioBuffer<float> bandLowBuf_;
    juce::AudioBuffer<float> bandHighBuf_;
    juce::AudioBuffer<float> gLowOutBuf_;
    juce::AudioBuffer<float> gHighOutBuf_;
    juce::AudioBuffer<float> gLowOutRBuf_;
    juce::AudioBuffer<float> gHighOutRBuf_;
    juce::AudioBuffer<float> bandLimitedBuf_;
    mdsp_dsp::FinalCeilingLimiter finalCeiling_;
    mdsp_dsp::TruePeakDetector inputTruePeakL_;
    mdsp_dsp::TruePeakDetector inputTruePeakR_;
    mdsp_dsp::TruePeakDetector outputTruePeakL_;
    mdsp_dsp::TruePeakDetector outputTruePeakR_;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay_ { 4096 };
    juce::AudioBuffer<float> dryScratch_;
    juce::LinearSmoothedValue<float> bypassFade_;
    juce::LinearSmoothedValue<float> bandLinkSmoothed_;
    juce::LinearSmoothedValue<float> inputGainSmoothed_;
    juce::LinearSmoothedValue<float> clipperDriveSmoothed_;
    juce::LinearSmoothedValue<float> ioInputGainLSmoothed_;
    juce::LinearSmoothedValue<float> ioInputGainRSmoothed_;
    juce::LinearSmoothedValue<float> ioOutputGainLSmoothed_;
    juce::LinearSmoothedValue<float> ioOutputGainRSmoothed_;

    juce::AudioParameterChoice* ceilingMode_ = nullptr;
    juce::AudioParameterChoice* stereoMode_ = nullptr;
    juce::AudioParameterChoice* characterChoice_ = nullptr;
    juce::AudioParameterFloat* inputGainDbParam_ = nullptr;
    juce::AudioParameterFloat* ceilingDbParam_ = nullptr;
    juce::AudioParameterBool* limiterActive_ = nullptr;
    juce::AudioParameterBool* pluginBypass_ = nullptr;
    juce::AudioParameterBool* gainCeilingLink_ = nullptr;
    juce::AudioParameterFloat* releaseSustainRatio_ = nullptr;
    juce::AudioParameterBool* releaseAuto_ = nullptr;
    juce::AudioParameterChoice* autoReleaseMode_ = nullptr;
    juce::AudioParameterBool* clipperActive_ = nullptr;
    std::atomic<float>* clipperDriveDb_ = nullptr;
    juce::AudioParameterChoice* clipperMode_ = nullptr;
    std::atomic<float>* ioInputLDb_ = nullptr;
    std::atomic<float>* ioInputRDb_ = nullptr;
    std::atomic<float>* ioOutputLDb_ = nullptr;
    std::atomic<float>* ioOutputRDb_ = nullptr;
    std::atomic<float>* stereoLinkPct_ = nullptr;
    std::atomic<float>* msLinkPct_ = nullptr;
    std::atomic<float>* bandColor_ = nullptr;
    std::atomic<float>* gainMatchAuto_ = nullptr;
    std::atomic<float>* devLowBandReleaseScale_ = nullptr;
    std::atomic<float>* devHighBandReleaseScale_ = nullptr;
    std::atomic<float>* devSigmaAttackMs_ = nullptr;
    std::atomic<float>* devSigmaDecayScale_ = nullptr;
    std::atomic<float>* devAttackMs_ = nullptr;
    std::atomic<float>* devAttackMode_ = nullptr;
    std::atomic<float>* devRealAttackMs_ = nullptr;
    std::atomic<float>* devReleaseEngine_ = nullptr;
    std::atomic<float>* devLaReleaseMs_ = nullptr;
    std::atomic<float>* devLaReleasePoles_ = nullptr;
    std::atomic<float>* devLookaheadBandMs_ = nullptr;
    std::atomic<float>* devLookaheadWideMs_ = nullptr;
    std::atomic<float>* devXoverCutoffHz_ = nullptr;
    std::atomic<float>* devXoverTransitionHz_ = nullptr;
    std::atomic<float>* devXoverAttenDb_ = nullptr;
    std::atomic<float>* devBandStereoLinkPct_ = nullptr;
    std::atomic<float>* devMsSafetyClamp_ = nullptr;
    std::atomic<float>* devFinalCeiling_ = nullptr;
    juce::AudioParameterBool* ioInputLink_ = nullptr;
    juce::AudioParameterBool* ioOutputLink_ = nullptr;

    int  baseLatencySamples_ = 0;
    int  crossoverOsLatencySamples_ = 0;
    int  crossoverOsLatencyHostSamples_ = 0;
    int  limiterOsLatencySamples_ = 0;
    int  clipperOsLatencySamples4x_ = 0;   // padded total latency in 4×-rate samples
    int  clipperOsLatencyHostSamples_ = 0;
    int  clipperOsAlignPad4x_ = 0;
    int  finalCeilingLatencySamples_ = 0;
    int  cachedCeilingMode_   = 0;
    int  osMaxLookaheadSamples_ = 0;
    double osSampleRate_ = 0.0;

    std::atomic<float> currentGrDb_ { 0.0f };
    std::atomic<float> currentGrLDb_ { 0.0f };
    std::atomic<float> currentGrRDb_ { 0.0f };
    std::atomic<float> currentGrLowLDb_ { 0.0f };
    std::atomic<float> currentGrLowRDb_ { 0.0f };
    std::atomic<float> currentGrMidLDb_ { 0.0f };
    std::atomic<float> currentGrMidRDb_ { 0.0f };
    std::atomic<float> currentGrHighLDb_ { 0.0f };
    std::atomic<float> currentGrHighRDb_ { 0.0f };
    std::atomic<float> currentClipDb_ { 0.0f };
    std::atomic<float> maxGrSinceResetDb_ { 0.0f };
    std::atomic<float> maxGrLowLDb_ { 0.0f };
    std::atomic<float> maxGrLowRDb_ { 0.0f };
    std::atomic<float> maxGrMidLDb_ { 0.0f };
    std::atomic<float> maxGrMidRDb_ { 0.0f };
    std::atomic<float> maxGrHighLDb_ { 0.0f };
    std::atomic<float> maxGrHighRDb_ { 0.0f };
    std::atomic<float> maxClipSinceResetDb_ { 0.0f };
    std::atomic<float> currentMsClampDb_ { 0.0f };
    std::atomic<float> maxMsClampDb_ { 0.0f };
    std::atomic<float> currentFinalCeilingDb_ { 0.0f };
    std::atomic<float> maxFinalCeilingDb_ { 0.0f };

    std::atomic<float> inputPeakLDb_ { -100.0f };
    std::atomic<float> inputPeakRDb_ { -100.0f };
    std::atomic<float> inputRmsLDb_ { -100.0f };
    std::atomic<float> inputRmsRDb_ { -100.0f };
    std::atomic<float> inputTruePeakLDb_ { -100.0f };
    std::atomic<float> inputTruePeakRDb_ { -100.0f };
    std::atomic<float> maxInputTruePeakLDb_ { -100.0f };
    std::atomic<float> maxInputTruePeakRDb_ { -100.0f };
    std::atomic<float> outputPeakLDb_ { -100.0f };
    std::atomic<float> outputPeakRDb_ { -100.0f };
    std::atomic<float> outputRmsLDb_ { -100.0f };
    std::atomic<float> outputRmsRDb_ { -100.0f };
    std::atomic<float> outputTpDb_ { -100.0f };
    std::atomic<float> outputTruePeakLDb_ { -100.0f };
    std::atomic<float> outputTruePeakRDb_ { -100.0f };
    std::atomic<float> maxOutputTruePeakLDb_ { -100.0f };
    std::atomic<float> maxOutputTruePeakRDb_ { -100.0f };
    std::atomic<float> lastLinkedInputGainDb_ { 0.0f };
    std::atomic<float> lastLinkedCeilingDb_ { 0.0f };
    std::atomic<bool> gainCeilingLinkWasEnabled_ { false };
    std::atomic<bool> couplingInProgress_ { false };

    static constexpr int kHistoryRingSize = 65536;
    HistoryFrame historyRing_[kHistoryRingSize] {};
    std::atomic<uint32_t> historyWriteIdx_ { 0 };
    std::vector<float> grEnvBuf_;
    std::vector<float> clipEnvBuf_;
    int historyFrameSamples_ = 0;
    int historySampleCounter_ = 0;
    float frameMaxGrDb_ = 0.0f;
    float frameMaxGrLowDb_ = 0.0f;
    float frameMaxGrMidDb_ = 0.0f;
    float frameMaxGrHighDb_ = 0.0f;
    float frameMaxOutDb_ = -120.0f;
    float frameMaxInDb_ = -120.0f;
    float frameMaxClipDb_ = 0.0f;

    mdsp_dsp::LoudnessAnalyzer loudness_;
    mdsp_dsp::LoudnessAnalyzer loudnessRef_;
    mdsp_dsp::LoudnessAnalyzer loudnessTrack_;

    std::atomic<float> learnedRefLufs_ { -std::numeric_limits<float>::infinity() };
    std::atomic<int> learnState_ { static_cast<int> (LearnState::Idle) };
    std::atomic<bool> learnResetRequest_ { false };
    int armedSamplesElapsed_ = 0;
    int learnWindowSamples_ = 0;

    std::atomic<float> compGainDbMirror_ { 0.0f };
    float compGainDbSmoothed_ = 0.0f;
    float compGainSmoothCoef_ = 0.0f;
    bool compActiveLastBlock_ = false;
    float dryCompGainDbSmoothed_ = 0.0f;
    std::atomic<float> dryCompGainDbMirror_ { 0.0f };
    float msInL_ = 0.0f;
    float msInR_ = 0.0f;
    float msOutL_ = 0.0f;
    float msOutR_ = 0.0f;
    float meterRmsMsCoeff_ = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLimiterAudioProcessor)
};
