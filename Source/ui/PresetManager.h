#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace master_limiter_ui
{

class PresetManager
{
public:
    static int getNumPresets() noexcept;
    static juce::String getPresetName (int index);
    static bool applyPreset (juce::AudioProcessorValueTreeState& apvts, int index);

    static juce::File getUserPresetsDir();
    static juce::Array<juce::File> listUserPresets();
    static bool saveUserPreset (const juce::AudioProcessorValueTreeState& apvts, const juce::String& name);
    static bool loadUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::File& file);
    static bool deleteUserPreset (const juce::File& file);
};

} // namespace master_limiter_ui
