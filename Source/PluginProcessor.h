#pragma once

#include <atomic>
#include <limits>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <mdsp_dsp/dynamics/LimiterEnvelope.h>
#include <mdsp_dsp/dynamics/LookaheadDelay.h>
#include <mdsp_dsp/dynamics/PeakDetector.h>
#include <mdsp_dsp/loudness/LoudnessAnalyzer.h>

//==============================================================================
class MasterLimiterAudioProcessor : public juce::AudioProcessor,
                                    private juce::AudioProcessorValueTreeState::Listener,
                                    private juce::AsyncUpdater
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

    float getCurrentGrDb() const noexcept { return currentGrDb_.load (std::memory_order_relaxed); }
    float getCurrentGrLDb() const noexcept { return currentGrLDb_.load (std::memory_order_relaxed); }
    float getCurrentGrRDb() const noexcept { return currentGrRDb_.load (std::memory_order_relaxed); }
    float getCurrentClipDb() const noexcept { return currentClipDb_.load (std::memory_order_relaxed); }
    float getMaxGrSinceResetDb() const noexcept { return maxGrSinceResetDb_.load (std::memory_order_relaxed); }
    float getMaxClipSinceResetDb() const noexcept { return maxClipSinceResetDb_.load (std::memory_order_relaxed); }

    void resetMaxGr() noexcept { maxGrSinceResetDb_.store (0.0f, std::memory_order_relaxed); }
    void resetMaxClip() noexcept { maxClipSinceResetDb_.store (0.0f, std::memory_order_relaxed); }

    float getInputPeakLDb() const noexcept { return inputPeakLDb_.load (std::memory_order_relaxed); }
    float getInputPeakRDb() const noexcept { return inputPeakRDb_.load (std::memory_order_relaxed); }
    float getOutputPeakLDb() const noexcept { return outputPeakLDb_.load (std::memory_order_relaxed); }
    float getOutputPeakRDb() const noexcept { return outputPeakRDb_.load (std::memory_order_relaxed); }
    float getOutputTpDb() const noexcept { return outputTpDb_.load (std::memory_order_relaxed); }

    mdsp_dsp::LoudnessAnalyzer& getLoudnessAnalyzer() noexcept { return loudness_; }
    const mdsp_dsp::LoudnessAnalyzer& getLoudnessAnalyzer() const noexcept { return loudness_; }

    LearnState getLearnState() const noexcept { return static_cast<LearnState> (learnState_.load (std::memory_order_relaxed)); }
    float getLearnedRefLufs() const noexcept { return learnedRefLufs_.load (std::memory_order_relaxed); }
    float getCompGainDb() const noexcept { return compGainDbMirror_.load (std::memory_order_relaxed); }

    void armLearnReference() noexcept;
    void clearLearnedReference();

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void cacheGainCeilingLinkParameters();
    void refreshGainCeilingLinkBaseline();
    void applyIoInputGain (juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) const;
    void applyIoOutputGain (juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) const;
    void processLearnResetRequest();
    void updateLearnCapture (int numSamples);
    float updateCompensationGainDb (float liveLufs);
    void applyCompensationGain (juce::AudioBuffer<float>& buffer, int numSamples, int numChannels, float compGainDb) const;
    void commitLearnedRef();

    juce::AudioProcessorValueTreeState apvts;

    mdsp_dsp::LookaheadDelay<float> lookahead_;
    mdsp_dsp::PeakDetector peakDetector_;
    mdsp_dsp::LimiterEnvelope envelope_;
    mdsp_dsp::LimiterEnvelope envelope_R_;
    juce::dsp::Oversampling<float> limiterOversampler_ {
        2,
        2,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
        true,
        true
    };

    juce::AudioBuffer<float> peakBuf_;
    juce::AudioBuffer<float> gainBuf_;
    juce::AudioBuffer<float> peakBufR_;
    juce::AudioBuffer<float> gainBufR_;

    juce::AudioParameterChoice* ceilingMode_ = nullptr;
    juce::AudioParameterChoice* characterChoice_ = nullptr;
    juce::AudioParameterFloat* inputGainDbParam_ = nullptr;
    juce::AudioParameterFloat* ceilingDbParam_ = nullptr;
    juce::AudioParameterBool* limiterActive_ = nullptr;
    juce::AudioParameterBool* gainCeilingLink_ = nullptr;
    juce::AudioParameterFloat* releaseSustainRatio_ = nullptr;
    std::atomic<float>* clipperDriveDb_ = nullptr;
    std::atomic<float>* ioInputLDb_ = nullptr;
    std::atomic<float>* ioInputRDb_ = nullptr;
    std::atomic<float>* ioOutputLDb_ = nullptr;
    std::atomic<float>* ioOutputRDb_ = nullptr;
    std::atomic<float>* stereoLinkPct_ = nullptr;
    std::atomic<float>* gainMatchAuto_ = nullptr;
    juce::AudioParameterBool* ioInputLink_ = nullptr;
    juce::AudioParameterBool* ioOutputLink_ = nullptr;

    int  baseLatencySamples_ = 0;
    int  limiterOsLatencySamples_ = 0;
    int  cachedCeilingMode_   = 0;

    std::atomic<float> currentGrDb_ { 0.0f };
    std::atomic<float> currentGrLDb_ { 0.0f };
    std::atomic<float> currentGrRDb_ { 0.0f };
    std::atomic<float> currentClipDb_ { 0.0f };
    std::atomic<float> maxGrSinceResetDb_ { 0.0f };
    std::atomic<float> maxClipSinceResetDb_ { 0.0f };

    std::atomic<float> inputPeakLDb_ { -100.0f };
    std::atomic<float> inputPeakRDb_ { -100.0f };
    std::atomic<float> outputPeakLDb_ { -100.0f };
    std::atomic<float> outputPeakRDb_ { -100.0f };
    std::atomic<float> outputTpDb_ { -100.0f };
    std::atomic<float> lastLinkedInputGainDb_ { 0.0f };
    std::atomic<float> lastLinkedCeilingDb_ { -1.0f };
    std::atomic<bool> gainCeilingLinkWasEnabled_ { false };
    std::atomic<bool> couplingInProgress_ { false };

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLimiterAudioProcessor)
};
