#include "Parameters.h"
#include "ParameterIDs.h"

namespace
{
juce::ParameterID pid (std::string_view sv, int version)
{
    return { juce::String (sv.data(), static_cast<size_t> (sv.size())), version };
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout Parameters::createParameterLayout()
{
    using namespace juce;
    using namespace param;

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (input_gain_db, 1),
        "Input Gain",
        NormalisableRange<float> (-12.0f, 24.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (ceiling_db, 1),
        "Ceiling",
        NormalisableRange<float> (-12.0f, 0.0f, 0.01f),
        -1.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (release_ms, 1),
        "Release",
        NormalisableRange<float> (1.0f, 1000.0f, 0.0f, 0.3f),
        100.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    const auto releaseAutoAttrs = AudioParameterBoolAttributes()
                                      .withStringFromValueFunction ([] (bool v, int) { return v ? "Auto" : "Off"; })
                                      .withValueFromStringFunction ([] (const String& t)
                                                                    {
                                                                        const auto s = t.trim().toLowerCase();
                                                                        return s == "auto";
                                                                    });

    layout.add (std::make_unique<AudioParameterBool> (
        pid (release_auto, 1),
        "Release Auto",
        false,
        releaseAutoAttrs));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (lookahead_ms, 1),
        "Lookahead",
        NormalisableRange<float> (4.0f, 6.0f, 0.01f),
        5.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterChoice> (
        pid (ceiling_mode, 1),
        "Ceiling Mode",
        StringArray { "SamplePeak", "TruePeak" },
        1,
        AudioParameterChoiceAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (stereo_link_pct, 1),
        "Stereo Link",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (ms_link_pct, 1),
        "M/S Link",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterChoice> (
        pid (character, 1),
        "Character",
        StringArray { "Clean" },
        0,
        AudioParameterChoiceAttributes()));

    return layout;
}
