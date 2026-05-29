#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "parameters/ParameterIDs.h"
#include "parameters/Parameters.h"

namespace
{
constexpr const char* kStateWrapperType = "MasterLimiterState";
constexpr const char* kLearnedRefLufsProperty = "learned_ref_lufs";

float readFloatParam (juce::AudioProcessorValueTreeState& apvts, const char* paramId)
{
    auto* p = apvts.getParameter (paramId);
    jassert (p != nullptr);
    return (float) static_cast<juce::RangedAudioParameter*> (p)->convertFrom0to1 (p->getValue());
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
    jassert (apvts.getParameter ("clipper_drive_db") != nullptr);
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
    jassert (apvts.getParameter ("gain_match_auto") != nullptr);
    jassert (apvts.getParameter ("stereo_link_pct") != nullptr);
    jassert (apvts.getParameter ("character") != nullptr);

    apvts.addParameterListener (param::input_gain_db.data(), this);
    apvts.addParameterListener (param::ceiling_db.data(), this);
    apvts.addParameterListener (param::gain_ceiling_link.data(), this);
}

MasterLimiterAudioProcessor::~MasterLimiterAudioProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener (param::input_gain_db.data(), this);
    apvts.removeParameterListener (param::ceiling_db.data(), this);
    apvts.removeParameterListener (param::gain_ceiling_link.data(), this);
}

void MasterLimiterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    constexpr int osFactor = 4;
    const int baseLookaheadSamples = static_cast<int> (std::llround (5.0e-3 * sampleRate));
    const int osLookaheadSamples = baseLookaheadSamples * osFactor;
    const int osMaxBlockSize = samplesPerBlock * osFactor;
    const double osSampleRate = sampleRate * (double) osFactor;

    limiterOversampler_.initProcessing ((size_t) samplesPerBlock);
    limiterOversampler_.reset();
    limiterOsLatencySamples_ = (int) std::llround ((double) limiterOversampler_.getLatencyInSamples());

    lookahead_.prepare (2, osLookaheadSamples);
    lookahead_.setDelaySamples (osLookaheadSamples);
    peakDetector_.prepare (2);

    mdsp_dsp::LimiterEnvelope::Spec spec;
    spec.sampleRate = osSampleRate;
    spec.lookaheadSamples = osLookaheadSamples;
    spec.maxBlockSize = osMaxBlockSize;
    envelope_.prepare (spec);
    envelope_R_.prepare (spec);

    peakBuf_.setSize (1, osMaxBlockSize, false, true, true);
    gainBuf_.setSize (1, osMaxBlockSize, false, true, true);
    peakBufR_.setSize (1, osMaxBlockSize, false, true, true);
    gainBufR_.setSize (1, osMaxBlockSize, false, true, true);

    ceilingMode_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("ceiling_mode"));
    characterChoice_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("character"));
    jassert (ceilingMode_ != nullptr);
    jassert (characterChoice_ != nullptr);

    cacheGainCeilingLinkParameters();

    limiterActive_ = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("limiter_active"));
    clipperDriveDb_ = apvts.getRawParameterValue ("clipper_drive_db");
    stereoLinkPct_ = apvts.getRawParameterValue ("stereo_link_pct");
    gainMatchAuto_ = apvts.getRawParameterValue ("gain_match_auto");
    jassert (limiterActive_ != nullptr);
    jassert (clipperDriveDb_ != nullptr);
    jassert (stereoLinkPct_ != nullptr);
    jassert (gainMatchAuto_ != nullptr);

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

    baseLatencySamples_ = baseLookaheadSamples + limiterOsLatencySamples_;
    cachedCeilingMode_  = ceilingMode_->getIndex();

    limiterOversampler_.reset();
    lookahead_.reset();
    envelope_.reset();
    envelope_R_.reset();

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

void MasterLimiterAudioProcessor::handleAsyncUpdate()
{
    commitLearnedRef();
}

void MasterLimiterAudioProcessor::commitLearnedRef()
{
    updateHostDisplay();
}

void MasterLimiterAudioProcessor::applyIoInputGain (juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) const
{
    const float ioInputLGain = juce::Decibels::decibelsToGain (ioInputLDb_->load (std::memory_order_relaxed));
    const float ioInputRGain = juce::Decibels::decibelsToGain (ioInputRDb_->load (std::memory_order_relaxed));
    buffer.applyGain (0, 0, numSamples, ioInputLGain);
    if (numChannels > 1)
        buffer.applyGain (1, 0, numSamples, ioInputRGain);
}

void MasterLimiterAudioProcessor::applyIoOutputGain (juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) const
{
    const float ioOutputLGain = juce::Decibels::decibelsToGain (ioOutputLDb_->load (std::memory_order_relaxed));
    const float ioOutputRGain = juce::Decibels::decibelsToGain (ioOutputRDb_->load (std::memory_order_relaxed));
    buffer.applyGain (0, 0, numSamples, ioOutputLGain);
    if (numChannels > 1)
        buffer.applyGain (1, 0, numSamples, ioOutputRGain);
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
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);

    if (inputGainDbParam_ == nullptr || ceilingDbParam_ == nullptr
        || apvts.getParameter ("release_ms") == nullptr || releaseSustainRatio_ == nullptr
        || ceilingMode_ == nullptr || characterChoice_ == nullptr || limiterActive_ == nullptr
        || gainCeilingLink_ == nullptr || clipperDriveDb_ == nullptr || ioInputLDb_ == nullptr
        || ioInputRDb_ == nullptr || ioOutputLDb_ == nullptr || ioOutputRDb_ == nullptr
        || stereoLinkPct_ == nullptr || gainMatchAuto_ == nullptr || ioInputLink_ == nullptr || ioOutputLink_ == nullptr)
        return;

    const int n = buffer.getNumSamples();
    if (n <= 0)
        return;

    const int nch = juce::jmin (2, buffer.getNumChannels());
    if (nch <= 0)
        return;

    applyIoInputGain (buffer, n, nch);

    processLearnResetRequest();
    loudnessRef_.process (buffer);
    updateLearnCapture (n);

    {
        const float pL = buffer.getMagnitude (0, 0, n);
        const float pR = (nch > 1) ? buffer.getMagnitude (1, 0, n) : pL;
        inputPeakLDb_.store (juce::Decibels::gainToDecibels (pL, -100.0f), std::memory_order_relaxed);
        inputPeakRDb_.store (juce::Decibels::gainToDecibels (pR, -100.0f), std::memory_order_relaxed);
    }

    const int modeIdx = ceilingMode_->getIndex();

    if (modeIdx != cachedCeilingMode_)
    {
        // Latency is the same in SP and TP after 9.6 OS consolidation.
        setLatencySamples (baseLatencySamples_);
        cachedCeilingMode_ = modeIdx;
    }

    if (limiterActive_->get())
    {
        const float inGainLin = juce::Decibels::decibelsToGain (inputGainDbParam_->get());
        for (int ch = 0; ch < nch; ++ch)
            buffer.applyGain (ch, 0, n, inGainLin);

        juce::dsp::AudioBlock<float> block (buffer);
        auto osBlock = limiterOversampler_.processSamplesUp (block);
        const int osN = (int) osBlock.getNumSamples();

        const float clipperDriveDb = clipperDriveDb_->load (std::memory_order_relaxed);
        if (clipperDriveDb > 0.0f)
        {
            const float driveGain = juce::Decibels::decibelsToGain (clipperDriveDb);
            float maxPreClipAbs = 0.0f;
            for (int ch = 0; ch < nch; ++ch)
            {
                auto* d = osBlock.getChannelPointer ((size_t) ch);
                for (int i = 0; i < osN; ++i)
                {
                    const float scaled = d[i] * driveGain;
                    const float a = std::abs (scaled);
                    if (a > maxPreClipAbs)
                        maxPreClipAbs = a;
                    d[i] = juce::jlimit (-1.0f, 1.0f, scaled);
                }
            }
            currentClipDb_.store (maxPreClipAbs > 1.0f
                                      ? juce::Decibels::gainToDecibels (maxPreClipAbs)
                                      : 0.0f,
                                  std::memory_order_relaxed);
        }
        else
        {
            currentClipDb_.store (0.0f, std::memory_order_relaxed);
        }
        const float frameClipDb = currentClipDb_.load (std::memory_order_relaxed);
        if (frameClipDb > maxClipSinceResetDb_.load (std::memory_order_relaxed))
            maxClipSinceResetDb_.store (frameClipDb, std::memory_order_relaxed);

        const float* const ch0 = osBlock.getChannelPointer ((size_t) 0);
        const float* const ch1 = nch > 1 ? osBlock.getChannelPointer ((size_t) 1) : ch0;
        const float ceilingLin = juce::Decibels::decibelsToGain (ceilingDbParam_->get());
        const float thresholdLin = (modeIdx == 1) ? (ceilingLin * 0.965f) : ceilingLin;
        envelope_.setThresholdLinear (thresholdLin);
        envelope_.setReleaseMs (readFloatParam (apvts, "release_ms"));
        envelope_.setReleaseSustainRatio (releaseSustainRatio_->get());
        envelope_.setMode (mapCharacterIndexToMode (characterChoice_->getIndex()));

        const float linkPct = stereoLinkPct_->load (std::memory_order_relaxed);
        const float link = juce::jlimit (0.0f, 1.0f, linkPct * 0.01f);
        const bool fastPath = link >= 0.9995f;

        if (fastPath)
        {
            auto* peak = peakBuf_.getWritePointer (0);
            const float* const channelPtrs[2] = { ch0, ch1 };

            for (int i = 0; i < osN; ++i)
                peak[i] = peakDetector_.process (channelPtrs, nch, i);

            auto* gain = gainBuf_.getWritePointer (0);
            envelope_.process (peak, gain, osN);

            for (int ch = 0; ch < nch; ++ch)
            {
                auto* d = osBlock.getChannelPointer ((size_t) ch);
                for (int i = 0; i < osN; ++i)
                {
                    const float delayed = lookahead_.pushPop (ch, d[i]);
                    d[i] = delayed * gain[i];
                }
            }

            const float gr = envelope_.getLastBlockMaxGrDb();
            currentGrDb_.store (gr, std::memory_order_relaxed);
            currentGrLDb_.store (gr, std::memory_order_relaxed);
            currentGrRDb_.store (gr, std::memory_order_relaxed);
        }
        else
        {
            envelope_R_.setThresholdLinear (thresholdLin);
            envelope_R_.setReleaseMs (readFloatParam (apvts, "release_ms"));
            envelope_R_.setReleaseSustainRatio (releaseSustainRatio_->get());
            envelope_R_.setMode (mapCharacterIndexToMode (characterChoice_->getIndex()));

            auto* peakL = peakBuf_.getWritePointer (0);
            auto* peakR = peakBufR_.getWritePointer (0);
            for (int i = 0; i < osN; ++i)
            {
                peakL[i] = std::abs (ch0[i]);
                peakR[i] = std::abs (ch1[i]);
            }

            auto* gainL = gainBuf_.getWritePointer (0);
            auto* gainR = gainBufR_.getWritePointer (0);
            envelope_.process (peakL, gainL, osN);
            envelope_R_.process (peakR, gainR, osN);

            auto* dL = osBlock.getChannelPointer ((size_t) 0);
            auto* dR = nch > 1 ? osBlock.getChannelPointer ((size_t) 1) : dL;
            const float oneMinusLink = 1.0f - link;

            for (int i = 0; i < osN; ++i)
            {
                const float gLinked = std::min (gainL[i], gainR[i]);
                const float gOutL = link * gLinked + oneMinusLink * gainL[i];
                const float gOutR = link * gLinked + oneMinusLink * gainR[i];

                const float delayedL = lookahead_.pushPop (0, dL[i]);
                dL[i] = delayedL * gOutL;

                if (nch > 1)
                {
                    const float delayedR = lookahead_.pushPop (1, dR[i]);
                    dR[i] = delayedR * gOutR;
                }
            }

            const float grL = envelope_.getLastBlockMaxGrDb();
            const float grR = envelope_R_.getLastBlockMaxGrDb();
            currentGrLDb_.store (grL, std::memory_order_relaxed);
            currentGrRDb_.store (grR, std::memory_order_relaxed);
            currentGrDb_.store (std::max (grL, grR), std::memory_order_relaxed);
        }

        const float frameMaxGr = std::max (currentGrLDb_.load (std::memory_order_relaxed),
                                           currentGrRDb_.load (std::memory_order_relaxed));
        if (frameMaxGr > maxGrSinceResetDb_.load (std::memory_order_relaxed))
            maxGrSinceResetDb_.store (frameMaxGr, std::memory_order_relaxed);

        limiterOversampler_.processSamplesDown (block);
    }
    else
    {
        currentGrDb_.store (0.0f, std::memory_order_relaxed);
        currentGrLDb_.store (0.0f, std::memory_order_relaxed);
        currentGrRDb_.store (0.0f, std::memory_order_relaxed);
        currentClipDb_.store (0.0f, std::memory_order_relaxed);
    }

    loudnessTrack_.process (buffer);
    applyCompensationGain (buffer, n, nch, updateCompensationGainDb (loudnessTrack_.getSnapshot().shortTermLufs));

    applyIoOutputGain (buffer, n, nch);

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

void MasterLimiterAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);

    if (gainMatchAuto_ == nullptr || ioInputLDb_ == nullptr || ioInputRDb_ == nullptr
        || ioOutputLDb_ == nullptr || ioOutputRDb_ == nullptr)
        return;

    const bool trackOn = gainMatchAuto_->load (std::memory_order_relaxed) > 0.5f;
    const float ref = learnedRefLufs_.load (std::memory_order_relaxed);
    if (! trackOn || ! std::isfinite (ref))
        return;

    const int n = buffer.getNumSamples();
    if (n <= 0)
        return;

    const int nch = juce::jmin (2, buffer.getNumChannels());
    if (nch <= 0)
        return;

    applyIoInputGain (buffer, n, nch);
    loudnessRef_.process (buffer);
    applyCompensationGain (buffer, n, nch, updateCompensationGainDb (loudnessRef_.getSnapshot().shortTermLufs));
    applyIoOutputGain (buffer, n, nch);
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
    auto state = apvts.copyState();
    juce::ValueTree wrapper (kStateWrapperType);
    const float learnedRef = learnedRefLufs_.load (std::memory_order_relaxed);
    if (std::isfinite (learnedRef))
        wrapper.setProperty (kLearnedRefLufsProperty, learnedRef, nullptr);
    // Wrap APVTS state so the hidden Learn reference stays off the automation surface.
    wrapper.addChild (state, -1, nullptr);

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

            if (vt.hasType (kStateWrapperType))
            {
                if (vt.hasProperty (kLearnedRefLufsProperty))
                    loadedRef = static_cast<float> (vt.getProperty (kLearnedRefLufsProperty));

                auto apvtsState = vt.getChildWithName (apvts.state.getType());
                if (apvtsState.isValid())
                    apvts.replaceState (apvtsState);
            }
            else
            {
                if (vt.hasProperty (kLearnedRefLufsProperty))
                    loadedRef = static_cast<float> (vt.getProperty (kLearnedRefLufsProperty));

                apvts.replaceState (vt);
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
