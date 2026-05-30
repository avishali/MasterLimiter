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
};

} // namespace master_limiter_ui
