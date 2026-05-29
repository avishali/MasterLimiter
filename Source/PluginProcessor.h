#pragma once

#include <atomic>
#include <cstdint>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <mdsp_dsp/dynamics/LimiterEnvelope.h>
#include <mdsp_dsp/dynamics/LookaheadDelay.h>
#include <mdsp_dsp/dynamics/PeakDetector.h>
#include <mdsp_dsp/loudness/LoudnessAnalyzer.h>

#include "parameters/Parameters.h"

//==============================================================================
class MasterLimiterAudioProcessor : public juce::AudioProcessor,
                                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    MasterLimiterAudioProcessor();
    ~MasterLimiterAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
    int64_t getAudioBlockMaxUs() const noexcept { return audioBlockMaxUs_.load (std::memory_order_relaxed); }
    int64_t getSectionMaxUsDrive() const noexcept { return sectionMaxUsDrive_.load (std::memory_order_relaxed); }
    int64_t getSectionMaxUsUpsample() const noexcept { return sectionMaxUsUpsample_.load (std::memory_order_relaxed); }
    int64_t getSectionMaxUsClipperPeak() const noexcept { return sectionMaxUsClipperPeak_.load (std::memory_order_relaxed); }
    int64_t getSectionMaxUsEnvelope() const noexcept { return sectionMaxUsEnvelope_.load (std::memory_order_relaxed); }
    int64_t getSectionMaxUsGainMul() const noexcept { return sectionMaxUsGainMul_.load (std::memory_order_relaxed); }
    int64_t getSectionMaxUsDownsample() const noexcept { return sectionMaxUsDownsample_.load (std::memory_order_relaxed); }
    int64_t getSectionMaxUsOutput() const noexcept { return sectionMaxUsOutput_.load (std::memory_order_relaxed); }

    void resetMaxGr() noexcept { maxGrSinceResetDb_.store (0.0f, std::memory_order_relaxed); }
    void resetMaxClip() noexcept { maxClipSinceResetDb_.store (0.0f, std::memory_order_relaxed); }
    void resetAudioBlockMaxUs() noexcept
    {
        audioBlockMaxUs_.store (0, std::memory_order_relaxed);
        sectionMaxUsDrive_.store (0, std::memory_order_relaxed);
        sectionMaxUsUpsample_.store (0, std::memory_order_relaxed);
        sectionMaxUsClipperPeak_.store (0, std::memory_order_relaxed);
        sectionMaxUsEnvelope_.store (0, std::memory_order_relaxed);
        sectionMaxUsGainMul_.store (0, std::memory_order_relaxed);
        sectionMaxUsDownsample_.store (0, std::memory_order_relaxed);
        sectionMaxUsOutput_.store (0, std::memory_order_relaxed);
    }

    float getInputPeakLDb() const noexcept { return inputPeakLDb_.load (std::memory_order_relaxed); }
    float getInputPeakRDb() const noexcept { return inputPeakRDb_.load (std::memory_order_relaxed); }
    float getOutputPeakLDb() const noexcept { return outputPeakLDb_.load (std::memory_order_relaxed); }
    float getOutputPeakRDb() const noexcept { return outputPeakRDb_.load (std::memory_order_relaxed); }
    float getOutputTpDb() const noexcept { return outputTpDb_.load (std::memory_order_relaxed); }

    mdsp_dsp::LoudnessAnalyzer& getLoudnessAnalyzer() noexcept { return loudness_; }
    const mdsp_dsp::LoudnessAnalyzer& getLoudnessAnalyzer() const noexcept { return loudness_; }

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void cacheGainCeilingLinkParameters();
    void refreshGainCeilingLinkBaseline();

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
    std::atomic<int64_t> audioBlockMaxUs_ { 0 };
    std::atomic<int64_t> sectionMaxUsDrive_ { 0 };
    std::atomic<int64_t> sectionMaxUsUpsample_ { 0 };
    std::atomic<int64_t> sectionMaxUsClipperPeak_ { 0 };
    std::atomic<int64_t> sectionMaxUsEnvelope_ { 0 };
    std::atomic<int64_t> sectionMaxUsGainMul_ { 0 };
    std::atomic<int64_t> sectionMaxUsDownsample_ { 0 };
    std::atomic<int64_t> sectionMaxUsOutput_ { 0 };

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLimiterAudioProcessor)
};
