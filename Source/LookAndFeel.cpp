#include "LookAndFeel.h"

const juce::Colour SynthifyLAF::kBg      { 0xff050505 };
const juce::Colour SynthifyLAF::kPanel   { 0xff0b0b0b };
const juce::Colour SynthifyLAF::kBorder  { 0xff1f1f1f };
const juce::Colour SynthifyLAF::kAccent  { 0xff8b0000 };   // dark blood red
const juce::Colour SynthifyLAF::kAccent2 { 0xff5a1a1a };   // dim rust
const juce::Colour SynthifyLAF::kDim     { 0xff4a4a4a };
const juce::Colour SynthifyLAF::kTrack   { 0xff141414 };

SynthifyLAF::SynthifyLAF()
{
    setColour (juce::Slider::rotarySliderFillColourId,    kAccent);
    setColour (juce::Slider::rotarySliderOutlineColourId, kBorder);
    setColour (juce::Slider::thumbColourId,               kAccent);
    setColour (juce::Slider::trackColourId,               kTrack);
    setColour (juce::Slider::backgroundColourId,          kTrack);
    setColour (juce::Slider::textBoxTextColourId,         kDim);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,    kBorder);

    setColour (juce::Label::textColourId,       kDim);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::TextButton::buttonColourId,  kPanel);
    setColour (juce::TextButton::textColourOffId, kDim);
    setColour (juce::TextButton::textColourOnId,  kAccent);

    setColour (juce::ComboBox::backgroundColourId, kPanel);
    setColour (juce::ComboBox::textColourId,       kDim);
    setColour (juce::ComboBox::outlineColourId,    kBorder);
    setColour (juce::ComboBox::arrowColourId,      kAccent);

    setColour (juce::PopupMenu::backgroundColourId,             kPanel);
    setColour (juce::PopupMenu::textColourId,                   kDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId,  kAccent2);
    setColour (juce::PopupMenu::highlightedTextColourId,        juce::Colours::white);

    setColour (juce::TabbedButtonBar::tabTextColourId,              kDim);
    setColour (juce::TabbedButtonBar::frontTextColourId,            kAccent);
    setColour (juce::TabbedComponent::backgroundColourId,           kBg);
    setColour (juce::TabbedComponent::outlineColourId,              kBorder);
}

//==============================================================================
void SynthifyLAF::drawRotarySlider (juce::Graphics& g,
                                    int x, int y, int width, int height,
                                    float sliderPos,
                                    float startAngle, float endAngle,
                                    juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float)x, (float)y,
                                          (float)width, (float)height).reduced (6.0f);
    auto centre = bounds.getCentre();
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto angle  = startAngle + sliderPos * (endAngle - startAngle);
    auto trackR = radius - 5.0f;

    // Dark filled circle
    g.setColour (kPanel);
    g.fillEllipse (bounds);

    // Outer ring — rough scratchy look
    g.setColour (kBorder);
    g.drawEllipse (bounds.reduced (1.0f), 1.0f);

    // Track arc
    {
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f,
                             startAngle, endAngle, true);
        g.setColour (kTrack);
        g.strokePath (track, juce::PathStrokeType (3.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Value arc — blood red
    if (sliderPos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f,
                             startAngle, angle, true);
        g.setColour (kAccent);
        g.strokePath (value, juce::PathStrokeType (3.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer line
    {
        float px = centre.x + trackR * std::sin (angle);
        float py = centre.y - trackR * std::cos (angle);
        g.setColour (kAccent);
        g.drawLine (centre.x, centre.y, px, py, 1.5f);
        g.fillEllipse (px - 2.5f, py - 2.5f, 5.0f, 5.0f);
    }

    // Centre dot
    g.setColour (kBorder.brighter (0.3f));
    g.fillEllipse (centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
}

//==============================================================================
void SynthifyLAF::drawLinearSlider (juce::Graphics& g,
                                    int x, int y, int width, int height,
                                    float sliderPos, float /*minPos*/, float /*maxPos*/,
                                    juce::Slider::SliderStyle style, juce::Slider&)
{
    if (style == juce::Slider::LinearHorizontal)
    {
        float centreY = (float)y + (float)height * 0.5f;
        float trackH  = 2.0f;

        g.setColour (kTrack);
        g.fillRoundedRectangle ((float)x, centreY - trackH * 0.5f,
                                (float)width, trackH, 1.0f);
        g.setColour (kAccent);
        g.fillRoundedRectangle ((float)x, centreY - trackH * 0.5f,
                                sliderPos - (float)x, trackH, 1.0f);
        g.setColour (kAccent);
        g.fillEllipse (sliderPos - 5.0f, centreY - 5.0f, 10.0f, 10.0f);
        g.setColour (kBorder);
        g.drawEllipse (sliderPos - 5.0f, centreY - 5.0f, 10.0f, 10.0f, 1.0f);
    }
    else if (style == juce::Slider::LinearVertical)
    {
        float centreX = (float)x + (float)width * 0.5f;
        float trackW  = 2.0f;

        g.setColour (kTrack);
        g.fillRoundedRectangle (centreX - trackW * 0.5f, (float)y,
                                trackW, (float)height, 1.0f);
        g.setColour (kAccent);
        g.fillRoundedRectangle (centreX - trackW * 0.5f, sliderPos,
                                trackW, (float)y + (float)height - sliderPos, 1.0f);
        g.setColour (kAccent);
        g.fillEllipse (centreX - 5.0f, sliderPos - 5.0f, 10.0f, 10.0f);
        g.setColour (kBorder);
        g.drawEllipse (centreX - 5.0f, sliderPos - 5.0f, 10.0f, 10.0f, 1.0f);
    }
}

//==============================================================================
void SynthifyLAF::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                       const juce::Colour&,
                                       bool highlighted, bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced (0.5f);

    juce::Colour fill = down        ? kAccent.withAlpha (0.5f)
                      : highlighted ? kAccent2.withAlpha (0.6f)
                      : kPanel;

    g.setColour (fill);
    g.fillRoundedRectangle (b, 3.0f);

    juce::Colour border = highlighted ? kAccent.withAlpha (0.8f)
                                      : kBorder;
    g.setColour (border);
    g.drawRoundedRectangle (b, 3.0f, 1.0f);
}

//==============================================================================
void SynthifyLAF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                   bool highlighted, bool /*down*/)
{
    auto b   = button.getLocalBounds().toFloat().reduced (0.5f);
    bool on  = button.getToggleState();

    g.setColour (on ? kAccent.withAlpha (0.25f) : kPanel);
    g.fillRoundedRectangle (b, 3.0f);

    juce::Colour border = on        ? kAccent
                        : highlighted ? kAccent2
                        : kBorder;
    g.setColour (border);
    g.drawRoundedRectangle (b, 3.0f, 1.0f);

    g.setColour (on ? kAccent : kDim);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds(),
                      juce::Justification::centred, 1);
}

//==============================================================================
juce::Label* SynthifyLAF::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::Font (9.0f));
    label->setColour (juce::Label::textColourId, kDim);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::TextEditor::textColourId, kDim);
    label->setColour (juce::TextEditor::backgroundColourId, kPanel);
    return label;
}

juce::Font SynthifyLAF::getLabelFont (juce::Label&)
{
    return juce::Font (10.0f, juce::Font::bold);
}

//==============================================================================
void SynthifyLAF::paintGrungePanelBg (juce::Graphics& g, juce::Rectangle<int> bounds,
                                      const juce::String& title)
{
    auto bf = bounds.toFloat();

    // Base fill — very dark
    g.setColour (juce::Colour (0xff080808));
    g.fillRoundedRectangle (bf, 4.0f);

    // Scratchy border — drawn as multiple thin lines with slight irregularity
    g.setColour (kBorder);
    g.drawRoundedRectangle (bf.reduced (0.5f), 4.0f, 1.0f);

    // Subtle inner shadow line top/left
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine (bf.getX() + 4.0f, bf.getY() + 1.5f, bf.getRight() - 4.0f, bf.getY() + 1.5f, 1.0f);
    g.drawLine (bf.getX() + 1.5f, bf.getY() + 4.0f, bf.getX() + 1.5f, bf.getBottom() - 4.0f, 1.0f);

    // Accent corner ticks — gothic cross-corner detail
    g.setColour (kAccent.withAlpha (0.4f));
    float cs = 6.0f;
    // top-left
    g.drawLine (bf.getX(), bf.getY() + cs, bf.getX(), bf.getY(), 1.5f);
    g.drawLine (bf.getX(), bf.getY(), bf.getX() + cs, bf.getY(), 1.5f);
    // top-right
    g.drawLine (bf.getRight() - cs, bf.getY(), bf.getRight(), bf.getY(), 1.5f);
    g.drawLine (bf.getRight(), bf.getY(), bf.getRight(), bf.getY() + cs, 1.5f);
    // bottom-left
    g.drawLine (bf.getX(), bf.getBottom() - cs, bf.getX(), bf.getBottom(), 1.5f);
    g.drawLine (bf.getX(), bf.getBottom(), bf.getX() + cs, bf.getBottom(), 1.5f);
    // bottom-right
    g.drawLine (bf.getRight() - cs, bf.getBottom(), bf.getRight(), bf.getBottom(), 1.5f);
    g.drawLine (bf.getRight(), bf.getBottom(), bf.getRight(), bf.getBottom() - cs, 1.5f);

    if (title.isNotEmpty())
    {
        g.setFont (juce::Font (8.5f, juce::Font::bold));
        g.setColour (kAccent.withAlpha (0.55f));
        g.drawText (title, bounds.reduced (8, 3).removeFromTop (14),
                    juce::Justification::topLeft);
    }
}

//==============================================================================
void SynthifyLAF::paintSynthifyTitle (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto bf = bounds.toFloat();

    // Shadow/echo layer — slightly offset
    g.setColour (kAccent.withAlpha (0.25f));
    g.setFont (juce::Font (32.0f, juce::Font::bold | juce::Font::italic));
    g.drawText ("SYNTHIFY", bf.translated (2.0f, 2.0f).toNearestInt(),
                juce::Justification::centredLeft);

    // Main text — dim white
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText ("SYNTHIFY", bf.toNearestInt(), juce::Justification::centredLeft);

    // Thin red underline
    float underY = bf.getBottom() - 3.0f;
    g.setColour (kAccent.withAlpha (0.7f));
    g.drawLine (bf.getX(), underY, bf.getX() + 110.0f, underY, 1.5f);

    // Sub-label — "SYNTHESIZER"
    g.setFont (juce::Font (8.0f, juce::Font::bold));
    g.setColour (kAccent.withAlpha (0.5f));
    g.drawText ("SYNTHESIZER",
                juce::Rectangle<int> ((int)bf.getX(), (int)bf.getBottom() - 14,
                                      120, 12),
                juce::Justification::centredLeft);
}
