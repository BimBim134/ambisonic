#include "PluginEditor.h"

namespace
{
    constexpr int kMargin = 14;
    constexpr int kHeaderHeight = 54;
    constexpr int kFooterHeight = 118;
    constexpr int kMeterWidth = 24;

    const juce::Colour kPanel = ambi::palette::panel;
} // namespace

//==============================================================================
AmbiEncoderAudioProcessorEditor::ScopeComponent::ScopeComponent(
    AmbiEncoderAudioProcessor &p)
    : proc_(p)
{
    startTimerHz(60);
}

AmbiEncoderAudioProcessorEditor::ScopeComponent::~ScopeComponent()
{
    stopTimer();
}

void AmbiEncoderAudioProcessorEditor::ScopeComponent::resized()
{
    if (!buffer_.isValid() || buffer_.getWidth() != getWidth() ||
        buffer_.getHeight() != getHeight())
    {
        buffer_ = juce::Image(juce::Image::ARGB, juce::jmax(1, getWidth()),
                              juce::jmax(1, getHeight()), true);
        if (buffer_.isValid())
        {
            juce::Graphics(buffer_).fillAll(ambi::palette::panel);
        }
    }
}

void AmbiEncoderAudioProcessorEditor::ScopeComponent::timerCallback()
{
    proc_.readScope(midBuf_.data(), sideBuf_.data(),
                    static_cast<int>(midBuf_.size()));

    if (!buffer_.isValid())
        return;

    juce::Graphics g(buffer_);

    // Phosphor fade towards the panel colour (also serves as the background).
    g.setColour(kPanel.withAlpha(0.18f));
    g.fillAll();

    drawMidSide(g, getLocalBounds().toFloat().reduced(2.0f));
    repaint();
}

void AmbiEncoderAudioProcessorEditor::ScopeComponent::drawMidSide(
    juce::Graphics &g, juce::Rectangle<float> rect)
{
    g.saveState();
    g.reduceClipRegion(rect.toNearestInt());

    const juce::Point<float> c = rect.getCentre();
    const float R = juce::jmin(rect.getWidth(), rect.getHeight()) * 0.5f - 10.0f;
    if (R <= 4.0f)
    {
        g.restoreState();
        return;
    }

    // Grid: unit circle, axes, and the L/R diagonals.
    g.setColour(ambi::palette::panelEdge.withAlpha(0.65f));
    g.drawEllipse(c.x - R, c.y - R, R * 2.0f, R * 2.0f, 1.0f);
    g.drawLine(c.x - R, c.y, c.x + R, c.y, 1.0f);
    g.drawLine(c.x, c.y - R, c.x, c.y + R, 1.0f);
    g.drawLine(c.x - R * 0.7071f, c.y - R * 0.7071f, c.x + R * 0.7071f,
               c.y + R * 0.7071f, 1.0f);
    g.drawLine(c.x - R * 0.7071f, c.y + R * 0.7071f, c.x + R * 0.7071f,
               c.y - R * 0.7071f, 1.0f);

    // Traces in the classic Lissajous orientation (mono = 45 deg up-right).
    // Screen x ~ R channel, screen up ~ L channel.
    g.setColour(ambi::palette::cyan.withAlpha(0.32f));
    const auto n = static_cast<int>(midBuf_.size());
    for (int i = 0; i < n; ++i)
    {
        const float mid = midBuf_[static_cast<size_t>(i)];
        const float side = sideBuf_[static_cast<size_t>(i)];
        const float px = c.x - (mid - side) * R;
        const float py = c.y - (mid + side) * R;
        if (px < rect.getX() || px > rect.getRight() || py < rect.getY() ||
            py > rect.getBottom())
            continue;
        g.fillEllipse(px - 1.0f, py - 1.0f, 2.0f, 2.0f);
    }

    g.setColour(ambi::palette::textDim);
    g.setFont(ambi::AmbiLookAndFeel::regularFont(9.0f));
    g.drawText("MID / SIDE", rect.reduced(6.0f).removeFromTop(11.0f),
               juce::Justification::centredLeft, false);

    g.restoreState();
}

void AmbiEncoderAudioProcessorEditor::ScopeComponent::paint(
    juce::Graphics &g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::Path clip;
    clip.addRoundedRectangle(bounds, 8.0f);
    g.reduceClipRegion(clip, {});
    if (buffer_.isValid())
    {
        g.drawImageAt(buffer_, 0, 0);
    }
    g.setColour(ambi::palette::panelEdge);
    g.strokePath(clip, juce::PathStrokeType(1.0f));
}

//==============================================================================
AmbiEncoderAudioProcessorEditor::AmbiEncoderAudioProcessorEditor(
    AmbiEncoderAudioProcessor &p)
    : juce::AudioProcessorEditor(&p),
      processorRef_(p),
      scope_(p),
      gainAttachment_(p.apvts(), AmbiEncoderAudioProcessor::kGainParamId,
                      gainSlider_),
      spreadAttachment_(p.apvts(), AmbiEncoderAudioProcessor::kSpreadParamId,
                        spreadSlider_)
{
    setLookAndFeel(&lookAndFeel_);

    titleLabel_.setText("THE POTATE", juce::dontSendNotification);
    titleLabel_.setFont(ambi::AmbiLookAndFeel::boldFont(22.0f));
    titleLabel_.setColour(juce::Label::textColourId, ambi::palette::cyan);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    const juce::String bullet = "\xE2\x80\xA2";
    infoLabel_.setText("SN3D / ACN  |  ORDER " +
                           juce::String(AmbiEncoderAudioProcessor::kOrder) +
                           "  |  " +
                           juce::String(AmbiEncoderAudioProcessor::kNumChannels) +
                           " CH",
                       juce::dontSendNotification);
    infoLabel_.setFont(ambi::AmbiLookAndFeel::regularFont(11.0f));
    infoLabel_.setColour(juce::Label::textColourId, ambi::palette::textDim);
    infoLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(infoLabel_);

    auto setupKnob = [this](juce::Slider &slider, juce::Label &label,
                            const juce::String &text)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 18);
        slider.setColour(juce::Slider::rotarySliderFillColourId,
                         ambi::palette::cyan);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setFont(ambi::AmbiLookAndFeel::regularFont(12.0f));
        label.setColour(juce::Label::textColourId, ambi::palette::textDim);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    };

    setupKnob(gainSlider_, gainLabel_, "GAIN");
    gainSlider_.setTextValueSuffix(" dB");
    setupKnob(spreadSlider_, spreadLabel_, "SPREAD");

    addAndMakeVisible(scope_);

    setResizable(true, true);
    setResizeLimits(440, 340, 900, 720);
    setSize(560, 440);

    startTimerHz(30);
}

AmbiEncoderAudioProcessorEditor::~AmbiEncoderAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void AmbiEncoderAudioProcessorEditor::paint(juce::Graphics &g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(ambi::palette::background);

    juce::ColourGradient vignette(
        ambi::palette::panel.brighter(0.10f), bounds.getCentreX(),
        bounds.getCentreY() * 0.85f, ambi::palette::background,
        bounds.getRight(), bounds.getCentreY(), true);
    g.setGradientFill(vignette);
    g.fillRect(bounds);

    drawMeter(g);
}

void AmbiEncoderAudioProcessorEditor::drawMeter(juce::Graphics &g)
{
    if (meterBounds_.isEmpty())
        return;

    g.setColour(ambi::palette::panel);
    g.fillRoundedRectangle(meterBounds_.toFloat(), 3.0f);
    g.setColour(ambi::palette::panelEdge);
    g.drawRoundedRectangle(meterBounds_.toFloat(), 3.0f, 1.0f);

    auto inner = meterBounds_.toFloat().reduced(2.0f);
    const float p = juce::jlimit(0.0f, 1.0f, displayPeak_);
    if (p > 0.0f)
    {
        const float h = inner.getHeight() * p;
        auto filled = inner.withHeight(h).withY(inner.getBottom() - h);
        juce::ColourGradient grad(ambi::palette::cyan, inner.getCentreX(),
                                  inner.getBottom(), ambi::palette::magenta,
                                  inner.getCentreX(), inner.getY(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(filled, 2.0f);
    }

    if (!clipBounds_.isEmpty())
    {
        const auto clipF = clipBounds_.toFloat();
        if (clipLatched_)
        {
            g.setColour(ambi::palette::red.withAlpha(0.35f));
            g.fillRoundedRectangle(clipF.expanded(2.0f), 5.0f);
        }
        g.setColour(clipLatched_ ? ambi::palette::red
                                 : ambi::palette::red.withAlpha(0.12f));
        g.fillRoundedRectangle(clipF, 3.0f);
        g.setColour(clipLatched_ ? juce::Colours::white
                                 : ambi::palette::textDim);
        g.setFont(ambi::AmbiLookAndFeel::regularFont(9.0f));
        g.drawText("CLIP", clipBounds_, juce::Justification::centred, false);
    }
}

void AmbiEncoderAudioProcessorEditor::resized()
{
    auto b = getLocalBounds().reduced(kMargin);

    auto header = b.removeFromTop(kHeaderHeight);
    titleLabel_.setBounds(header.removeFromTop(30));
    infoLabel_.setBounds(header.removeFromTop(18));

    auto footer = b.removeFromBottom(kFooterHeight);

    // Meter strip down the right of the scope.
    auto strip = b.removeFromRight(kMeterWidth);
    strip = strip.withTrimmedTop(8).withTrimmedBottom(8);
    clipBounds_ = strip.removeFromBottom(32);
    meterBounds_ = strip.withTrimmedBottom(10);

    scope_.setBounds(b.reduced(4));

    auto knobArea = footer.reduced(16, 6);
    auto left = knobArea.removeFromLeft(knobArea.getWidth() / 2);
    auto right = knobArea;

    auto place = [](juce::Rectangle<int> area, juce::Label &label,
                    juce::Slider &slider)
    {
        label.setBounds(area.removeFromTop(18));
        slider.setBounds(area);
    };
    place(left, gainLabel_, gainSlider_);
    place(right, spreadLabel_, spreadSlider_);
}

void AmbiEncoderAudioProcessorEditor::mouseDown(const juce::MouseEvent &)
{
    processorRef_.clearClip();
    clipLatched_ = false;
    repaint(clipBounds_);
}

void AmbiEncoderAudioProcessorEditor::timerCallback()
{
    const float peak = processorRef_.getOutputPeak();
    displayPeak_ = juce::jmax(peak, displayPeak_ * 0.86f);

    if (processorRef_.getClipFlag())
    {
        clipLatched_ = true;
    }

    repaint(meterBounds_.getUnion(clipBounds_));
}
