#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "spherical_harmonics.hpp"

#include <cmath>

namespace {
constexpr const char* kStateType = "PARAMETERS";

// Gain range in dB. Default 0 dB (unity), matching the "Unity + peak meter"
// plugin convention rather than the CLI's clipping-averse 0.1 default.
constexpr float kGainMinDb = -60.0f;
constexpr float kGainMaxDb = 12.0f;

// Azimuth spread. Default matches sde::kDefaultSpread.
constexpr float kSpreadMin = 0.0f;
constexpr float kSpreadMax = 3.0f;
}  // namespace

AmbiEncoderAudioProcessor::AmbiEncoderAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input",
                                          juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output",
                                           juce::AudioChannelSet::ambisonic(kOrder),
                                           true)),
      apvts_(*this, nullptr, kStateType, createLayout()) {
    gainRaw_ = apvts_.getRawParameterValue(kGainParamId);
    spreadRaw_ = apvts_.getRawParameterValue(kSpreadParamId);

    // Prime the matrix from the default spread.
    rebuildMatrix(static_cast<double>(sde::kDefaultSpread));
    builtSpread_ = static_cast<double>(sde::kDefaultSpread);

    // Report zero latency; the encoder is memoryless.
    setLatencySamples(0);
}

juce::AudioProcessorValueTreeState::ParameterLayout
AmbiEncoderAudioProcessor::createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kGainParamId, 1}, "Gain",
        juce::NormalisableRange<float>(kGainMinDb, kGainMaxDb, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kSpreadParamId, 1}, "Spread",
        juce::NormalisableRange<float>(kSpreadMin, kSpreadMax, 0.001f),
        static_cast<float>(sde::kDefaultSpread)));

    return layout;
}

void AmbiEncoderAudioProcessor::rebuildMatrix(double spread) {
    sde::build_encoder_matrix_into(kOrder, spread, matrix_.data());
}

void AmbiEncoderAudioProcessor::prepareToPlay(double sampleRate,
                                              int /*samplesPerBlock*/) {
    currentSampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    gainSmoothed_.reset(currentSampleRate_, 0.02);
    gainSmoothed_.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(gainRaw_->load()));
    outputPeak_.store(0.0f, std::memory_order_relaxed);
    clipFlag_.store(false, std::memory_order_relaxed);

    {
        const juce::SpinLock::ScopedLockType lock(scopeLock_);
        scopeMid_.fill(0.0f);
        scopeSide_.fill(0.0f);
        scopeWrite_ = 0;
    }
}

bool AmbiEncoderAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) {
        return false;
    }

    const auto out = layouts.getMainOutputChannelSet();
    // Preferred: a proper SN3D/ACN ambisonic layout.
    if (out == juce::AudioChannelSet::ambisonic(kOrder)) {
        return true;
    }
    // Fallback for hosts that don't negotiate ambisonic speaker arrangements.
    return out == juce::AudioChannelSet::discreteChannels(kNumChannels);
}

void AmbiEncoderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    auto input = getBusBuffer(buffer, true, 0);
    auto output = getBusBuffer(buffer, false, 0);

    const int numSamples = output.getNumSamples();
    const int numOut = juce::jmin(output.getNumChannels(), kNumChannels);

    // Silence any output channels we don't drive (shouldn't normally happen).
    for (int ch = numOut; ch < output.getNumChannels(); ++ch) {
        output.clear(ch, 0, numSamples);
    }

    if (input.getNumChannels() < kNumInputChannels || numOut <= 0 ||
        numSamples <= 0) {
        output.clear();
        outputPeak_.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Rebuild the encoder matrix only when the spread parameter changes. This
    // is non-allocating, so it is safe here on the audio thread.
    const double spread = static_cast<double>(spreadRaw_->load());
    if (spread != builtSpread_) {
        rebuildMatrix(spread);
        builtSpread_ = spread;
    }

    gainSmoothed_.setTargetValue(
        juce::Decibels::decibelsToGain(gainRaw_->load()));

    const float* inPtrs[kNumInputChannels] = {input.getReadPointer(0),
                                              input.getReadPointer(1)};

    float* outPtrs[sde::kMaxAmbisonicChannels];
    for (int c = 0; c < numOut; ++c) {
        outPtrs[c] = output.getWritePointer(c);
    }

    sde::encode_planar(inPtrs, outPtrs, static_cast<std::size_t>(numSamples),
                       matrix_.data(), numOut);

    // Apply the smoothed gain and meter the result.
    float peak = 0.0f;
    for (int f = 0; f < numSamples; ++f) {
        const float g = gainSmoothed_.getNextValue();
        for (int c = 0; c < numOut; ++c) {
            float* channel = outPtrs[c];
            const float v = channel[f] * g;
            channel[f] = v;
            peak = juce::jmax(peak, std::abs(v));
        }
    }

    outputPeak_.store(peak, std::memory_order_relaxed);
    if (peak > 1.0f) {
        clipFlag_.store(true, std::memory_order_relaxed);
    }

    // Capture input Mid/Side for the vectorscope. Best-effort: if the editor is
    // mid-copy we simply skip this block rather than block the audio thread.
    {
        const juce::SpinLock::ScopedTryLockType lock(scopeLock_);
        if (lock.isLocked()) {
            const float* l = inPtrs[0];
            const float* r = inPtrs[1];
            int idx = scopeWrite_;
            for (int f = 0; f < numSamples; ++f) {
                scopeMid_[idx] = 0.5f * (l[f] + r[f]);
                scopeSide_[idx] = 0.5f * (l[f] - r[f]);
                idx = (idx + 1 < kScopePoints) ? idx + 1 : 0;
            }
            scopeWrite_ = idx;
        }
    }
}

void AmbiEncoderAudioProcessor::readScope(float* midOut, float* sideOut,
                                          int count) const {
    const juce::SpinLock::ScopedLockType lock(scopeLock_);
    count = juce::jlimit(1, kScopePoints, count);
    int start = (scopeWrite_ - count) % kScopePoints;
    if (start < 0) {
        start += kScopePoints;
    }
    for (int i = 0; i < count; ++i) {
        const int idx = (start + i) % kScopePoints;
        midOut[i] = scopeMid_[idx];
        sideOut[i] = scopeSide_[idx];
    }
}

juce::AudioProcessorEditor* AmbiEncoderAudioProcessor::createEditor() {
    return new AmbiEncoderAudioProcessorEditor(*this);
}

const juce::String AmbiEncoderAudioProcessor::getName() const {
    return JucePlugin_Name;
}

void AmbiEncoderAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    if (state.isValid()) {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void AmbiEncoderAudioProcessor::setStateInformation(const void* data,
                                                    int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(
        getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr) {
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new AmbiEncoderAudioProcessor();
}
