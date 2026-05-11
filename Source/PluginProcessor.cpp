#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

#include "parameters/Parameters.h"

namespace
{
float readFloatParam (juce::AudioProcessorValueTreeState& apvts, const char* paramId)
{
    auto* p = apvts.getParameter (paramId);
    jassert (p != nullptr);
    return (float) static_cast<juce::RangedAudioParameter*> (p)->convertFrom0to1 (p->getValue());
}
} // namespace

//==============================================================================
MasterLimiterAudioProcessor::MasterLimiterAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this,
             nullptr,
             "MasterLimiterParams",
             Parameters::createParameterLayout())
{
    jassert (apvts.getParameter ("input_gain_db") != nullptr);
    jassert (apvts.getParameter ("ceiling_db") != nullptr);
    jassert (apvts.getParameter ("release_ms") != nullptr);
}

MasterLimiterAudioProcessor::~MasterLimiterAudioProcessor() = default;

void MasterLimiterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int lookaheadSamples = static_cast<int> (std::llround (5.0e-3 * sampleRate));
    lookahead_.prepare (2, lookaheadSamples);
    lookahead_.setDelaySamples (lookaheadSamples);
    peakDetector_.prepare (2);

    mdsp_dsp::LimiterEnvelope::Spec spec;
    spec.sampleRate = sampleRate;
    spec.lookaheadSamples = lookaheadSamples;
    spec.maxBlockSize = samplesPerBlock;
    envelope_.prepare (spec);

    peakBuf_.setSize (1, samplesPerBlock, false, true, true);
    gainBuf_.setSize (1, samplesPerBlock, false, true, true);

    lookahead_.reset();
    envelope_.reset();

    setLatencySamples (lookaheadSamples);
}

void MasterLimiterAudioProcessor::releaseResources()
{
}

bool MasterLimiterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() != 1 || layouts.outputBuses.size() != 1)
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MasterLimiterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);

    if (apvts.getParameter ("input_gain_db") == nullptr || apvts.getParameter ("ceiling_db") == nullptr
        || apvts.getParameter ("release_ms") == nullptr)
        return;

    const int n = buffer.getNumSamples();
    if (n <= 0)
        return;

    const int nch = juce::jmin (2, buffer.getNumChannels());
    if (nch <= 0)
        return;

    const float inGainLin = juce::Decibels::decibelsToGain (readFloatParam (apvts, "input_gain_db"));
    for (int ch = 0; ch < nch; ++ch)
        buffer.applyGain (ch, 0, n, inGainLin);

    auto* peak = peakBuf_.getWritePointer (0);
    const float* const ch0 = buffer.getReadPointer (0);
    const float* const ch1 = nch > 1 ? buffer.getReadPointer (1) : ch0;
    const float* const channelPtrs[2] = { ch0, ch1 };

    for (int i = 0; i < n; ++i)
        peak[i] = peakDetector_.process (channelPtrs, nch, i);

    const float thresholdLin = juce::Decibels::decibelsToGain (readFloatParam (apvts, "ceiling_db"));
    envelope_.setThresholdLinear (thresholdLin);
    envelope_.setReleaseMs (readFloatParam (apvts, "release_ms"));

    auto* gain = gainBuf_.getWritePointer (0);
    envelope_.process (peak, gain, n);

    for (int ch = 0; ch < nch; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const float delayed = lookahead_.pushPop (ch, d[i]);
            d[i] = delayed * gain[i];
        }
    }

    currentGrDb_.store (envelope_.getLastBlockMaxGrDb(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* MasterLimiterAudioProcessor::createEditor()
{
    return new MasterLimiterAudioProcessorEditor (*this);
}

bool MasterLimiterAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String MasterLimiterAudioProcessor::getName() const
{
    return "MasterLimiter";
}

bool MasterLimiterAudioProcessor::acceptsMidi() const
{
    return false;
}

bool MasterLimiterAudioProcessor::producesMidi() const
{
    return false;
}

bool MasterLimiterAudioProcessor::isMidiEffect() const
{
    return false;
}

double MasterLimiterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MasterLimiterAudioProcessor::getNumPrograms()
{
    return 1;
}

int MasterLimiterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MasterLimiterAudioProcessor::setCurrentProgram (int)
{
}

const juce::String MasterLimiterAudioProcessor::getProgramName (int)
{
    return {};
}

void MasterLimiterAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void MasterLimiterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MasterLimiterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, static_cast<int> (sizeInBytes)))
    {
        const auto vt = juce::ValueTree::fromXml (*xml);
        if (vt.isValid())
            apvts.replaceState (vt);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MasterLimiterAudioProcessor();
}
