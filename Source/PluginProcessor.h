#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <mdsp_dsp/dynamics/IspTrimStage.h>
#include <mdsp_dsp/dynamics/LimiterEnvelope.h>
#include <mdsp_dsp/dynamics/LookaheadDelay.h>
#include <mdsp_dsp/dynamics/PeakDetector.h>

#include "parameters/Parameters.h"

//==============================================================================
class MasterLimiterAudioProcessor : public juce::AudioProcessor
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
    float getCurrentTpTrimDb() const noexcept { return currentTpTrimDb_.load (std::memory_order_relaxed); }

private:
    juce::AudioProcessorValueTreeState apvts;

    mdsp_dsp::LookaheadDelay<float> lookahead_;
    mdsp_dsp::PeakDetector peakDetector_;
    mdsp_dsp::LimiterEnvelope envelope_;
    mdsp_dsp::IspTrimStage ispTrim_;

    juce::AudioBuffer<float> peakBuf_;
    juce::AudioBuffer<float> gainBuf_;

    juce::AudioParameterChoice* ceilingMode_ = nullptr;
    juce::AudioParameterFloat* releaseSustainRatio_ = nullptr;

    int  baseLatencySamples_ = 0;
    int  osLatencySamples_   = 0;
    int  cachedCeilingMode_   = 0;

    std::atomic<float> currentGrDb_ { 0.0f };
    std::atomic<float> currentTpTrimDb_ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLimiterAudioProcessor)
};
