#include "LookAndFeel.h"

#include <cmath>

#include "BinaryData.h"

namespace ambi
{

    AmbiLookAndFeel::AmbiLookAndFeel()
    {
        // Use the embedded monospaced typeface as the default for all text.
        setDefaultSansSerifTypeface(regularTypeface());

        setColour(juce::ResizableWindow::backgroundColourId, palette::background);
        setColour(juce::Label::textColourId, palette::text);
        setColour(juce::Slider::textBoxTextColourId, palette::cyan);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::rotarySliderFillColourId, palette::cyan);
        setColour(juce::Slider::rotarySliderOutlineColourId, palette::panelEdge);
    }

    juce::Typeface::Ptr AmbiLookAndFeel::regularTypeface()
    {
        static juce::Typeface::Ptr typeface =
            juce::Typeface::createSystemTypefaceFor(BinaryData::JetBrainsMonoRegular_ttf,
                                                    BinaryData::JetBrainsMonoRegular_ttfSize);
        return typeface;
    }

    juce::Typeface::Ptr AmbiLookAndFeel::boldTypeface()
    {
        static juce::Typeface::Ptr typeface =
            juce::Typeface::createSystemTypefaceFor(BinaryData::JetBrainsMonoBold_ttf,
                                                    BinaryData::JetBrainsMonoBold_ttfSize);
        return typeface;
    }

    juce::Font AmbiLookAndFeel::regularFont(float height)
    {
        return juce::Font{juce::FontOptions(regularTypeface()).withHeight(height)};
    }

    juce::Font AmbiLookAndFeel::boldFont(float height)
    {
        return juce::Font{juce::FontOptions(boldTypeface()).withHeight(height)};
    }

    juce::Font AmbiLookAndFeel::getLabelFont(juce::Label &label)
    {
        return regularFont(juce::jmax(10.0f, label.getFont().getHeight()));
    }

    void AmbiLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width,
                                           int height, float sliderPos,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider &slider)
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(6.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float lineW = juce::jmax(3.0f, radius * 0.11f);
        const float arcR = radius - lineW * 0.5f;
        const float angle =
            rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);

        // Track.
        {
            juce::Path track;
            track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle,
                                rotaryEndAngle, true);
            g.setColour(palette::panelEdge);
            g.strokePath(track, juce::PathStrokeType(lineW, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // Value arc with a soft glow.
        if (sliderPos > 0.0001f)
        {
            juce::Path value;
            value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle,
                                angle, true);

            g.setColour(accent.withAlpha(0.16f));
            g.strokePath(value, juce::PathStrokeType(lineW * 3.0f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            g.setColour(accent.withAlpha(0.32f));
            g.strokePath(value, juce::PathStrokeType(lineW * 1.8f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            g.setColour(accent);
            g.strokePath(value, juce::PathStrokeType(lineW, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // Knob body.
        const float knobR = radius - lineW * 2.2f;
        auto knobArea =
            juce::Rectangle<float>(knobR * 2.0f, knobR * 2.0f).withCentre(centre);
        {
            juce::ColourGradient grad(palette::panel.brighter(0.22f), centre.x,
                                      centre.y - knobR,
                                      palette::background.darker(0.35f), centre.x,
                                      centre.y + knobR, false);
            g.setGradientFill(grad);
            g.fillEllipse(knobArea);
            g.setColour(palette::panelEdge);
            g.drawEllipse(knobArea, 1.0f);
        }

        // Pointer.
        {
            const juce::Point<float> inner(
                centre.x + std::sin(angle) * knobR * 0.15f,
                centre.y - std::cos(angle) * knobR * 0.15f);
            const juce::Point<float> outer(
                centre.x + std::sin(angle) * knobR * 0.82f,
                centre.y - std::cos(angle) * knobR * 0.82f);
            g.setColour(accent.withAlpha(0.25f));
            g.drawLine({inner, outer}, lineW * 1.6f);
            g.setColour(accent);
            g.drawLine({inner, outer}, juce::jmax(2.0f, knobR * 0.10f));
        }
    }

} // namespace ambi
