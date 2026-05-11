#include "PluginProcessor.h"
#include "PluginEditor.h"

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
}

MasterLimiterAudioProcessor::~MasterLimiterAudioProcessor() = default;

void MasterLimiterAudioProcessor::prepareToPlay (double, int)
{
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
    juce::ignoreUnused (buffer, midi);
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
