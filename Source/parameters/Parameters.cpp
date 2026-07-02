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
        NormalisableRange<float> (0.0f, 24.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (
        pid (limiter_active, 1),
        "Limiter Active",
        true,
        AudioParameterBoolAttributes()));

    layout.add (std::make_unique<AudioParameterBool> (
        pid (plugin_bypass, 1),
        "Bypass",
        false,
        AudioParameterBoolAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (clipper_drive_db, 1),
        "Clipper",
        NormalisableRange<float> (-12.0f, 0.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterChoice> (
        pid (clipper_mode, 1),
        "Clipper Mode",
        StringArray { "Hard", "Soft" },
        0,
        AudioParameterChoiceAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (ceiling_db, 1),
        "Ceiling",
        NormalisableRange<float> (-24.0f, 0.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (
        pid (gain_ceiling_link, 1),
        "Gain/Ceiling Link",
        false,
        AudioParameterBoolAttributes()));

    layout.add (std::make_unique<AudioParameterBool> (
        pid (gain_match_auto, 1),
        "Auto / Track",
        false,
        AudioParameterBoolAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (io_input_l_db, 1),
        "I/O Input L",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (io_input_r_db, 1),
        "I/O Input R",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (
        pid (io_input_link, 1),
        "I/O Input Link",
        true,
        AudioParameterBoolAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (io_output_l_db, 1),
        "I/O Output L",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (io_output_r_db, 1),
        "I/O Output R",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (
        pid (io_output_link, 1),
        "I/O Output Link",
        true,
        AudioParameterBoolAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (release_ms, 1),
        "Release",
        NormalisableRange<float> (1.0f, 1000.0f, 0.0f, 0.3f),
        30.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (release_sustain_ratio, 1),
        "Release Sustain Ratio",
        NormalisableRange<float> (1.0f, 10.0f, 0.0f),
        4.0f,
        AudioParameterFloatAttributes().withLabel ("")));

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
        true,
        releaseAutoAttrs));

    layout.add (std::make_unique<AudioParameterChoice> (
        pid (auto_release_mode, 1),
        "Auto Release",
        StringArray { "Transparent", "Balanced", "Reactive" },
        2,
        AudioParameterChoiceAttributes()));

    layout.add (std::make_unique<AudioParameterChoice> (
        pid (ceiling_mode, 1),
        "Ceiling Mode",
        StringArray { "SamplePeak", "TruePeak" },
        0,
        AudioParameterChoiceAttributes()));

    layout.add (std::make_unique<AudioParameterChoice> (
        pid (stereo_mode, 1),
        "Stereo Mode",
        StringArray { "Stereo", "M/S" },
        0,
        AudioParameterChoiceAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (stereo_link_pct, 1),
        "Stereo Link",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f,
        AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0) + "%"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (ms_link_pct, 1),
        "M/S Link",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (band_color, 1),
        "Color",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        0.0f,
        AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0) + "%"; })));

    layout.add (std::make_unique<AudioParameterChoice> (
        pid (character, 1),
        "Character",
        StringArray { "Clean", "Tight", "Aggressive" },
        0,
        AudioParameterChoiceAttributes()));

    layout.add (std::make_unique<AudioParameterBool> (
        pid (clipper_active, 1),
        "Clipper Active",
        false,
        AudioParameterBoolAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_low_band_release_scale, 1),
        "DEV Low Release Scale",
        NormalisableRange<float> (0.5f, 8.0f, 0.01f),
        1.52f,
        AudioParameterFloatAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_high_band_release_scale, 1),
        "DEV High/Wide Release Scale",
        NormalisableRange<float> (0.5f, 8.0f, 0.01f),
        1.0f,
        AudioParameterFloatAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_sigma_attack_ms, 1),
        "DEV Sigma Attack",
        NormalisableRange<float> (1.0f, 50.0f, 0.1f),
        17.4f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_sigma_decay_scale, 1),
        "DEV Sigma Decay Scale",
        NormalisableRange<float> (0.5f, 8.0f, 0.01f),
        1.0f,
        AudioParameterFloatAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_attack_ms, 1),
        "DEV Attack",
        NormalisableRange<float> (0.05f, 10.0f, 0.01f, 0.35f),
        2.05f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterChoice> (
        pid (dev_attack_mode, 1),
        "DEV Attack Mode",
        StringArray { "Ramp", "Real" },
        1,
        AudioParameterChoiceAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_real_attack_ms, 1),
        "DEV Real Attack",
        NormalisableRange<float> (0.05f, 100.0f, 0.01f, 0.3f),
        3.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterChoice> (
        pid (dev_release_engine, 1),
        "DEV Release Engine",
        StringArray { "Adaptive", "Lookahead" },
        1,
        AudioParameterChoiceAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_la_release_ms, 1),
        "DEV LA Release",
        NormalisableRange<float> (5.0f, 400.0f, 0.1f, 0.4f),
        8.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterChoice> (
        pid (dev_la_release_poles, 1),
        "DEV LA Poles",
        StringArray { "2", "3", "4" },
        1,
        AudioParameterChoiceAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_lookahead_band_ms, 1),
        "DEV LA Band",
        NormalisableRange<float> (0.0f, 6.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_lookahead_wide_ms, 1),
        "DEV LA Wide",
        NormalisableRange<float> (0.0f, 6.0f, 0.01f),
        0.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_xover_cutoff_hz, 1),
        "DEV Xover Cutoff",
        NormalisableRange<float> (40.0f, 250.0f, 1.0f, 0.35f),
        100.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_xover_transition_hz, 1),
        "DEV Xover Transition",
        NormalisableRange<float> (60.0f, 240.0f, 1.0f, 0.5f),
        120.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_xover_atten_db, 1),
        "DEV Xover Atten",
        NormalisableRange<float> (48.0f, 72.0f, 1.0f),
        60.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_xover_hi_cutoff_hz, 1),
        "DEV Xover Hi Cutoff",
        NormalisableRange<float> (800.0f, 8000.0f, 1.0f, 0.35f),
        2500.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_xover_hi_transition_hz, 1),
        "DEV Xover Hi Transition",
        NormalisableRange<float> (200.0f, 2000.0f, 1.0f, 0.5f),
        600.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_xover_hi_atten_db, 1),
        "DEV Xover Hi Atten",
        NormalisableRange<float> (48.0f, 72.0f, 1.0f),
        60.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_mid_band_release_scale, 1),
        "DEV Mid Release Scale",
        NormalisableRange<float> (0.5f, 8.0f, 0.01f),
        1.0f,
        AudioParameterFloatAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        pid (dev_band_stereo_link_pct, 1),
        "DEV Band Stereo Link",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterBool> (
        pid (dev_ms_safety_clamp, 1),
        "DEV M/S Safety Clamp",
        true,
        AudioParameterBoolAttributes()));

    layout.add (std::make_unique<AudioParameterBool> (
        pid (dev_final_ceiling, 1),
        "DEV Final Ceiling",
        true,
        AudioParameterBoolAttributes()));

    return layout;
}
