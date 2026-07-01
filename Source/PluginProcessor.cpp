#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "parameters/ParameterIDs.h"
#include "parameters/Parameters.h"
#include "ui/PresetManager.h"

namespace
{
constexpr const char* kStateWrapperType = "MasterLimiterState";
constexpr const char* kLearnedRefLufsProperty = "learned_ref_lufs";
constexpr const char* kCompareStateType = "ABCompareState";
constexpr const char* kCompareSlotAType = "slotA";
constexpr const char* kCompareSlotBType = "slotB";
constexpr const char* kCompareActiveSlotProperty = "active_slot";

float readFloatParam (juce::AudioProcessorValueTreeState& apvts, const char* paramId)
{
    auto* p = apvts.getParameter (paramId);
    jassert (p != nullptr);
    return (float) static_cast<juce::RangedAudioParameter*> (p)->convertFrom0to1 (p->getValue());
}

float mapBandColorToLink (float colorPct) noexcept
{
    return 1.0f - juce::jlimit (0.0f, 100.0f, colorPct) * 0.01f;
}

float xoFadeOutGain (int pos, int len) noexcept
{
    return 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * (float) pos / (float) len));
}

float xoFadeInGain (int pos, int len) noexcept
{
    return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (float) pos / (float) len));
}

mdsp_dsp::LimiterEnvelope::Mode mapCharacterIndexToMode (int index) noexcept
{
    switch (index)
    {
        case 0:  return mdsp_dsp::LimiterEnvelope::Mode::Clean;
        case 2:  return mdsp_dsp::LimiterEnvelope::Mode::Aggressive;
        default: return mdsp_dsp::LimiterEnvelope::Mode::Tight;
    }
}

mdsp_dsp::LimiterEnvelope::AutoReleaseMode mapAutoReleaseModeIndexToMode (int index) noexcept
{
    switch (index)
    {
        case 1:  return mdsp_dsp::LimiterEnvelope::AutoReleaseMode::Balanced;
        case 2:  return mdsp_dsp::LimiterEnvelope::AutoReleaseMode::Reactive;
        default: return mdsp_dsp::LimiterEnvelope::AutoReleaseMode::Transparent;
    }
}

float meanSquareForChannel (const juce::AudioBuffer<float>& buffer, int channel, int numSamples) noexcept
{
    if (channel < 0 || channel >= buffer.getNumChannels() || numSamples <= 0)
        return 0.0f;

    const auto* data = buffer.getReadPointer (channel);
    double sum = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const double sample = static_cast<double> (data[i]);
        sum += sample * sample;
    }

    return static_cast<float> (sum / static_cast<double> (numSamples));
}

float rmsDbFromMeanSquare (float meanSquare) noexcept
{
    return juce::Decibels::gainToDecibels (std::sqrt (juce::jmax (0.0f, meanSquare)), -120.0f);
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
    jassert (apvts.getParameter ("limiter_active") != nullptr);
    jassert (apvts.getParameter ("plugin_bypass") != nullptr);
    jassert (apvts.getParameter ("clipper_drive_db") != nullptr);
    jassert (apvts.getParameter ("clipper_mode") != nullptr);
    jassert (apvts.getParameter ("clipper_active") != nullptr);
    jassert (apvts.getParameter ("ceiling_db") != nullptr);
    jassert (apvts.getParameter ("io_input_l_db") != nullptr);
    jassert (apvts.getParameter ("io_input_r_db") != nullptr);
    jassert (apvts.getParameter ("io_input_link") != nullptr);
    jassert (apvts.getParameter ("io_output_l_db") != nullptr);
    jassert (apvts.getParameter ("io_output_r_db") != nullptr);
    jassert (apvts.getParameter ("io_output_link") != nullptr);
    jassert (apvts.getParameter ("release_ms") != nullptr);
    jassert (apvts.getParameter ("release_sustain_ratio") != nullptr);
    jassert (apvts.getParameter ("release_auto") != nullptr);
    jassert (apvts.getParameter ("auto_release_mode") != nullptr);
    jassert (apvts.getParameter ("gain_ceiling_link") != nullptr);
    jassert (apvts.getParameter ("gain_match_auto") != nullptr);
    jassert (apvts.getParameter ("stereo_mode") != nullptr);
    jassert (apvts.getParameter ("stereo_link_pct") != nullptr);
    jassert (apvts.getParameter ("ms_link_pct") != nullptr);
    jassert (apvts.getParameter ("character") != nullptr);
    jassert (apvts.getParameter (param::dev_low_band_release_scale.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_high_band_release_scale.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_sigma_attack_ms.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_sigma_decay_scale.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_attack_ms.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_attack_mode.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_real_attack_ms.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_release_engine.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_la_release_ms.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_la_release_poles.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_lookahead_band_ms.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_lookahead_wide_ms.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_xover_cutoff_hz.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_xover_transition_hz.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_xover_atten_db.data()) != nullptr);
    jassert (apvts.getParameter (param::dev_band_stereo_link_pct.data()) != nullptr);

    pluginBypass_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (param::plugin_bypass.data()));
    jassert (pluginBypass_ != nullptr);

    apvts.addParameterListener (param::input_gain_db.data(), this);
    apvts.addParameterListener (param::ceiling_db.data(), this);
    apvts.addParameterListener (param::gain_ceiling_link.data(), this);
    apvts.addParameterListener (param::dev_xover_cutoff_hz.data(), this);
    apvts.addParameterListener (param::dev_xover_transition_hz.data(), this);
    apvts.addParameterListener (param::dev_xover_atten_db.data(), this);
    apvts.addParameterListener (param::dev_lookahead_band_ms.data(), this);
    apvts.addParameterListener (param::dev_lookahead_wide_ms.data(), this);

    for (auto id : { param::dev_xover_cutoff_hz, param::dev_xover_transition_hz, param::dev_xover_atten_db,
                     param::dev_lookahead_band_ms, param::dev_lookahead_wide_ms })
        if (auto* p = apvts.getParameter (id.data()))
            p->addListener (this);
}

MasterLimiterAudioProcessor::~MasterLimiterAudioProcessor()
{
    for (auto id : { param::dev_xover_cutoff_hz, param::dev_xover_transition_hz, param::dev_xover_atten_db,
                     param::dev_lookahead_band_ms, param::dev_lookahead_wide_ms })
        if (auto* p = apvts.getParameter (id.data()))
            p->removeListener (this);

    stopTimer();
    cancelPendingUpdate();
    apvts.removeParameterListener (param::input_gain_db.data(), this);
    apvts.removeParameterListener (param::ceiling_db.data(), this);
    apvts.removeParameterListener (param::gain_ceiling_link.data(), this);
    apvts.removeParameterListener (param::dev_xover_cutoff_hz.data(), this);
    apvts.removeParameterListener (param::dev_xover_transition_hz.data(), this);
    apvts.removeParameterListener (param::dev_xover_atten_db.data(), this);
    apvts.removeParameterListener (param::dev_lookahead_band_ms.data(), this);
    apvts.removeParameterListener (param::dev_lookahead_wide_ms.data(), this);
}

void MasterLimiterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    constexpr int osFactor = 4;
    constexpr double controlSmoothingSeconds = 0.02;
    const int baseLookaheadSamples = static_cast<int> (std::llround ((double) kLookaheadMs * 1.0e-3 * sampleRate));
    const int baseMaxLookaheadSamples = static_cast<int> (std::llround ((double) kMaxLookaheadMs * 1.0e-3 * sampleRate));
    constexpr int finalCeilingLookaheadSamples = 64;
    const int osLookaheadSamples = baseLookaheadSamples * osFactor;
    const int osMaxLookaheadSamples = baseMaxLookaheadSamples * osFactor;
    const int osMaxBlockSize = samplesPerBlock * osFactor;
    const double osSampleRate = sampleRate * (double) osFactor;
    osMaxLookaheadSamples_ = juce::jmax (1, osMaxLookaheadSamples);
    osSampleRate_ = osSampleRate;

    // Custom linear-phase 4× oversampler (mdsp_dsp::HalfbandPolyphaseOS) — same half-band
    // specs as the previous juce::dsp::Oversampling (0.03/-110 + 0.10/-100) but with EXACT
    // integer host latency (no setUsingIntegerLatency fractional rounding), which removes the
    // residual HF phase tilt. See docs/CUSTOM_FILTERS.md F-3.
    limiterOversampler_.prepare (2, samplesPerBlock, sampleRate);
    limiterOversampler_.reset();

    limiterOsLatencySamples_ = limiterOversampler_.getLatencyInSamples();

    // Clipper-only 2× OS at the 4× rate — gentle spec; suppresses images in the top octave of the 4× band.
    clipperOversampler_.prepare (2, osMaxBlockSize, osSampleRate, { { 0.10, 90.0 } });
    clipperOversampler_.reset();
    const int rawClipperOsLat4x = clipperOversampler_.getLatencyInSamples();
    const int paddedClipperOsLat4x = ((rawClipperOsLat4x + 3) / 4) * 4;
    clipperOsAlignPad4x_ = paddedClipperOsLat4x - rawClipperOsLat4x;
    clipperOsLatencySamples4x_ = paddedClipperOsLat4x;
    clipperOsLatencyHostSamples_ = paddedClipperOsLat4x / 4;

    juce::dsp::ProcessSpec clipperOsAlignSpec;
    clipperOsAlignSpec.sampleRate       = osSampleRate;
    clipperOsAlignSpec.maximumBlockSize = static_cast<juce::uint32> (osMaxBlockSize);
    clipperOsAlignSpec.numChannels      = 2;
    clipperOsAlignDelay_.prepare (clipperOsAlignSpec);
    clipperOsAlignDelay_.setMaximumDelayInSamples (std::max (1, clipperOsAlignPad4x_));
    clipperOsAlignDelay_.setDelay (static_cast<float> (clipperOsAlignPad4x_));
    clipperOsAlignDelay_.reset();

    lookahead_.prepare (2, osMaxLookaheadSamples_);
    lookahead_.setDelaySamples (osLookaheadSamples);
    lookaheadWide_.prepare (2, osMaxLookaheadSamples_);
    lookaheadWide_.setDelaySamples (osLookaheadSamples);
    lookaheadPad_.prepare (2, 2 * osMaxLookaheadSamples_);
    peakDetector_.prepare (2);

    mdsp_dsp::LimiterEnvelope::Spec spec;
    spec.sampleRate = osSampleRate;
    spec.lookaheadSamples = osMaxLookaheadSamples_;
    spec.maxBlockSize = osMaxBlockSize;
    envelope_.prepare (spec);
    envelope_R_.prepare (spec);

    envelopeLow_.prepare (spec);
    envelopeHigh_.prepare (spec);
    envelopeLowR_.prepare (spec);
    envelopeHighR_.prepare (spec);

    peakBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gainBuf_.setSize (1, osMaxBlockSize, false, true, true);
    peakBufR_.setSize (1, osMaxBlockSize, false, true, true);
    gainBufR_.setSize (1, osMaxBlockSize, false, true, true);
    peakLowBuf_.setSize (1, osMaxBlockSize, false, true, true);
    peakLowLBuf_.setSize (1, osMaxBlockSize, false, true, true);
    peakLowRBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gainLowBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gainLowLBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gainLowRBuf_.setSize (1, osMaxBlockSize, false, true, true);
    peakHighBuf_.setSize (1, osMaxBlockSize, false, true, true);
    peakHighLBuf_.setSize (1, osMaxBlockSize, false, true, true);
    peakHighRBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gainHighBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gainHighLBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gainHighRBuf_.setSize (1, osMaxBlockSize, false, true, true);
    bandLowBuf_.setSize (2, osMaxBlockSize, false, true, true);
    bandHighBuf_.setSize (2, osMaxBlockSize, false, true, true);
    gLowOutBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gHighOutBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gLowOutRBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gHighOutRBuf_.setSize (1, osMaxBlockSize, false, true, true);
    bandLimitedBuf_.setSize (2, osMaxBlockSize, false, true, true);

    mdsp_dsp::FinalCeilingLimiter::Spec finalCeilingSpec;
    finalCeilingSpec.sampleRate = sampleRate;
    finalCeilingSpec.numChannels = 2;
    finalCeilingSpec.maxBlockSize = samplesPerBlock;
    finalCeilingSpec.lookaheadSamples = finalCeilingLookaheadSamples;
    finalCeiling_.prepare (finalCeilingSpec);
    finalCeilingLatencySamples_ = finalCeiling_.getLatencyInSamples();

    mdsp_dsp::TruePeakDetector::Spec outputTpSpec;
    outputTpSpec.numChannels = 1;
    outputTpSpec.maxBlockSize = samplesPerBlock;
    inputTruePeakL_.prepare (outputTpSpec);
    inputTruePeakR_.prepare (outputTpSpec);
    outputTruePeakL_.prepare (outputTpSpec);
    outputTruePeakR_.prepare (outputTpSpec);

    ceilingMode_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("ceiling_mode"));
    stereoMode_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("stereo_mode"));
    characterChoice_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("character"));
    clipperMode_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("clipper_mode"));
    jassert (ceilingMode_ != nullptr);
    jassert (stereoMode_ != nullptr);
    jassert (characterChoice_ != nullptr);
    jassert (clipperMode_ != nullptr);

    cacheGainCeilingLinkParameters();

    limiterActive_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("limiter_active"));
    pluginBypass_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("plugin_bypass"));
    clipperActive_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("clipper_active"));
    clipperDriveDb_ = apvts.getRawParameterValue ("clipper_drive_db");
    stereoLinkPct_ = apvts.getRawParameterValue ("stereo_link_pct");
    msLinkPct_ = apvts.getRawParameterValue ("ms_link_pct");
    bandColor_ = apvts.getRawParameterValue ("band_color");
    gainMatchAuto_ = apvts.getRawParameterValue ("gain_match_auto");
    devLowBandReleaseScale_ = apvts.getRawParameterValue (param::dev_low_band_release_scale.data());
    devHighBandReleaseScale_ = apvts.getRawParameterValue (param::dev_high_band_release_scale.data());
    devSigmaAttackMs_ = apvts.getRawParameterValue (param::dev_sigma_attack_ms.data());
    devSigmaDecayScale_ = apvts.getRawParameterValue (param::dev_sigma_decay_scale.data());
    devAttackMs_ = apvts.getRawParameterValue (param::dev_attack_ms.data());
    devAttackMode_ = apvts.getRawParameterValue (param::dev_attack_mode.data());
    devRealAttackMs_ = apvts.getRawParameterValue (param::dev_real_attack_ms.data());
    devReleaseEngine_ = apvts.getRawParameterValue (param::dev_release_engine.data());
    devLaReleaseMs_ = apvts.getRawParameterValue (param::dev_la_release_ms.data());
    devLaReleasePoles_ = apvts.getRawParameterValue (param::dev_la_release_poles.data());
    devLookaheadBandMs_ = apvts.getRawParameterValue (param::dev_lookahead_band_ms.data());
    devLookaheadWideMs_ = apvts.getRawParameterValue (param::dev_lookahead_wide_ms.data());
    committedLookaheadBandMs_.store (devLookaheadBandMs_ != nullptr ? devLookaheadBandMs_->load() : kLookaheadMs,
                                     std::memory_order_release);
    committedLookaheadWideMs_.store (devLookaheadWideMs_ != nullptr ? devLookaheadWideMs_->load() : kLookaheadMs,
                                     std::memory_order_release);
    devXoverCutoffHz_ = apvts.getRawParameterValue (param::dev_xover_cutoff_hz.data());
    devXoverTransitionHz_ = apvts.getRawParameterValue (param::dev_xover_transition_hz.data());
    devXoverAttenDb_ = apvts.getRawParameterValue (param::dev_xover_atten_db.data());
    devBandStereoLinkPct_ = apvts.getRawParameterValue (param::dev_band_stereo_link_pct.data());
    jassert (limiterActive_ != nullptr);
    jassert (pluginBypass_ != nullptr);
    jassert (clipperActive_ != nullptr);
    jassert (clipperDriveDb_ != nullptr);
    jassert (stereoLinkPct_ != nullptr);
    jassert (msLinkPct_ != nullptr);
    jassert (bandColor_ != nullptr);
    jassert (gainMatchAuto_ != nullptr);
    jassert (devLowBandReleaseScale_ != nullptr);
    jassert (devHighBandReleaseScale_ != nullptr);
    jassert (devSigmaAttackMs_ != nullptr);
    jassert (devSigmaDecayScale_ != nullptr);
    jassert (devAttackMs_ != nullptr);
    jassert (devAttackMode_ != nullptr);
    jassert (devRealAttackMs_ != nullptr);
    jassert (devReleaseEngine_ != nullptr);
    jassert (devLaReleaseMs_ != nullptr);
    jassert (devLaReleasePoles_ != nullptr);
    jassert (devLookaheadBandMs_ != nullptr);
    jassert (devLookaheadWideMs_ != nullptr);
    jassert (devXoverCutoffHz_ != nullptr);
    jassert (devXoverTransitionHz_ != nullptr);
    jassert (devXoverAttenDb_ != nullptr);
    bandLinkSmoothed_.reset (osSampleRate, 0.02);
    bandLinkSmoothed_.setCurrentAndTargetValue (mapBandColorToLink (bandColor_ != nullptr ? bandColor_->load (std::memory_order_relaxed) : 0.0f));

    prepareCrossoverBanks (osSampleRate);

    releaseSustainRatio_ = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("release_sustain_ratio"));
    releaseAuto_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("release_auto"));
    autoReleaseMode_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("auto_release_mode"));
    jassert (releaseSustainRatio_ != nullptr);
    jassert (releaseAuto_ != nullptr);
    jassert (autoReleaseMode_ != nullptr);

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

    inputGainSmoothed_.reset (osSampleRate, controlSmoothingSeconds);
    inputGainSmoothed_.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputGainDbParam_ != nullptr ? inputGainDbParam_->get() : 0.0f));
    clipperDriveSmoothed_.reset (osSampleRate, controlSmoothingSeconds);
    clipperDriveSmoothed_.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (clipperDriveDb_ != nullptr ? -clipperDriveDb_->load (std::memory_order_relaxed) : 0.0f));
    ioInputGainLSmoothed_.reset (sampleRate, controlSmoothingSeconds);
    ioInputGainLSmoothed_.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (ioInputLDb_ != nullptr ? ioInputLDb_->load (std::memory_order_relaxed) : 0.0f));
    ioInputGainRSmoothed_.reset (sampleRate, controlSmoothingSeconds);
    ioInputGainRSmoothed_.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (ioInputRDb_ != nullptr ? ioInputRDb_->load (std::memory_order_relaxed) : 0.0f));
    ioOutputGainLSmoothed_.reset (sampleRate, controlSmoothingSeconds);
    ioOutputGainLSmoothed_.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (ioOutputLDb_ != nullptr ? ioOutputLDb_->load (std::memory_order_relaxed) : 0.0f));
    ioOutputGainRSmoothed_.reset (sampleRate, controlSmoothingSeconds);
    ioOutputGainRSmoothed_.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (ioOutputRDb_ != nullptr ? ioOutputRDb_->load (std::memory_order_relaxed) : 0.0f));

    baseLatencySamples_ = (2 * baseMaxLookaheadSamples) + limiterOsLatencySamples_ + finalCeilingLatencySamples_
                        + crossoverOsLatencyHostSamples_ + clipperOsLatencyHostSamples_;
    cachedCeilingMode_  = ceilingMode_->getIndex();

    limiterOversampler_.reset();
    clipperOversampler_.reset();
    clipperOsAlignDelay_.reset();
    lookahead_.reset();
    lookaheadWide_.reset();
    lookaheadPad_.reset();
    envelope_.reset();
    envelope_R_.reset();
    detectCrossover_[0].reset();
    detectCrossover_[1].reset();
    applyCrossover_[0].reset();
    applyCrossover_[1].reset();
    envelopeLow_.reset();
    envelopeHigh_.reset();
    envelopeLowR_.reset();
    envelopeHighR_.reset();
    finalCeiling_.reset();
    inputTruePeakL_.reset();
    inputTruePeakR_.reset();
    outputTruePeakL_.reset();
    outputTruePeakR_.reset();
    resetInputTruePeakHolds();
    resetOutputTruePeakHolds();

    setLatencySamples (baseLatencySamples_);

    loudness_.prepare (sampleRate, samplesPerBlock);
    loudness_.reset();
    loudnessRef_.prepare (sampleRate, samplesPerBlock);
    loudnessRef_.reset();
    loudnessTrack_.prepare (sampleRate, samplesPerBlock);
    loudnessTrack_.reset();

    learnWindowSamples_ = static_cast<int> (std::llround (sampleRate * 3.0));
    armedSamplesElapsed_ = 0;
    compGainDbSmoothed_ = 0.0f;
    compGainDbMirror_.store (0.0f, std::memory_order_relaxed);
    compActiveLastBlock_ = false;

    const double tauSec = 1.0;
    const double blocksPerSec = sampleRate / static_cast<double> (std::max (1, samplesPerBlock));
    compGainSmoothCoef_ = static_cast<float> (1.0 - std::exp (-1.0 / (tauSec * blocksPerSec)));

    juce::dsp::ProcessSpec dryDelaySpec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 2 };
    dryDelay_.prepare (dryDelaySpec);
    dryDelay_.reset();
    dryDelay_.setDelay (static_cast<float> (baseLatencySamples_));

    dryScratch_.setSize (2, std::max (samplesPerBlock, 4096), false, false, true);
    dryScratch_.clear();
    grEnvBuf_.assign ((size_t) samplesPerBlock, 1.0f);
    clipEnvBuf_.assign ((size_t) samplesPerBlock, 0.0f);

    bypassFade_.reset (sampleRate, 5.0e-3);
    bypassFade_.setCurrentAndTargetValue (0.0f);

    dryCompGainDbSmoothed_ = 0.0f;
    dryCompGainDbMirror_.store (0.0f, std::memory_order_relaxed);

    historyFrameSamples_ = juce::jmax (1, static_cast<int> (std::llround (sampleRate * 0.0005)));
    historySampleCounter_ = 0;
    frameMaxGrDb_ = 0.0f;
    frameMaxGrLowDb_ = 0.0f;
    frameMaxGrMidDb_ = 0.0f;
    frameMaxGrHighDb_ = 0.0f;
    frameMaxOutDb_ = -120.0f;
    frameMaxInDb_ = -120.0f;
    frameMaxClipDb_ = 0.0f;
    historyWriteIdx_.store (0, std::memory_order_release);

    msInL_ = 0.0f;
    msInR_ = 0.0f;
    msOutL_ = 0.0f;
    msOutR_ = 0.0f;
    constexpr double meterRmsTauSec = 0.3;
    meterRmsMsCoeff_ = static_cast<float> (1.0 - std::exp (-static_cast<double> (std::max (1, samplesPerBlock)) / (sampleRate * meterRmsTauSec)));
}

void MasterLimiterAudioProcessor::releaseResources()
{
    stopTimer();
}

int MasterLimiterAudioProcessor::readHistorySince (uint32_t& inOutCursor,
                                                   HistoryFrame* out,
                                                   int maxOut) const noexcept
{
    if (out == nullptr || maxOut <= 0)
        return 0;

    const uint32_t writeIdx = historyWriteIdx_.load (std::memory_order_acquire);
    uint32_t cursor = inOutCursor;

    if (cursor > writeIdx)
        cursor = writeIdx;

    const uint32_t available = writeIdx - cursor;
    if (available > static_cast<uint32_t> (kHistoryRingSize))
        cursor = writeIdx - static_cast<uint32_t> (kHistoryRingSize);

    const uint32_t clampedAvailable = writeIdx - cursor;
    const uint32_t toCopy = std::min (clampedAvailable, static_cast<uint32_t> (maxOut));

    for (uint32_t i = 0; i < toCopy; ++i)
        out[i] = historyRing_[(cursor + i) & static_cast<uint32_t> (kHistoryRingSize - 1)];

    inOutCursor = cursor + toCopy;
    return static_cast<int> (toCopy);
}

double MasterLimiterAudioProcessor::getHistorySampleRate() const noexcept
{
    return getSampleRate();
}

float MasterLimiterAudioProcessor::getCeilingDbForGraph() const noexcept
{
    return ceilingDbParam_ != nullptr ? ceilingDbParam_->get() : -1.0f;
}

float MasterLimiterAudioProcessor::getClipThresholdDbForGraph() const noexcept
{
    const bool clipperEnabled = clipperActive_ != nullptr && clipperActive_->get();
    if (! clipperEnabled || clipperDriveDb_ == nullptr)
        return 1.0e9f;

    return clipperDriveDb_->load (std::memory_order_relaxed);
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

mdsp_dsp::LinearPhaseCrossover::Spec MasterLimiterAudioProcessor::readCrossoverSpecFromParams() const
{
    mdsp_dsp::LinearPhaseCrossover::Spec spec;
    spec.sampleRate   = osSampleRate_ > 0.0 ? osSampleRate_ : 192000.0;
    spec.cutoffHz     = devXoverCutoffHz_ != nullptr ? (double) devXoverCutoffHz_->load (std::memory_order_relaxed)
                                                     : (double) kDevXoverCutoffDefault;
    spec.transitionHz = devXoverTransitionHz_ != nullptr ? (double) devXoverTransitionHz_->load (std::memory_order_relaxed)
                                                         : (double) kDevXoverTransitionDefault;
    spec.stopAttenDb  = devXoverAttenDb_ != nullptr ? (double) devXoverAttenDb_->load (std::memory_order_relaxed)
                                                    : (double) kDevXoverAttenDefault;
    spec.numChannels  = 2;
    return spec;
}

void MasterLimiterAudioProcessor::prepareCrossoverBanks (double osSampleRate)
{
    constexpr int osFactor = 4;

    mdsp_dsp::LinearPhaseCrossover::Spec worst;
    worst.sampleRate   = osSampleRate;
    worst.cutoffHz     = 40.0;
    worst.transitionHz = (double) kDevXoverTransitionMin;
    worst.stopAttenDb  = (double) kDevXoverAttenMax;
    worst.numChannels  = 2;

    auto active = readCrossoverSpecFromParams();
    active.sampleRate = osSampleRate;

    // Align the crossover's OS-rate latency to a multiple of osFactor so the host-rate
    // PDC divides exactly — otherwise the OS→host rounding leaves a sub-sample delay
    // that shows up as a high-frequency phase tilt.
    for (auto& xo : detectCrossover_)
        xo.prepareFixedLatency (worst, 2, osFactor);
    for (auto& xo : applyCrossover_)
        xo.prepareFixedLatency (worst, 2, osFactor);

    crossoverOsLatencySamples_ = detectCrossover_[0].getLatencySamples();
    jassert (crossoverOsLatencySamples_ % osFactor == 0); // exact host PDC, no HF phase residual
    crossoverOsLatencyHostSamples_ = crossoverOsLatencySamples_ / osFactor;

    xoFadeOutSamples_ = juce::jmax (1, static_cast<int> (std::llround (0.005 * osSampleRate)));
    xoFadeInSamples_  = juce::jmax (xoFadeOutSamples_, crossoverOsLatencySamples_);
    xoDuckPhase_ = 0;
    xoDuckPos_   = 0;

    for (auto& xo : detectCrossover_)
        xo.installActiveKernel (active);
    for (auto& xo : applyCrossover_)
        xo.installActiveKernel (active);

    activeCrossoverBank_.store (0, std::memory_order_release);
    crossoverPendingBank_ = 1;
    crossoverBankLock_.clear (std::memory_order_release);
    crossoverSwapReady_.store      (false, std::memory_order_release);
    heavyCrossoverDirty_.store     (false, std::memory_order_release);
    heavyLookaheadDirty_.store     (false, std::memory_order_release);
}

void MasterLimiterAudioProcessor::rebuildCrossoverKernels()
{
    const auto active = readCrossoverSpecFromParams();

    lockCrossoverBankSpin();
    const int target = 1 - activeCrossoverBank_.load (std::memory_order_relaxed);
    detectCrossover_[target].installActiveKernel (active);
    applyCrossover_[target].installActiveKernel (active);
    crossoverPendingBank_ = target;
    crossoverSwapReady_.store (true, std::memory_order_release);
    unlockCrossoverBank();
}

void MasterLimiterAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == param::dev_xover_cutoff_hz.data()
        || parameterID == param::dev_xover_transition_hz.data()
        || parameterID == param::dev_xover_atten_db.data())
    {
        juce::ignoreUnused (newValue);
        heavyCrossoverDirty_.store (true, std::memory_order_release);
        lastHeavyChangeMs_.store (juce::Time::getMillisecondCounter(), std::memory_order_release);
        triggerAsyncUpdate();
        return;
    }

    if (parameterID == param::dev_lookahead_band_ms.data()
        || parameterID == param::dev_lookahead_wide_ms.data())
    {
        juce::ignoreUnused (newValue);
        heavyLookaheadDirty_.store (true, std::memory_order_release);
        lastHeavyChangeMs_.store (juce::Time::getMillisecondCounter(), std::memory_order_release);
        triggerAsyncUpdate();
        return;
    }

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

void MasterLimiterAudioProcessor::armLearnReference() noexcept
{
    learnResetRequest_.store (true, std::memory_order_release);
    learnState_.store (static_cast<int> (LearnState::Armed), std::memory_order_release);
}

void MasterLimiterAudioProcessor::clearLearnedReference()
{
    learnedRefLufs_.store (-std::numeric_limits<float>::infinity(), std::memory_order_release);
    learnState_.store (static_cast<int> (LearnState::Idle), std::memory_order_release);
    learnResetRequest_.store (true, std::memory_order_release);
    triggerAsyncUpdate();
}

bool MasterLimiterAudioProcessor::applyPreset (int presetIndex)
{
    return master_limiter_ui::PresetManager::applyPreset (apvts, presetIndex);
}

void MasterLimiterAudioProcessor::ensureCompareSlotsInitialized()
{
    const auto current = apvts.copyState();

    if (! compareSlotA_.isValid())
        compareSlotA_ = current.createCopy();

    if (! compareSlotB_.isValid())
        compareSlotB_ = current.createCopy();
}

void MasterLimiterAudioProcessor::switchCompareSlot (int slotIndex)
{
    ensureCompareSlotsInitialized();
    slotIndex = juce::jlimit (0, 1, slotIndex);

    if (slotIndex == activeCompareSlot_)
        return;

    captureCurrentStateToActiveCompareSlot();
    replaceLiveStateFromCompareSlot (slotIndex);
    activeCompareSlot_ = slotIndex;
}

void MasterLimiterAudioProcessor::copyActiveCompareSlotToOther()
{
    ensureCompareSlotsInitialized();
    captureCurrentStateToActiveCompareSlot();

    if (activeCompareSlot_ == 0)
        compareSlotB_ = compareSlotA_.createCopy();
    else
        compareSlotA_ = compareSlotB_.createCopy();
}

void MasterLimiterAudioProcessor::captureCurrentStateToActiveCompareSlot()
{
    const auto current = apvts.copyState();

    if (activeCompareSlot_ == 0)
        compareSlotA_ = current.createCopy();
    else
        compareSlotB_ = current.createCopy();
}

void MasterLimiterAudioProcessor::replaceLiveStateFromCompareSlot (int slotIndex)
{
    const auto& slot = slotIndex == 0 ? compareSlotA_ : compareSlotB_;

    if (slot.isValid())
        apvts.replaceState (slot);
}

juce::ValueTree MasterLimiterAudioProcessor::createCompareStateTree() const
{
    juce::ValueTree compareState (kCompareStateType);
    compareState.setProperty (kCompareActiveSlotProperty, activeCompareSlot_, nullptr);

    if (compareSlotA_.isValid())
    {
        juce::ValueTree slotA (kCompareSlotAType);
        slotA.addChild (compareSlotA_.createCopy(), -1, nullptr);
        compareState.addChild (slotA, -1, nullptr);
    }

    if (compareSlotB_.isValid())
    {
        juce::ValueTree slotB (kCompareSlotBType);
        slotB.addChild (compareSlotB_.createCopy(), -1, nullptr);
        compareState.addChild (slotB, -1, nullptr);
    }

    return compareState;
}

void MasterLimiterAudioProcessor::restoreCompareStateFromTree (const juce::ValueTree& tree)
{
    const auto slotA = tree.getChildWithName (kCompareSlotAType);
    const auto slotB = tree.getChildWithName (kCompareSlotBType);
    const auto slotAState = slotA.getChildWithName (apvts.state.getType());
    const auto slotBState = slotB.getChildWithName (apvts.state.getType());

    compareSlotA_ = slotAState.isValid() ? slotAState.createCopy() : juce::ValueTree();
    compareSlotB_ = slotBState.isValid() ? slotBState.createCopy() : juce::ValueTree();
    activeCompareSlot_ = juce::jlimit (0, 1, static_cast<int> (tree.getProperty (kCompareActiveSlotProperty, 0)));
    ensureCompareSlotsInitialized();
}

void MasterLimiterAudioProcessor::handleAsyncUpdate()
{
    if (heavyGestureCommitPending_.exchange (false, std::memory_order_acq_rel))
    {
        commitHeavyControls();
        stopTimer();
    }
    else if ((heavyCrossoverDirty_.load (std::memory_order_acquire)
              || heavyLookaheadDirty_.load (std::memory_order_acquire))
             && heavyGestureActive_.load (std::memory_order_acquire) == 0
             && ! isTimerRunning())
    {
        startTimer (kHeavyPollMs);
    }

    commitLearnedRef();
}

void MasterLimiterAudioProcessor::commitHeavyControls()
{
    if (heavyCrossoverDirty_.exchange (false, std::memory_order_acq_rel))
        rebuildCrossoverKernels();

    if (heavyLookaheadDirty_.exchange (false, std::memory_order_acq_rel))
    {
        committedLookaheadBandMs_.store (devLookaheadBandMs_ != nullptr ? devLookaheadBandMs_->load (std::memory_order_relaxed) : kLookaheadMs,
                                         std::memory_order_release);
        committedLookaheadWideMs_.store (devLookaheadWideMs_ != nullptr ? devLookaheadWideMs_->load (std::memory_order_relaxed) : kLookaheadMs,
                                         std::memory_order_release);
    }
}

void MasterLimiterAudioProcessor::parameterGestureChanged (int, bool gestureIsStarting)
{
    if (gestureIsStarting)
    {
        heavyGestureActive_.fetch_add (1, std::memory_order_acq_rel);
    }
    else
    {
        const int prev = heavyGestureActive_.fetch_sub (1, std::memory_order_acq_rel);
        if (prev <= 1)
        {
            heavyGestureActive_.store (0, std::memory_order_release);
            heavyGestureCommitPending_.store (true, std::memory_order_release);
            triggerAsyncUpdate();
        }
    }
}

void MasterLimiterAudioProcessor::timerCallback()
{
    if (heavyGestureActive_.load (std::memory_order_acquire) > 0)
        return;

    const auto nowMs = juce::Time::getMillisecondCounter();
    if (nowMs - lastHeavyChangeMs_.load (std::memory_order_acquire)
        < (juce::uint32) kHeavyDebounceMs)
        return;

    commitHeavyControls();
    stopTimer();
}

void MasterLimiterAudioProcessor::commitLearnedRef()
{
    updateHostDisplay();
}

void MasterLimiterAudioProcessor::applyIoInputGain (juce::AudioBuffer<float>& buffer, int numSamples, int numChannels)
{
    ioInputGainLSmoothed_.setTargetValue (juce::Decibels::decibelsToGain (ioInputLDb_->load (std::memory_order_relaxed)));
    ioInputGainRSmoothed_.setTargetValue (juce::Decibels::decibelsToGain (ioInputRDb_->load (std::memory_order_relaxed)));

    auto* left = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        left[i] *= ioInputGainLSmoothed_.getNextValue();
        if (right != nullptr)
            right[i] *= ioInputGainRSmoothed_.getNextValue();
    }
}

void MasterLimiterAudioProcessor::applyIoOutputGain (juce::AudioBuffer<float>& buffer, int numSamples, int numChannels)
{
    ioOutputGainLSmoothed_.setTargetValue (juce::Decibels::decibelsToGain (ioOutputLDb_->load (std::memory_order_relaxed)));
    ioOutputGainRSmoothed_.setTargetValue (juce::Decibels::decibelsToGain (ioOutputRDb_->load (std::memory_order_relaxed)));

    auto* left = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        left[i] *= ioOutputGainLSmoothed_.getNextValue();
        if (right != nullptr)
            right[i] *= ioOutputGainRSmoothed_.getNextValue();
    }
}

void MasterLimiterAudioProcessor::processLearnResetRequest()
{
    if (! learnResetRequest_.exchange (false, std::memory_order_acq_rel))
        return;

    loudnessRef_.reset();
    armedSamplesElapsed_ = 0;
}

void MasterLimiterAudioProcessor::updateLearnCapture (int numSamples)
{
    if (learnState_.load (std::memory_order_acquire) != static_cast<int> (LearnState::Armed))
        return;

    armedSamplesElapsed_ += numSamples;
    if (armedSamplesElapsed_ < learnWindowSamples_)
        return;

    const float shortTerm = loudnessRef_.getSnapshot().shortTermLufs;
    if (! std::isfinite (shortTerm) || shortTerm <= -99.9f)
        return;

    learnedRefLufs_.store (shortTerm, std::memory_order_release);
    learnState_.store (static_cast<int> (LearnState::Captured), std::memory_order_release);
    triggerAsyncUpdate();
}

float MasterLimiterAudioProcessor::updateCompensationGainDb (float liveLufs)
{
    const bool trackOn = gainMatchAuto_ != nullptr
                      && gainMatchAuto_->load (std::memory_order_relaxed) > 0.5f;
    const float ref = learnedRefLufs_.load (std::memory_order_relaxed);
    const bool refOk = std::isfinite (ref);
    const bool active = trackOn && refOk;

    if (active && ! compActiveLastBlock_)
        compGainDbSmoothed_ = 0.0f;

    compActiveLastBlock_ = active;

    float target = 0.0f;
    if (active && std::isfinite (liveLufs))
        target = juce::jlimit (-12.0f, 12.0f, ref - liveLufs);

    compGainDbSmoothed_ += compGainSmoothCoef_ * (target - compGainDbSmoothed_);
    compGainDbMirror_.store (compGainDbSmoothed_, std::memory_order_relaxed);
    return compGainDbSmoothed_;
}

float MasterLimiterAudioProcessor::updateDryCompensationGainDb (float liveDryLufs)
{
    const bool trackOn = gainMatchAuto_ != nullptr
                      && gainMatchAuto_->load (std::memory_order_relaxed) > 0.5f;
    const float ref = learnedRefLufs_.load (std::memory_order_relaxed);
    const bool refOk = std::isfinite (ref);
    const bool active = trackOn && refOk;

    float target = 0.0f;
    if (active && std::isfinite (liveDryLufs))
        target = juce::jlimit (-12.0f, 12.0f, ref - liveDryLufs);

    dryCompGainDbSmoothed_ += compGainSmoothCoef_ * (target - dryCompGainDbSmoothed_);
    dryCompGainDbMirror_.store (dryCompGainDbSmoothed_, std::memory_order_relaxed);
    return dryCompGainDbSmoothed_;
}

void MasterLimiterAudioProcessor::applyCompensationGain (juce::AudioBuffer<float>& buffer,
                                                         int numSamples,
                                                         int numChannels,
                                                         float compGainDb) const
{
    const float compLin = juce::Decibels::decibelsToGain (compGainDb);
    buffer.applyGain (0, 0, numSamples, compLin);
    if (numChannels > 1)
        buffer.applyGain (1, 0, numSamples, compLin);
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
    processCore (buffer, midi, false);
}

void MasterLimiterAudioProcessor::processCore (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);

    if (inputGainDbParam_ == nullptr || ceilingDbParam_ == nullptr
        || apvts.getParameter ("release_ms") == nullptr || releaseSustainRatio_ == nullptr
        || ceilingMode_ == nullptr || stereoMode_ == nullptr || characterChoice_ == nullptr || limiterActive_ == nullptr
        || pluginBypass_ == nullptr || gainCeilingLink_ == nullptr || clipperActive_ == nullptr || clipperDriveDb_ == nullptr || clipperMode_ == nullptr
        || releaseAuto_ == nullptr || autoReleaseMode_ == nullptr
        || devLowBandReleaseScale_ == nullptr || devHighBandReleaseScale_ == nullptr
        || devSigmaAttackMs_ == nullptr || devSigmaDecayScale_ == nullptr
        || devAttackMs_ == nullptr || devAttackMode_ == nullptr || devRealAttackMs_ == nullptr
        || devReleaseEngine_ == nullptr || devLaReleaseMs_ == nullptr || devLaReleasePoles_ == nullptr
        || devLookaheadBandMs_ == nullptr || devLookaheadWideMs_ == nullptr
        || devXoverCutoffHz_ == nullptr || devXoverTransitionHz_ == nullptr || devXoverAttenDb_ == nullptr
        || ioInputLDb_ == nullptr || ioInputRDb_ == nullptr || ioOutputLDb_ == nullptr || ioOutputRDb_ == nullptr
        || stereoLinkPct_ == nullptr || msLinkPct_ == nullptr || gainMatchAuto_ == nullptr || ioInputLink_ == nullptr || ioOutputLink_ == nullptr)
        return;

    const int n = buffer.getNumSamples();
    if (n <= 0)
        return;

    const int nch = juce::jmin (2, buffer.getNumChannels());
    if (nch <= 0)
        return;

    if (dryScratch_.getNumSamples() < n || (int) grEnvBuf_.size() < n || (int) clipEnvBuf_.size() < n)
    {
        jassertfalse;
        return;
    }

    std::fill_n (grEnvBuf_.data(), n, 1.0f);
    std::fill_n (clipEnvBuf_.data(), n, 0.0f);

    const bool wantBypass = forceBypass || pluginBypass_->get();
    bypassFade_.setTargetValue (wantBypass ? 1.0f : 0.0f);

    applyIoInputGain (buffer, n, nch);

    for (int ch = 0; ch < nch; ++ch)
    {
        const auto* src = buffer.getReadPointer (ch);
        auto* dst = dryScratch_.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            dryDelay_.pushSample (ch, src[i]);
            dst[i] = dryDelay_.popSample (ch);
        }
    }

    if (nch == 1)
    {
        auto* dst = dryScratch_.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            dryDelay_.pushSample (1, 0.0f);
            dst[i] = dryDelay_.popSample (1);
        }
    }

    processLearnResetRequest();
    loudnessRef_.process (buffer);
    updateLearnCapture (n);

    {
        const float pL = dryScratch_.getMagnitude (0, 0, n);
        const float pR = (nch > 1) ? dryScratch_.getMagnitude (1, 0, n) : pL;
        inputPeakLDb_.store (juce::Decibels::gainToDecibels (pL, -120.0f), std::memory_order_relaxed);
        inputPeakRDb_.store (juce::Decibels::gainToDecibels (pR, -120.0f), std::memory_order_relaxed);

        float* tpLData[] = { dryScratch_.getWritePointer (0) };
        float* tpRData[] = { dryScratch_.getWritePointer (nch > 1 ? 1 : 0) };
        juce::AudioBuffer<float> tpLView (tpLData, 1, n);
        juce::AudioBuffer<float> tpRView (tpRData, 1, n);
        const float inTpLDb = juce::Decibels::gainToDecibels (inputTruePeakL_.measure (tpLView), -120.0f);
        const float inTpRDb = juce::Decibels::gainToDecibels (inputTruePeakR_.measure (tpRView), -120.0f);
        inputTruePeakLDb_.store (inTpLDb, std::memory_order_relaxed);
        inputTruePeakRDb_.store (inTpRDb, std::memory_order_relaxed);

        if (inTpLDb > maxInputTruePeakLDb_.load (std::memory_order_relaxed))
            maxInputTruePeakLDb_.store (inTpLDb, std::memory_order_relaxed);
        if (inTpRDb > maxInputTruePeakRDb_.load (std::memory_order_relaxed))
            maxInputTruePeakRDb_.store (inTpRDb, std::memory_order_relaxed);

        const float msL = meanSquareForChannel (dryScratch_, 0, n);
        const float msR = (nch > 1) ? meanSquareForChannel (dryScratch_, 1, n) : msL;
        msInL_ += meterRmsMsCoeff_ * (msL - msInL_);
        msInR_ += meterRmsMsCoeff_ * (msR - msInR_);
        inputRmsLDb_.store (rmsDbFromMeanSquare (msInL_), std::memory_order_relaxed);
        inputRmsRDb_.store (rmsDbFromMeanSquare (msInR_), std::memory_order_relaxed);
    }

    const int modeIdx = ceilingMode_->getIndex();

    if (modeIdx != cachedCeilingMode_)
    {
        // Final ceiling latency is fixed across SP and TP modes.
        setLatencySamples (baseLatencySamples_);
        cachedCeilingMode_ = modeIdx;
    }

    if (limiterActive_->get())
    {
        if (xoDuckPhase_ == 1 && xoDuckPos_ >= xoFadeOutSamples_)
        {
            if (tryLockCrossoverBank())
            {
                if (crossoverSwapReady_.exchange (false, std::memory_order_acq_rel))
                {
                    const int nb = crossoverPendingBank_;
                    detectCrossover_[nb].reset();
                    applyCrossover_[nb].reset();
                    activeCrossoverBank_.store (nb, std::memory_order_release);
                }
                unlockCrossoverBank();
                xoDuckPhase_ = 2;
                xoDuckPos_   = 0;
            }
        }

        const int xoBank = activeCrossoverBank_.load (std::memory_order_acquire);

        juce::dsp::AudioBlock<float> block (buffer);
        auto osBlock = limiterOversampler_.processSamplesUp (block);
        const int osN = (int) osBlock.getNumSamples();

        const bool clipperActive = clipperActive_ != nullptr && clipperActive_->get();

        auto clipBlock = clipperOversampler_.processSamplesUp (osBlock);
        const int clipN = static_cast<int> (clipBlock.getNumSamples());

        if (clipperActive)
        {
            clipperDriveSmoothed_.setTargetValue (juce::Decibels::decibelsToGain (-clipperDriveDb_->load (std::memory_order_relaxed)));
            const int clipperModeIdx = clipperMode_->getIndex();
            float maxAttenuationDb = 0.0f;
            float clipperDriveGain = clipperDriveSmoothed_.getCurrentValue();

            for (int i = 0; i < clipN; ++i)
            {
                if ((i & 1) == 0)
                    clipperDriveGain = clipperDriveSmoothed_.getNextValue();

                const int hostIdx = juce::jmin (n - 1, i * n / clipN);
                for (int ch = 0; ch < nch; ++ch)
                {
                    auto* d = clipBlock.getChannelPointer ((size_t) ch);
                    const float x = d[i] * clipperDriveGain;
                    const float ax = std::abs (x);
                    float y = x;

                    if (clipperModeIdx == 0)
                    {
                        if (ax > 1.0f)
                            y = std::copysign (1.0f, x);
                    }
                    else
                    {
                        constexpr float kSoftKnee = 0.891f;
                        if (ax > kSoftKnee)
                        {
                            const float over = (ax - kSoftKnee) / (1.0f - kSoftKnee);
                            y = std::copysign (kSoftKnee + (1.0f - kSoftKnee) * std::tanh (over), x);
                        }
                    }

                    if (ax > 1.0e-6f)
                    {
                        const float ay = std::abs (y);
                        const float attDb = juce::Decibels::gainToDecibels (ay / ax, -200.0f);
                        if (attDb < maxAttenuationDb)
                            maxAttenuationDb = attDb;
                        if (attDb < 0.0f)
                            clipEnvBuf_[(size_t) hostIdx] = std::max (clipEnvBuf_[(size_t) hostIdx], -attDb);
                    }

                    if (clipperDriveGain > 0.0f)
                        y /= clipperDriveGain;

                    d[i] = y;
                }
            }

            const float clipReadDb = -maxAttenuationDb;
            currentClipDb_.store (clipReadDb, std::memory_order_relaxed);
            if (clipReadDb > maxClipSinceResetDb_.load (std::memory_order_relaxed))
                maxClipSinceResetDb_.store (clipReadDb, std::memory_order_relaxed);
        }
        else
        {
            currentClipDb_.store (0.0f, std::memory_order_relaxed);
        }

        clipperOversampler_.processSamplesDown (osBlock);

        if (clipperOsAlignPad4x_ > 0)
        {
            juce::dsp::ProcessContextReplacing<float> clipAlignCtx (osBlock);
            clipperOsAlignDelay_.process (clipAlignCtx);
        }

        inputGainSmoothed_.setTargetValue (juce::Decibels::decibelsToGain (inputGainDbParam_->get()));
        for (int i = 0; i < osN; ++i)
        {
            const float inGainLin = inputGainSmoothed_.getNextValue();
            for (int ch = 0; ch < nch; ++ch)
            {
                auto* d = osBlock.getChannelPointer ((size_t) ch);
                d[i] *= inGainLin;
            }
        }

        const float ceilingLin = juce::Decibels::decibelsToGain (ceilingDbParam_->get());
        finalCeiling_.setMode (modeIdx == 1 ? mdsp_dsp::FinalCeilingLimiter::Mode::TruePeak
                                             : mdsp_dsp::FinalCeilingLimiter::Mode::SamplePeak);
        finalCeiling_.setCeilingLinear (ceilingLin);
        const float thresholdLin = 1.0f;
        const float bandThresholdLin = thresholdLin * juce::Decibels::decibelsToGain (-kBandHeadroomDb);
        const float releaseMs = readFloatParam (apvts, "release_ms");
        const float releaseSustainRatio = releaseSustainRatio_->get();
        const bool autoRelease = releaseAuto_->get();
        const auto autoReleaseMode = mapAutoReleaseModeIndexToMode (autoReleaseMode_->getIndex());
        const auto envelopeMode = mapCharacterIndexToMode (characterChoice_->getIndex());
        const float bandColor = bandColor_ != nullptr ? bandColor_->load (std::memory_order_relaxed) : 0.0f;
        const float lowReleaseScale = devLowBandReleaseScale_ != nullptr ? devLowBandReleaseScale_->load (std::memory_order_relaxed)
                                                                        : kLowBandAutoReleaseScale;
        const float highReleaseScale = devHighBandReleaseScale_ != nullptr ? devHighBandReleaseScale_->load (std::memory_order_relaxed)
                                                                          : kHighBandAutoReleaseScale;
        const float sigmaAttackMs = devSigmaAttackMs_ != nullptr ? devSigmaAttackMs_->load (std::memory_order_relaxed)
                                                                 : kAutoSigmaAttackMs;
        const float sigmaDecayScale = devSigmaDecayScale_ != nullptr ? devSigmaDecayScale_->load (std::memory_order_relaxed)
                                                                     : kAutoSigmaDecayScale;
        const float devAttackMs = devAttackMs_ != nullptr ? devAttackMs_->load (std::memory_order_relaxed)
                                                          : 3.0f;
        const int attackModeIdx = devAttackMode_ != nullptr ? (int) devAttackMode_->load (std::memory_order_relaxed)
                                                            : 0;
        const float realAttackMs = devRealAttackMs_ != nullptr ? devRealAttackMs_->load (std::memory_order_relaxed)
                                                               : 5.0f;
        const auto attackMode = attackModeIdx == 1 ? mdsp_dsp::LimiterEnvelope::AttackMode::Real
                                                   : mdsp_dsp::LimiterEnvelope::AttackMode::Ramp;
        const int laReleaseEngineIdx = devReleaseEngine_ != nullptr ? (int) devReleaseEngine_->load (std::memory_order_relaxed)
                                                                    : 0;
        const float laReleaseMs = devLaReleaseMs_ != nullptr ? devLaReleaseMs_->load (std::memory_order_relaxed)
                                                             : 80.0f;
        const int laReleasePoles = 2 + (devLaReleasePoles_ != nullptr ? (int) devLaReleasePoles_->load (std::memory_order_relaxed)
                                                                      : 1);
        const float devLaBandMs = committedLookaheadBandMs_.load (std::memory_order_acquire);
        const float devLaWideMs = committedLookaheadWideMs_.load (std::memory_order_acquire);
        const int osMaxLookahead = juce::jmax (1, osMaxLookaheadSamples_);
        const double activeOsRate = osSampleRate_ > 0.0 ? osSampleRate_ : (getSampleRate() * 4.0);
        const int laBandActive = juce::jlimit (1, osMaxLookahead, static_cast<int> (std::llround ((double) devLaBandMs * 1.0e-3 * activeOsRate)));
        const int laWideActive = juce::jlimit (1, osMaxLookahead, static_cast<int> (std::llround ((double) devLaWideMs * 1.0e-3 * activeOsRate)));
        const auto laEngine = laReleaseEngineIdx == 1
            ? mdsp_dsp::LimiterEnvelope::ReleaseEngine::LookaheadFollower
            : mdsp_dsp::LimiterEnvelope::ReleaseEngine::AdaptiveSigma;
        bandLinkSmoothed_.setTargetValue (mapBandColorToLink (bandColor));

        // Band audio delay = full envelope window. The detect and apply crossovers
        // BOTH add the same group delay M, so they cancel in the effective lookahead
        // (audio_delay - detect_latency) — do NOT subtract M here, or the band
        // lookahead collapses to ~0 (envelope window stays laBandActive at :1096).
        lookahead_.setDelaySamples (laBandActive);
        lookaheadWide_.setDelaySamples (laWideActive);
        envelopeLow_.setActiveLookaheadSamples (laBandActive);
        envelopeHigh_.setActiveLookaheadSamples (laBandActive);
        envelopeLowR_.setActiveLookaheadSamples (laBandActive);
        envelopeHighR_.setActiveLookaheadSamples (laBandActive);
        envelope_.setActiveLookaheadSamples (laWideActive);
        envelope_R_.setActiveLookaheadSamples (laWideActive);

        auto configureEnvelope = [&] (mdsp_dsp::LimiterEnvelope& envelope, float envThresholdLin, float autoReleaseScale)
        {
            envelope.setThresholdLinear (envThresholdLin);
            envelope.setAutoReleaseScale (autoReleaseScale);
            envelope.setAutoSigmaAttackMs (sigmaAttackMs);
            envelope.setAutoSigmaDecayScale (sigmaDecayScale);
            envelope.setAutoRelease (autoRelease);
            envelope.setAutoReleaseMode (autoReleaseMode);
            envelope.setReleaseEngine (laEngine);
            envelope.setLookaheadReleaseMs (laReleaseMs * autoReleaseScale);
            envelope.setLookaheadReleasePoles (laReleasePoles);
            envelope.setAttackMode (attackMode);
            envelope.setRealAttackMs (realAttackMs);
            envelope.setAttackOverrideMs (devAttackMs);
            if (! autoRelease)
                envelope.setReleaseMs (releaseMs);
            envelope.setReleaseSustainRatio (releaseSustainRatio);
            envelope.setMode (envelopeMode);
        };

        configureEnvelope (envelope_, thresholdLin, highReleaseScale);
        configureEnvelope (envelope_R_, thresholdLin, highReleaseScale);
        configureEnvelope (envelopeLow_, bandThresholdLin, lowReleaseScale);
        configureEnvelope (envelopeHigh_, bandThresholdLin, highReleaseScale);
        configureEnvelope (envelopeLowR_, bandThresholdLin, lowReleaseScale);
        configureEnvelope (envelopeHighR_, bandThresholdLin, highReleaseScale);

        const bool useMsMode = stereoMode_->getIndex() == 1 && nch > 1;
        const bool bandUnlink = (! useMsMode) && (nch > 1);

        auto* peakLow = peakLowBuf_.getWritePointer (0);
        auto* peakHigh = peakHighBuf_.getWritePointer (0);
        auto* peakLowL = peakLowLBuf_.getWritePointer (0);
        auto* peakLowR = peakLowRBuf_.getWritePointer (0);
        auto* peakHighL = peakHighLBuf_.getWritePointer (0);
        auto* peakHighR = peakHighRBuf_.getWritePointer (0);
        float* bandLow[2] = {
            bandLowBuf_.getWritePointer (0),
            nch > 1 ? bandLowBuf_.getWritePointer (1) : nullptr
        };
        float* bandHigh[2] = {
            bandHighBuf_.getWritePointer (0),
            nch > 1 ? bandHighBuf_.getWritePointer (1) : nullptr
        };

        for (int i = 0; i < osN; ++i)
        {
            if (! bandUnlink)
            {
                float lowMax = 0.0f;
                float highMax = 0.0f;
                for (int ch = 0; ch < nch; ++ch)
                {
                    float low = 0.0f;
                    float high = 0.0f;
                    float xDelayedDetect = 0.0f;
                    detectCrossover_[xoBank].processSample (
                        ch,
                        osBlock.getChannelPointer ((size_t) ch)[i],
                        low,
                        high,
                        xDelayedDetect);
                    juce::ignoreUnused (xDelayedDetect);
                    bandLow[ch][i] = low;
                    bandHigh[ch][i] = high;
                    lowMax = std::max (lowMax, std::abs (low));
                    highMax = std::max (highMax, std::abs (high));
                }
                peakLow[i] = lowMax;
                peakHigh[i] = highMax;
            }
            else
            {
                for (int ch = 0; ch < nch; ++ch)
                {
                    float low = 0.0f;
                    float high = 0.0f;
                    float xDelayedDetect = 0.0f;
                    detectCrossover_[xoBank].processSample (
                        ch,
                        osBlock.getChannelPointer ((size_t) ch)[i],
                        low,
                        high,
                        xDelayedDetect);
                    juce::ignoreUnused (xDelayedDetect);
                    bandLow[ch][i] = low;
                    bandHigh[ch][i] = high;
                }

                peakLowL[i] = std::abs (bandLow[0][i]);
                peakHighL[i] = std::abs (bandHigh[0][i]);
                peakLowR[i] = std::abs (bandLow[1][i]);
                peakHighR[i] = std::abs (bandHigh[1][i]);
            }
        }

        const float bandLinkPct = devBandStereoLinkPct_ != nullptr
            ? devBandStereoLinkPct_->load (std::memory_order_relaxed)
            : 100.0f;
        const float bandLink = juce::jlimit (0.0f, 1.0f, bandLinkPct * 0.01f);
        const bool bandFast = bandLink >= 0.9995f;

        auto* gainLow = gainLowBuf_.getWritePointer (0);
        auto* gainHigh = gainHighBuf_.getWritePointer (0);
        auto* gainLowL = gainLowLBuf_.getWritePointer (0);
        auto* gainLowR = gainLowRBuf_.getWritePointer (0);
        auto* gainHighL = gainHighLBuf_.getWritePointer (0);
        auto* gainHighR = gainHighRBuf_.getWritePointer (0);

        if (! bandUnlink)
        {
            envelopeLow_.process (peakLow, gainLow, osN);
            envelopeHigh_.process (peakHigh, gainHigh, osN);
        }
        else if (bandFast)
        {
            for (int i = 0; i < osN; ++i)
            {
                peakLow[i] = std::max (peakLowL[i], peakLowR[i]);
                peakHigh[i] = std::max (peakHighL[i], peakHighR[i]);
            }

            envelopeLow_.process (peakLow, gainLow, osN);
            envelopeHigh_.process (peakHigh, gainHigh, osN);
        }
        else
        {
            envelopeLow_.process (peakLowL, gainLowL, osN);
            envelopeLowR_.process (peakLowR, gainLowR, osN);
            envelopeHigh_.process (peakHighL, gainHighL, osN);
            envelopeHighR_.process (peakHighR, gainHighR, osN);

            const float oneMinusBandStereoLink = 1.0f - bandLink;
            for (int i = 0; i < osN; ++i)
            {
                const float gLowLinked = std::min (gainLowL[i], gainLowR[i]);
                gainLowL[i] = bandLink * gLowLinked + oneMinusBandStereoLink * gainLowL[i];
                gainLowR[i] = bandLink * gLowLinked + oneMinusBandStereoLink * gainLowR[i];

                const float gHighLinked = std::min (gainHighL[i], gainHighR[i]);
                gainHighL[i] = bandLink * gHighLinked + oneMinusBandStereoLink * gainHighL[i];
                gainHighR[i] = bandLink * gHighLinked + oneMinusBandStereoLink * gainHighR[i];
            }
        }

        auto* gLowOut = gLowOutBuf_.getWritePointer (0);
        auto* gHighOut = gHighOutBuf_.getWritePointer (0);
        auto* gLowOutR = gLowOutRBuf_.getWritePointer (0);
        auto* gHighOutR = gHighOutRBuf_.getWritePointer (0);

        for (int i = 0; i < osN; ++i)
        {
            float duck = 1.0f;

            if (xoDuckPhase_ == 0)
            {
                if (crossoverSwapReady_.load (std::memory_order_acquire))
                {
                    xoDuckPhase_ = 1;
                    xoDuckPos_ = 0;
                }
            }

            if (xoDuckPhase_ == 1)
            {
                if (xoDuckPos_ < xoFadeOutSamples_)
                {
                    duck = xoFadeOutGain (xoDuckPos_, xoFadeOutSamples_);
                    ++xoDuckPos_;
                }
                else
                    duck = 0.0f;
            }
            else if (xoDuckPhase_ == 2)
            {
                duck = xoFadeInGain (xoDuckPos_, xoFadeInSamples_);
                if (++xoDuckPos_ >= xoFadeInSamples_)
                {
                    xoDuckPhase_ = 0;
                    duck = 1.0f;
                }
            }

            const float bl = bandLinkSmoothed_.getNextValue();
            const float oneMinusBandLink = 1.0f - bl;

            if (! bandUnlink)
            {
                const float gLinked = std::min (gainLow[i], gainHigh[i]);
                const float linkedGain = bl * gLinked;
                gLowOut[i] = linkedGain + oneMinusBandLink * gainLow[i];
                gHighOut[i] = linkedGain + oneMinusBandLink * gainHigh[i];
                gLowOutR[i] = gLowOut[i];
                gHighOutR[i] = gHighOut[i];

                for (int ch = 0; ch < nch; ++ch)
                {
                    auto* d = osBlock.getChannelPointer ((size_t) ch);
                    const float delayed = lookahead_.pushPop (ch, d[i]);
                    float low = 0.0f;
                    float high = 0.0f;
                    float xDelayed = 0.0f;
                    applyCrossover_[xoBank].processSample (ch, delayed, low, high, xDelayed);

                    bandLimitedBuf_.getWritePointer (ch)[i] = duck * (linkedGain * xDelayed
                        + oneMinusBandLink * (low * gainLow[i] + high * gainHigh[i]));
                }
            }
            else
            {
                const float gainLowLVal = bandFast ? gainLow[i] : gainLowL[i];
                const float gainLowRVal = bandFast ? gainLow[i] : gainLowR[i];
                const float gainHighLVal = bandFast ? gainHigh[i] : gainHighL[i];
                const float gainHighRVal = bandFast ? gainHigh[i] : gainHighR[i];

                const float gLinkedL = std::min (gainLowLVal, gainHighLVal);
                const float gLinkedR = std::min (gainLowRVal, gainHighRVal);
                const float linkedGainL = bl * gLinkedL;
                const float linkedGainR = bl * gLinkedR;
                gLowOut[i] = linkedGainL + oneMinusBandLink * gainLowLVal;
                gHighOut[i] = linkedGainL + oneMinusBandLink * gainHighLVal;
                gLowOutR[i] = linkedGainR + oneMinusBandLink * gainLowRVal;
                gHighOutR[i] = linkedGainR + oneMinusBandLink * gainHighRVal;

                auto* dL = osBlock.getChannelPointer (0);
                const float delayedL = lookahead_.pushPop (0, dL[i]);
                float lowL = 0.0f;
                float highL = 0.0f;
                float xDelL = 0.0f;
                applyCrossover_[xoBank].processSample (0, delayedL, lowL, highL, xDelL);
                bandLimitedBuf_.getWritePointer (0)[i] = duck * (linkedGainL * xDelL
                    + oneMinusBandLink * (lowL * gainLowLVal + highL * gainHighLVal));

                if (nch > 1)
                {
                    auto* dR = osBlock.getChannelPointer (1);
                    const float delayedR = lookahead_.pushPop (1, dR[i]);
                    float lowR = 0.0f;
                    float highR = 0.0f;
                    float xDelR = 0.0f;
                    applyCrossover_[xoBank].processSample (1, delayedR, lowR, highR, xDelR);
                    bandLimitedBuf_.getWritePointer (1)[i] = duck * (linkedGainR * xDelR
                        + oneMinusBandLink * (lowR * gainLowRVal + highR * gainHighRVal));
                }
            }
        }

        const auto* bandLimitedL = bandLimitedBuf_.getReadPointer (0);
        const auto* bandLimitedR = nch > 1 ? bandLimitedBuf_.getReadPointer (1) : bandLimitedL;
        auto* peakWideL = peakBuf_.getWritePointer (0);
        auto* peakWideR = peakBufR_.getWritePointer (0);
        if (! useMsMode)
        {
            for (int i = 0; i < osN; ++i)
            {
                peakWideL[i] = std::abs (bandLimitedL[i]);
                peakWideR[i] = std::abs (bandLimitedR[i]);
            }
        }
        else
        {
            for (int i = 0; i < osN; ++i)
            {
                const float mid = 0.5f * (bandLimitedL[i] + bandLimitedR[i]);
                const float side = 0.5f * (bandLimitedL[i] - bandLimitedR[i]);
                peakWideL[i] = std::abs (mid);
                peakWideR[i] = std::abs (side);
            }
        }

        const float linkPct = (useMsMode ? msLinkPct_ : stereoLinkPct_)->load (std::memory_order_relaxed);
        const float link = juce::jlimit (0.0f, 1.0f, linkPct * 0.01f);
        const bool fastPath = link >= 0.9995f;

        if (fastPath)
        {
            auto* peak = peakBuf_.getWritePointer (0);
            auto* peakR = peakBufR_.getWritePointer (0);
            if (nch > 1)
            {
                for (int i = 0; i < osN; ++i)
                    peak[i] = std::max (peak[i], peakR[i]);
            }

            auto* gain = gainBuf_.getWritePointer (0);
            envelope_.process (peak, gain, osN);
        }
        else
        {
            auto* peakL = peakBuf_.getWritePointer (0);
            auto* peakR = peakBufR_.getWritePointer (0);

            auto* gainL = gainBuf_.getWritePointer (0);
            auto* gainR = gainBufR_.getWritePointer (0);
            envelope_.process (peakL, gainL, osN);
            envelope_R_.process (peakR, gainR, osN);

            const float oneMinusLink = 1.0f - link;

            for (int i = 0; i < osN; ++i)
            {
                const float gLinked = std::min (gainL[i], gainR[i]);
                gainL[i] = link * gLinked + oneMinusLink * gainL[i];
                gainR[i] = link * gLinked + oneMinusLink * gainR[i];
            }
        }

        auto* gainWideL = gainBuf_.getWritePointer (0);
        auto* gainWideR = fastPath ? gainWideL : gainBufR_.getWritePointer (0);
        float minTotalL = 1.0f;
        float minTotalR = 1.0f;
        float minLowL = 1.0f;
        float minLowR = 1.0f;
        float minHighL = 1.0f;
        float minHighR = 1.0f;

        if (! useMsMode)
        {
            for (int i = 0; i < osN; ++i)
            {
                const float gDeepBandL = std::min (gLowOut[i], gHighOut[i]);
                const float gDeepBandR = std::min (gLowOutR[i], gHighOutR[i]);
                minLowL = std::min (minLowL, gLowOut[i]);
                minLowR = std::min (minLowR, gLowOutR[i]);
                minHighL = std::min (minHighL, gHighOut[i]);
                minHighR = std::min (minHighR, gHighOutR[i]);

                const int hostIdx = juce::jmin (n - 1, i * n / osN);
                for (int ch = 0; ch < nch; ++ch)
                {
                    const auto* bandLimited = bandLimitedBuf_.getReadPointer (ch);
                    auto* d = osBlock.getChannelPointer ((size_t) ch);
                    const float delayed = lookaheadWide_.pushPop (ch, bandLimited[i]);
                    const float wideGain = (ch == 0) ? gainWideL[i] : gainWideR[i];
                    d[i] = delayed * wideGain * ceilingLin;

                    const float gDeepBand = (ch == 0) ? gDeepBandL : gDeepBandR;
                    const float totalGain = gDeepBand * wideGain;
                    if (ch == 0)
                        minTotalL = std::min (minTotalL, totalGain);
                    else
                        minTotalR = std::min (minTotalR, totalGain);

                    grEnvBuf_[(size_t) hostIdx] = std::min (grEnvBuf_[(size_t) hostIdx], totalGain);
                }
            }
        }
        else
        {
            auto* outL = osBlock.getChannelPointer (0);
            auto* outR = osBlock.getChannelPointer (1);
            for (int i = 0; i < osN; ++i)
            {
                const float gDeepBand = std::min (gLowOut[i], gHighOut[i]);
                minLowL = std::min (minLowL, gLowOut[i]);
                minLowR = minLowL;
                minHighL = std::min (minHighL, gHighOut[i]);
                minHighR = minHighL;

                const float delayedL = lookaheadWide_.pushPop (0, bandLimitedL[i]);
                const float delayedR = lookaheadWide_.pushPop (1, bandLimitedR[i]);
                const float mid = 0.5f * (delayedL + delayedR) * gainWideL[i];
                const float side = 0.5f * (delayedL - delayedR) * gainWideR[i];

                outL[i] = mid + side;
                outR[i] = mid - side;

                const float decodedPeak = std::max (std::abs (outL[i]), std::abs (outR[i]));
                float msSafetyGain = 1.0f;
                if (decodedPeak > thresholdLin)
                {
                    msSafetyGain = thresholdLin / decodedPeak;
                    outL[i] *= msSafetyGain;
                    outR[i] *= msSafetyGain;
                }

                outL[i] *= ceilingLin;
                outR[i] *= ceilingLin;

                const float totalGainL = gDeepBand * gainWideL[i] * msSafetyGain;
                const float totalGainR = gDeepBand * gainWideR[i] * msSafetyGain;
                const int hostIdx = juce::jmin (n - 1, i * n / osN);
                minTotalL = std::min (minTotalL, totalGainL);
                minTotalR = std::min (minTotalR, totalGainR);
                grEnvBuf_[(size_t) hostIdx] = std::min (grEnvBuf_[(size_t) hostIdx], std::min (totalGainL, totalGainR));
            }
        }

        if (nch == 1 || fastPath)
            minTotalR = minTotalL;

        const float grL = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minTotalL, -120.0f));
        const float grR = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minTotalR, -120.0f));
        const float grLowL = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minLowL, -120.0f));
        const float grLowR = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minLowR, -120.0f));
        const float grMidL = 0.0f;
        const float grMidR = 0.0f;
        const float grHighL = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minHighL, -120.0f));
        const float grHighR = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (minHighR, -120.0f));
        currentGrLDb_.store (grL, std::memory_order_relaxed);
        currentGrRDb_.store (grR, std::memory_order_relaxed);
        currentGrDb_.store (std::max (grL, grR), std::memory_order_relaxed);
        currentGrLowLDb_.store (grLowL, std::memory_order_relaxed);
        currentGrLowRDb_.store (grLowR, std::memory_order_relaxed);
        currentGrMidLDb_.store (grMidL, std::memory_order_relaxed);
        currentGrMidRDb_.store (grMidR, std::memory_order_relaxed);
        currentGrHighLDb_.store (grHighL, std::memory_order_relaxed);
        currentGrHighRDb_.store (grHighR, std::memory_order_relaxed);

        if (grLowL > maxGrLowLDb_.load (std::memory_order_relaxed))
            maxGrLowLDb_.store (grLowL, std::memory_order_relaxed);
        if (grLowR > maxGrLowRDb_.load (std::memory_order_relaxed))
            maxGrLowRDb_.store (grLowR, std::memory_order_relaxed);
        if (grMidL > maxGrMidLDb_.load (std::memory_order_relaxed))
            maxGrMidLDb_.store (grMidL, std::memory_order_relaxed);
        if (grMidR > maxGrMidRDb_.load (std::memory_order_relaxed))
            maxGrMidRDb_.store (grMidR, std::memory_order_relaxed);
        if (grHighL > maxGrHighLDb_.load (std::memory_order_relaxed))
            maxGrHighLDb_.store (grHighL, std::memory_order_relaxed);
        if (grHighR > maxGrHighRDb_.load (std::memory_order_relaxed))
            maxGrHighRDb_.store (grHighR, std::memory_order_relaxed);

        frameMaxGrLowDb_ = std::max (frameMaxGrLowDb_, std::max (grLowL, grLowR));
        frameMaxGrMidDb_ = std::max (frameMaxGrMidDb_, std::max (grMidL, grMidR));
        frameMaxGrHighDb_ = std::max (frameMaxGrHighDb_, std::max (grHighL, grHighR));

        const float frameMaxGr = std::max (currentGrLDb_.load (std::memory_order_relaxed),
                                           currentGrRDb_.load (std::memory_order_relaxed));
        if (frameMaxGr > maxGrSinceResetDb_.load (std::memory_order_relaxed))
            maxGrSinceResetDb_.store (frameMaxGr, std::memory_order_relaxed);

        const int padSamples = (osMaxLookahead - laBandActive) + (osMaxLookahead - laWideActive);
        lookaheadPad_.setDelaySamples (padSamples);
        for (int i = 0; i < osN; ++i)
        {
            for (int ch = 0; ch < nch; ++ch)
            {
                auto* d = osBlock.getChannelPointer ((size_t) ch);
                d[i] = lookaheadPad_.pushPop (ch, d[i]);
            }

            if (nch == 1)
                (void) lookaheadPad_.pushPop (1, 0.0f);
        }

        limiterOversampler_.processSamplesDown (block);
        finalCeiling_.process (buffer);
    }
    else
    {
        currentGrDb_.store (0.0f, std::memory_order_relaxed);
        currentGrLDb_.store (0.0f, std::memory_order_relaxed);
        currentGrRDb_.store (0.0f, std::memory_order_relaxed);
        currentGrLowLDb_.store (0.0f, std::memory_order_relaxed);
        currentGrLowRDb_.store (0.0f, std::memory_order_relaxed);
        currentGrMidLDb_.store (0.0f, std::memory_order_relaxed);
        currentGrMidRDb_.store (0.0f, std::memory_order_relaxed);
        currentGrHighLDb_.store (0.0f, std::memory_order_relaxed);
        currentGrHighRDb_.store (0.0f, std::memory_order_relaxed);
        currentClipDb_.store (0.0f, std::memory_order_relaxed);

        for (int ch = 0; ch < nch; ++ch)
            buffer.copyFrom (ch, 0, dryScratch_, ch, 0, n);
    }

    loudnessTrack_.process (buffer);
    const float liveCompDb = updateCompensationGainDb (loudnessTrack_.getSnapshot().shortTermLufs);
    const float dryCompDb = updateDryCompensationGainDb (loudnessRef_.getSnapshot().shortTermLufs);
    applyCompensationGain (buffer, n, nch, liveCompDb);
    applyCompensationGain (dryScratch_, n, nch, dryCompDb);

    for (int i = 0; i < n; ++i)
    {
        const float fade = bypassFade_.getNextValue();
        for (int ch = 0; ch < nch; ++ch)
        {
            const float live = buffer.getSample (ch, i);
            const float dry = dryScratch_.getSample (ch, i);
            buffer.setSample (ch, i, live + fade * (dry - live));
        }
    }

    applyIoOutputGain (buffer, n, nch);

    {
        const float pL = buffer.getMagnitude (0, 0, n);
        const float pR = (nch > 1) ? buffer.getMagnitude (1, 0, n) : pL;
        const float outLdb = juce::Decibels::gainToDecibels (pL, -120.0f);
        const float outRdb = juce::Decibels::gainToDecibels (pR, -120.0f);
        outputPeakLDb_.store (outLdb, std::memory_order_relaxed);
        outputPeakRDb_.store (outRdb, std::memory_order_relaxed);

        float* tpLData[] = { buffer.getWritePointer (0) };
        float* tpRData[] = { buffer.getWritePointer (nch > 1 ? 1 : 0) };
        juce::AudioBuffer<float> tpLView (tpLData, 1, n);
        juce::AudioBuffer<float> tpRView (tpRData, 1, n);
        const float outTpLDb = juce::Decibels::gainToDecibels (outputTruePeakL_.measure (tpLView), -120.0f);
        const float outTpRDb = juce::Decibels::gainToDecibels (outputTruePeakR_.measure (tpRView), -120.0f);
        outputTruePeakLDb_.store (outTpLDb, std::memory_order_relaxed);
        outputTruePeakRDb_.store (outTpRDb, std::memory_order_relaxed);
        outputTpDb_.store (std::max (outTpLDb, outTpRDb), std::memory_order_relaxed);

        if (outTpLDb > maxOutputTruePeakLDb_.load (std::memory_order_relaxed))
            maxOutputTruePeakLDb_.store (outTpLDb, std::memory_order_relaxed);
        if (outTpRDb > maxOutputTruePeakRDb_.load (std::memory_order_relaxed))
            maxOutputTruePeakRDb_.store (outTpRDb, std::memory_order_relaxed);

        const float msL = meanSquareForChannel (buffer, 0, n);
        const float msR = (nch > 1) ? meanSquareForChannel (buffer, 1, n) : msL;
        msOutL_ += meterRmsMsCoeff_ * (msL - msOutL_);
        msOutR_ += meterRmsMsCoeff_ * (msR - msOutR_);
        outputRmsLDb_.store (rmsDbFromMeanSquare (msOutL_), std::memory_order_relaxed);
        outputRmsRDb_.store (rmsDbFromMeanSquare (msOutR_), std::memory_order_relaxed);

        const float* outL = buffer.getReadPointer (0);
        const float* outR = nch > 1 ? buffer.getReadPointer (1) : outL;
        const float* dryL = dryScratch_.getReadPointer (0);
        const float* dryR = nch > 1 ? dryScratch_.getReadPointer (1) : dryL;

        for (int i = 0; i < n; ++i)
        {
            const float outDb = juce::Decibels::gainToDecibels (std::max (std::abs (outL[i]), std::abs (outR[i])), -120.0f);
            const float inDb = juce::Decibels::gainToDecibels (std::max (std::abs (dryL[i]), std::abs (dryR[i])), -120.0f);
            const float grDb = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (grEnvBuf_[(size_t) i], -120.0f));
            const float clipDb = clipEnvBuf_[(size_t) i];

            frameMaxGrDb_ = std::max (frameMaxGrDb_, grDb);
            frameMaxOutDb_ = std::max (frameMaxOutDb_, outDb);
            frameMaxInDb_ = std::max (frameMaxInDb_, inDb);
            frameMaxClipDb_ = std::max (frameMaxClipDb_, clipDb);

            if (++historySampleCounter_ >= historyFrameSamples_)
            {
                const uint32_t w = historyWriteIdx_.load (std::memory_order_relaxed);
                historyRing_[w & static_cast<uint32_t> (kHistoryRingSize - 1)] = {
                    frameMaxGrDb_, frameMaxOutDb_, frameMaxInDb_, frameMaxClipDb_,
                    frameMaxGrLowDb_, frameMaxGrMidDb_, frameMaxGrHighDb_
                };
                historyWriteIdx_.store (w + 1, std::memory_order_release);
                historySampleCounter_ = 0;
                frameMaxGrDb_ = 0.0f;
                frameMaxGrLowDb_ = 0.0f;
                frameMaxGrMidDb_ = 0.0f;
                frameMaxGrHighDb_ = 0.0f;
                frameMaxOutDb_ = -120.0f;
                frameMaxInDb_ = -120.0f;
                frameMaxClipDb_ = 0.0f;
            }
        }

        loudness_.process (buffer);
    }
}

void MasterLimiterAudioProcessor::resetOutputTruePeakHolds() noexcept
{
    outputTruePeakLDb_.store (-100.0f, std::memory_order_relaxed);
    outputTruePeakRDb_.store (-100.0f, std::memory_order_relaxed);
    maxOutputTruePeakLDb_.store (-100.0f, std::memory_order_relaxed);
    maxOutputTruePeakRDb_.store (-100.0f, std::memory_order_relaxed);
    outputTpDb_.store (-100.0f, std::memory_order_relaxed);
}

void MasterLimiterAudioProcessor::resetInputTruePeakHolds() noexcept
{
    inputTruePeakLDb_.store (-100.0f, std::memory_order_relaxed);
    inputTruePeakRDb_.store (-100.0f, std::memory_order_relaxed);
    maxInputTruePeakLDb_.store (-100.0f, std::memory_order_relaxed);
    maxInputTruePeakRDb_.store (-100.0f, std::memory_order_relaxed);
}

void MasterLimiterAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    processCore (buffer, midi, true);
}

juce::AudioProcessorParameter* MasterLimiterAudioProcessor::getBypassParameter() const
{
    return pluginBypass_;
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
    captureCurrentStateToActiveCompareSlot();
    auto state = apvts.copyState();
    juce::ValueTree wrapper (kStateWrapperType);
    const float learnedRef = learnedRefLufs_.load (std::memory_order_relaxed);
    if (std::isfinite (learnedRef))
        wrapper.setProperty (kLearnedRefLufsProperty, learnedRef, nullptr);
    // Wrap APVTS state so the hidden Learn reference stays off the automation surface.
    wrapper.addChild (state, -1, nullptr);
    wrapper.addChild (createCompareStateTree(), -1, nullptr);

    const std::unique_ptr<juce::XmlElement> xml (wrapper.createXml());
    copyXmlToBinary (*xml, destData);
}

void MasterLimiterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, static_cast<int> (sizeInBytes)))
    {
        const auto vt = juce::ValueTree::fromXml (*xml);
        if (vt.isValid())
        {
            const float noRef = -std::numeric_limits<float>::infinity();
            float loadedRef = noRef;
            bool compareStateRestored = false;

            if (vt.hasType (kStateWrapperType))
            {
                if (vt.hasProperty (kLearnedRefLufsProperty))
                    loadedRef = static_cast<float> (vt.getProperty (kLearnedRefLufsProperty));

                auto apvtsState = vt.getChildWithName (apvts.state.getType());
                if (apvtsState.isValid())
                    apvts.replaceState (apvtsState);

                auto compareState = vt.getChildWithName (kCompareStateType);
                if (compareState.isValid())
                {
                    restoreCompareStateFromTree (compareState);
                    compareStateRestored = true;
                }
            }
            else
            {
                if (vt.hasProperty (kLearnedRefLufsProperty))
                    loadedRef = static_cast<float> (vt.getProperty (kLearnedRefLufsProperty));

                apvts.replaceState (vt);
            }

            if (! compareStateRestored)
            {
                compareSlotA_ = {};
                compareSlotB_ = {};
                activeCompareSlot_ = 0;
                ensureCompareSlotsInitialized();
            }

            if (! std::isfinite (loadedRef))
                loadedRef = noRef;

            learnedRefLufs_.store (loadedRef, std::memory_order_release);
            learnState_.store (std::isfinite (loadedRef) ? static_cast<int> (LearnState::Captured)
                                                         : static_cast<int> (LearnState::Idle),
                               std::memory_order_release);
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MasterLimiterAudioProcessor();
}
