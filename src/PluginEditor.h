#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "LookAndFeel.h"

// Neon editor: a Mid/Side XY vectorscope, two illuminated rotary knobs (Gain,
// Spread), a peak meter and a click-to-reset clip LED. Resizable.
class AmbiEncoderAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer {
public:
    explicit AmbiEncoderAudioProcessorEditor(AmbiEncoderAudioProcessor&);
    ~AmbiEncoderAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    // Mid/Side XY vectorscope with phosphor persistence.
    class ScopeComponent : public juce::Component, private juce::Timer {
    public:
        explicit ScopeComponent(AmbiEncoderAudioProcessor&);
        ~ScopeComponent() override;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void drawMidSide(juce::Graphics&, juce::Rectangle<float>);

        AmbiEncoderAudioProcessor& proc_;

        juce::Image buffer_;
        std::array<float, AmbiEncoderAudioProcessor::kScopePoints> midBuf_{};
        std::array<float, AmbiEncoderAudioProcessor::kScopePoints> sideBuf_{};
    };

    void timerCallback() override;
    void drawMeter(juce::Graphics&);

    AmbiEncoderAudioProcessor& processorRef_;
    ambi::AmbiLookAndFeel lookAndFeel_;

    juce::Slider gainSlider_;
    juce::Slider spreadSlider_;
    juce::Label gainLabel_;
    juce::Label spreadLabel_;
    juce::Label titleLabel_;
    juce::Label infoLabel_;

    ScopeComponent scope_;

    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment_;
    juce::AudioProcessorValueTreeState::SliderAttachment spreadAttachment_;

    juce::Rectangle<int> meterBounds_;
    juce::Rectangle<int> clipBounds_;
    float displayPeak_ = 0.0f;
    bool clipLatched_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbiEncoderAudioProcessorEditor)
};
