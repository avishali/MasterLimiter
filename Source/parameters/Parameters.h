#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
struct Parameters
{
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};
