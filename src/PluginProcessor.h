#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

#include "encoder.hpp"

// Ambisonic order is fixed at CMake configure time (see AMBI_ORDER in
// CMakeLists.txt). The default matches sde::kDefaultOrder.
#ifndef AMBI_ORDER
#define AMBI_ORDER 3
#endif

// Stereo -> SN3D ambisonic encoder wrapped as a JUCE plugin. The DSP core is
// reused verbatim from the command-line tool; this class only adapts JUCE's
// planar float buffers to the shared encoder matrix.
class AmbiEncoderAudioProcessor : public juce::AudioProcessor {
public:
    static constexpr int kOrder = AMBI_ORDER;
    static constexpr int kNumChannels = (kOrder + 1) * (kOrder + 1);
    static constexpr int kNumInputChannels = sde::ProbeEncoder::kNumInputChannels;

    AmbiEncoderAudioProcessor();
    ~AmbiEncoderAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState& apvts() { return apvts_; }

    // Peak level of the last processed block (linear, already gain-scaled).
    float getOutputPeak() const { return outputPeak_.load(std::memory_order_relaxed); }

    // Latched clip flag; call clearClip() (typically from the message thread).
    bool getClipFlag() const { return clipFlag_.load(std::memory_order_relaxed); }
    void clearClip() { clipFlag_.store(false, std::memory_order_relaxed); }

    // Number of Mid/Side sample pairs retained for the vectorscope.
    static constexpr int kScopePoints = 1024;

    // Copy the most recent `count` (1..kScopePoints) input Mid/Side pairs in
    // chronological order. mid = (L+R)/2, side = (L-R)/2.
    void readScope(float* midOut, float* sideOut, int count) const;

    static constexpr const char* kGainParamId = "gain";
    static constexpr const char* kSpreadParamId = "spread";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Rebuild matrix_ from `spread` without allocating. Audio-thread only.
    void rebuildMatrix(double spread);

    juce::AudioProcessorValueTreeState apvts_;
    std::atomic<float>* gainRaw_ = nullptr;
    std::atomic<float>* spreadRaw_ = nullptr;

    // Row-major 2 x kNumChannels encoder matrix: [0, N) mid, [N, 2N) side.
    std::array<double, static_cast<std::size_t>(2 * kNumChannels)> matrix_{};
    double builtSpread_ = -1.0;  // spread currently baked into matrix_

    juce::SmoothedValue<float> gainSmoothed_;
    double currentSampleRate_ = 44100.0;

    std::atomic<float> outputPeak_{0.0f};
    std::atomic<bool> clipFlag_{false};

    // Input Mid/Side ring buffer for the vectorscope. Writes use a try-lock so
    // the audio thread never blocks; a dropped frame is harmless visually.
    std::array<float, kScopePoints> scopeMid_{};
    std::array<float, kScopePoints> scopeSide_{};
    int scopeWrite_ = 0;
    mutable juce::SpinLock scopeLock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbiEncoderAudioProcessor)
};
