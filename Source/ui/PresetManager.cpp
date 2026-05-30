#include "PresetManager.h"

#include <array>
#include <cmath>
#include <string_view>

#include "parameters/ParameterIDs.h"

namespace
{

struct FactoryPreset
{
    const char* name = "";
    float inputGainDb = 0.0f;
    float ceilingDb = -1.0f;
    int ceilingMode = 0;
    float clipperDriveDb = 0.0f;
    int clipperMode = 0;
    int character = 0;
    float releaseMs = 30.0f;
    bool releaseAuto = false;
    int autoReleaseMode = 0;
    int stereoMode = 0;
    float stereoLinkPct = 100.0f;
    float bandColor = 50.0f;
};

constexpr std::array<FactoryPreset, 5> kFactoryPresets {{
    { "Default",             0.0f, -1.0f, 0,  0.0f, 0, 0,  30.0f, false, 0, 0, 100.0f, 50.0f },
    { "Transparent Master",  3.0f, -1.0f, 1,  0.0f, 0, 0,  30.0f, true,  0, 0, 100.0f, 50.0f },
    { "Loud & Aggressive",   9.0f, -1.0f, 1, -3.0f, 1, 2,  30.0f, true,  2, 0, 100.0f, 25.0f },
    { "Gentle Glue",         2.0f, -1.0f, 0,  0.0f, 0, 1, 120.0f, false, 0, 0, 100.0f, 70.0f },
    { "Clipper Punch",       4.0f, -1.0f, 0, -4.0f, 0, 1,  30.0f, false, 0, 0, 100.0f, 40.0f },
}};

juce::String pid (std::string_view sv)
{
    return { sv.data(), static_cast<size_t> (sv.size()) };
}

void setParameterValue (juce::AudioProcessorValueTreeState& apvts, std::string_view paramId, float plainValue)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid (paramId)));
    jassert (parameter != nullptr);

    if (parameter == nullptr)
        return;

    const float normalised = parameter->convertTo0to1 (plainValue);
    if (std::abs (parameter->getValue() - normalised) <= 0.000001f)
        return;

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (normalised);
    parameter->endChangeGesture();
}

} // namespace

namespace master_limiter_ui
{

int PresetManager::getNumPresets() noexcept
{
    return static_cast<int> (kFactoryPresets.size());
}

juce::String PresetManager::getPresetName (int index)
{
    if (index < 0 || index >= getNumPresets())
        return {};

    return kFactoryPresets[static_cast<size_t> (index)].name;
}

bool PresetManager::applyPreset (juce::AudioProcessorValueTreeState& apvts, int index)
{
    if (index < 0 || index >= getNumPresets())
        return false;

    const auto& preset = kFactoryPresets[static_cast<size_t> (index)];

    setParameterValue (apvts, param::input_gain_db, preset.inputGainDb);
    setParameterValue (apvts, param::ceiling_db, preset.ceilingDb);
    setParameterValue (apvts, param::ceiling_mode, static_cast<float> (preset.ceilingMode));
    setParameterValue (apvts, param::limiter_active, 1.0f);
    setParameterValue (apvts, param::clipper_drive_db, preset.clipperDriveDb);
    setParameterValue (apvts, param::clipper_mode, static_cast<float> (preset.clipperMode));
    setParameterValue (apvts, param::character, static_cast<float> (preset.character));
    setParameterValue (apvts, param::release_ms, preset.releaseMs);
    setParameterValue (apvts, param::release_auto, preset.releaseAuto ? 1.0f : 0.0f);
    setParameterValue (apvts, param::auto_release_mode, static_cast<float> (preset.autoReleaseMode));
    setParameterValue (apvts, param::stereo_mode, static_cast<float> (preset.stereoMode));
    setParameterValue (apvts, param::stereo_link_pct, preset.stereoLinkPct);
    setParameterValue (apvts, param::ms_link_pct, 100.0f);
    setParameterValue (apvts, param::band_color, preset.bandColor);
    setParameterValue (apvts, param::gain_ceiling_link, 0.0f);
    setParameterValue (apvts, param::gain_match_auto, 0.0f);
    setParameterValue (apvts, param::io_input_l_db, 0.0f);
    setParameterValue (apvts, param::io_input_r_db, 0.0f);
    setParameterValue (apvts, param::io_input_link, 0.0f);
    setParameterValue (apvts, param::io_output_l_db, 0.0f);
    setParameterValue (apvts, param::io_output_r_db, 0.0f);
    setParameterValue (apvts, param::io_output_link, 0.0f);

    return true;
}

} // namespace master_limiter_ui
