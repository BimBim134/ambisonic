#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ambi {

// Neon-on-black palette shared by the editor and LookAndFeel.
namespace palette {
inline const juce::Colour background{0xff0a0a0f};
inline const juce::Colour panel{0xff14141c};
inline const juce::Colour panelEdge{0xff262634};
inline const juce::Colour cyan{0xff00e5ff};
inline const juce::Colour magenta{0xffff2bd6};
inline const juce::Colour text{0xffe8ecf5};
inline const juce::Colour textDim{0xff8890a0};
inline const juce::Colour red{0xffff3b3b};
}  // namespace palette

// Custom LookAndFeel: embedded JetBrains Mono typeface, illuminated rotary
// knobs and the neon accent palette.
class AmbiLookAndFeel : public juce::LookAndFeel_V4 {
public:
    AmbiLookAndFeel();

    static juce::Typeface::Ptr regularTypeface();
    static juce::Typeface::Ptr boldTypeface();
    static juce::Font regularFont(float height);
    static juce::Font boldFont(float height);

    juce::Font getLabelFont(juce::Label&) override;

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbiLookAndFeel)
};

}  // namespace ambi
