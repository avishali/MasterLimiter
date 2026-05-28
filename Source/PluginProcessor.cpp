#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

#include "parameters/ParameterIDs.h"
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
    jassert (apvts.getParameter ("io_input_l_db") != nullptr);
    jassert (apvts.getParameter ("io_input_r_db") != nullptr);
    jassert (apvts.getParameter ("io_input_link") != nullptr);
    jassert (apvts.getParameter ("io_output_l_db") != nullptr);
    jassert (apvts.getParameter ("io_output_r_db") != nullptr);
    jassert (apvts.getParameter ("io_output_link") != nullptr);
    jassert (apvts.getParameter ("release_ms") != nullptr);
    jassert (apvts.getParameter ("release_sustain_ratio") != nullptr);
    jassert (apvts.getParameter ("gain_ceiling_link") != nullptr);

    apvts.addParameterListener (param::input_gain_db.data(), this);
    apvts.addParameterListener (param::ceiling_db.data(), this);
    apvts.addParameterListener (param::gain_ceiling_link.data(), this);
}

MasterLimiterAudioProcessor::~MasterLimiterAudioProcessor()
{
    apvts.removeParameterListener (param::input_gain_db.data(), this);
    apvts.removeParameterListener (param::ceiling_db.data(), this);
    apvts.removeParameterListener (param::gain_ceiling_link.data(), this);
}

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

    mdsp_dsp::IspTrimStage::Spec tspec;
    tspec.sampleRate    = sampleRate;
    tspec.numChannels   = 2;
    tspec.maxBlockSize  = samplesPerBlock;
    ispTrim_.prepare (tspec);

    peakBuf_.setSize (1, samplesPerBlock, false, true, true);
    gainBuf_.setSize (1, samplesPerBlock, false, true, true);

    ceilingMode_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("ceiling_mode"));
    jassert (ceilingMode_ != nullptr);

    cacheGainCeilingLinkParameters();

    releaseSustainRatio_ = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("release_sustain_ratio"));
    jassert (releaseSustainRatio_ != nullptr);

    ioInputLDb_ = apvts.getRawParameterValue ("io_input_l_db");
    ioInputRDb_ = apvts.getRawParameterValue ("io_input_r_db");
    ioOutputLDb_ = apvts.getRawParameterValue ("io_output_l_db");
    ioOutputRDb_ = apvts.getRawParameterValue ("io_output_r_db");
    jassert (ioInputLDb_ != nullptr);
    jassert (ioInputRDb_ != nullptr);
    jassert (ioOutputLDb_ != nullptr);
    jassert (ioOutputRDb_ != nullptr);

    ioInputLink_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("io_input_link"));
    ioOutputLink_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("io_output_link"));
    jassert (ioInputLink_ != nullptr);
    jassert (ioOutputLink_ != nullptr);

    baseLatencySamples_ = lookaheadSamples;
    osLatencySamples_   = ispTrim_.getLatencyInSamples();
    cachedCeilingMode_  = ceilingMode_->getIndex();

    lookahead_.reset();
    envelope_.reset();
    ispTrim_.reset();

    setLatencySamples (cachedCeilingMode_ == 1 ? (baseLatencySamples_ + osLatencySamples_)
                                               : baseLatencySamples_);

    loudness_.prepare (sampleRate, samplesPerBlock);
    loudness_.reset();
}

void MasterLimiterAudioProcessor::releaseResources()
{
}

void MasterLimiterAudioProcessor::cacheGainCeilingLinkParameters()
{
    inputGainDbParam_ = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (param::input_gain_db.data()));
    ceilingDbParam_ = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (param::ceiling_db.data()));
    gainCeilingLink_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (param::gain_ceiling_link.data()));

    jassert (inputGainDbParam_ != nullptr);
    jassert (ceilingDbParam_ != nullptr);
    jassert (gainCeilingLink_ != nullptr);

    refreshGainCeilingLinkBaseline();
    gainCeilingLinkWasEnabled_.store (gainCeilingLink_ != nullptr && gainCeilingLink_->get(), std::memory_order_relaxed);
}

void MasterLimiterAudioProcessor::refreshGainCeilingLinkBaseline()
{
    if (inputGainDbParam_ == nullptr || ceilingDbParam_ == nullptr)
        return;

    lastLinkedInputGainDb_.store (inputGainDbParam_->get(), std::memory_order_relaxed);
    lastLinkedCeilingDb_.store (ceilingDbParam_->get(), std::memory_order_relaxed);
}

void MasterLimiterAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == param::gain_ceiling_link.data())
    {
        const bool enabled = newValue >= 0.5f;
        gainCeilingLinkWasEnabled_.store (enabled, std::memory_order_relaxed);

        if (enabled)
            refreshGainCeilingLinkBaseline();

        return;
    }

    const bool inputChanged = parameterID == param::input_gain_db.data();
    const bool ceilingChanged = parameterID == param::ceiling_db.data();

    if (! inputChanged && ! ceilingChanged)
        return;

    auto storeChangedValue = [&]
    {
        if (inputChanged)
            lastLinkedInputGainDb_.store (newValue, std::memory_order_relaxed);
        else
            lastLinkedCeilingDb_.store (newValue, std::memory_order_relaxed);
    };

    if (inputGainDbParam_ == nullptr || ceilingDbParam_ == nullptr || gainCeilingLink_ == nullptr)
    {
        storeChangedValue();
        return;
    }

    if (couplingInProgress_.load (std::memory_order_relaxed))
    {
        storeChangedValue();
        return;
    }

    if (! gainCeilingLink_->get())
    {
        gainCeilingLinkWasEnabled_.store (false, std::memory_order_relaxed);
        storeChangedValue();
        return;
    }

    gainCeilingLinkWasEnabled_.store (true, std::memory_order_relaxed);

    auto& targetParam = inputChanged ? *ceilingDbParam_ : *inputGainDbParam_;
    auto& targetLast = inputChanged ? lastLinkedCeilingDb_ : lastLinkedInputGainDb_;
    auto& sourceLast = inputChanged ? lastLinkedInputGainDb_ : lastLinkedCeilingDb_;

    const float previousSourceValue = sourceLast.load (std::memory_order_relaxed);
    const float previousTargetValue = targetLast.load (std::memory_order_relaxed);
    const float delta = newValue - previousSourceValue;
    const float rangeStart = targetParam.convertFrom0to1 (0.0f);
    const float rangeEnd = targetParam.convertFrom0to1 (1.0f);
    const float targetValue = juce::jlimit (std::min (rangeStart, rangeEnd),
                                           std::max (rangeStart, rangeEnd),
                                           previousTargetValue - delta);

    sourceLast.store (newValue, std::memory_order_relaxed);
    targetLast.store (targetValue, std::memory_order_relaxed);

    if (std::abs (targetValue - previousTargetValue) <= 0.00001f)
        return;

    couplingInProgress_.store (true, std::memory_order_relaxed);
    targetParam.setValueNotifyingHost (targetParam.convertTo0to1 (targetValue));
    couplingInProgress_.store (false, std::memory_order_relaxed);
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

    if (inputGainDbParam_ == nullptr || ceilingDbParam_ == nullptr
        || apvts.getParameter ("release_ms") == nullptr || releaseSustainRatio_ == nullptr
        || ceilingMode_ == nullptr || gainCeilingLink_ == nullptr || ioInputLDb_ == nullptr || ioInputRDb_ == nullptr
        || ioOutputLDb_ == nullptr || ioOutputRDb_ == nullptr || ioInputLink_ == nullptr
        || ioOutputLink_ == nullptr)
        return;

    const int n = buffer.getNumSamples();
    if (n <= 0)
        return;

    const int nch = juce::jmin (2, buffer.getNumChannels());
    if (nch <= 0)
        return;

    const float ioInputLGain = juce::Decibels::decibelsToGain (ioInputLDb_->load (std::memory_order_relaxed));
    const float ioInputRGain = juce::Decibels::decibelsToGain (ioInputRDb_->load (std::memory_order_relaxed));
    buffer.applyGain (0, 0, n, ioInputLGain);
    if (nch > 1)
        buffer.applyGain (1, 0, n, ioInputRGain);

    {
        const float pL = buffer.getMagnitude (0, 0, n);
        const float pR = (nch > 1) ? buffer.getMagnitude (1, 0, n) : pL;
        inputPeakLDb_.store (juce::Decibels::gainToDecibels (pL, -100.0f), std::memory_order_relaxed);
        inputPeakRDb_.store (juce::Decibels::gainToDecibels (pR, -100.0f), std::memory_order_relaxed);
    }

    const float inGainLin = juce::Decibels::decibelsToGain (inputGainDbParam_->get());
    for (int ch = 0; ch < nch; ++ch)
        buffer.applyGain (ch, 0, n, inGainLin);

    auto* peak = peakBuf_.getWritePointer (0);
    const float* const ch0 = buffer.getReadPointer (0);
    const float* const ch1 = nch > 1 ? buffer.getReadPointer (1) : ch0;
    const float* const channelPtrs[2] = { ch0, ch1 };

    for (int i = 0; i < n; ++i)
        peak[i] = peakDetector_.process (channelPtrs, nch, i);

    const float thresholdLin = juce::Decibels::decibelsToGain (ceilingDbParam_->get());
    envelope_.setThresholdLinear (thresholdLin);
    envelope_.setReleaseMs (readFloatParam (apvts, "release_ms"));
    envelope_.setReleaseSustainRatio (releaseSustainRatio_->get());

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

    const int modeIdx = ceilingMode_->getIndex();

    if (modeIdx != cachedCeilingMode_)
    {
        const int newLatency = (modeIdx == 1) ? (baseLatencySamples_ + osLatencySamples_)
                                              : baseLatencySamples_;
        setLatencySamples (newLatency);
        ispTrim_.reset();
        cachedCeilingMode_ = modeIdx;
    }

    if (modeIdx == 1)
    {
        ispTrim_.setCeilingLinear (thresholdLin);
        ispTrim_.process (buffer);
        currentTpTrimDb_.store (ispTrim_.getLastBlockMaxTpDbReduction(), std::memory_order_relaxed);
    }
    else
    {
        currentTpTrimDb_.store (0.0f, std::memory_order_relaxed);
    }

    const float ioOutputLGain = juce::Decibels::decibelsToGain (ioOutputLDb_->load (std::memory_order_relaxed));
    const float ioOutputRGain = juce::Decibels::decibelsToGain (ioOutputRDb_->load (std::memory_order_relaxed));
    buffer.applyGain (0, 0, n, ioOutputLGain);
    if (nch > 1)
        buffer.applyGain (1, 0, n, ioOutputRGain);

    {
        const float pL = buffer.getMagnitude (0, 0, n);
        const float pR = (nch > 1) ? buffer.getMagnitude (1, 0, n) : pL;
        const float outLdb = juce::Decibels::gainToDecibels (pL, -100.0f);
        const float outRdb = juce::Decibels::gainToDecibels (pR, -100.0f);
        outputPeakLDb_.store (outLdb, std::memory_order_relaxed);
        outputPeakRDb_.store (outRdb, std::memory_order_relaxed);
        outputTpDb_.store (std::max (outLdb, outRdb), std::memory_order_relaxed);

        loudness_.process (buffer);
    }
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
