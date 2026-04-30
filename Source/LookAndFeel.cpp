#include "LookAndFeel.h"

DigitalLookAndFeel::DigitalLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::white);
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff242424));
    setColour (juce::Slider::thumbColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff111111));
    setColour (juce::TextButton::textColourOffId, juce::Colours::white);
}

void DigitalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (5.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (juce::Colour (0xff050505));
    g.fillEllipse (bounds);
    g.setColour (juce::Colour (0xff2a2a2a));
    g.drawEllipse (bounds, 1.5f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 7.0f, radius - 7.0f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff222222));
    g.strokePath (track, juce::PathStrokeType (3.0f));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 7.0f, radius - 7.0f, 0.0f,
                         rotaryStartAngle, angle, true);
    g.setColour (juce::Colours::white);
    g.strokePath (value, juce::PathStrokeType (3.0f));

    juce::Path pointer;
    pointer.addRectangle (-1.5f, -radius + 10.0f, 3.0f, radius * 0.38f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.fillPath (pointer);
}

void DigitalLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                               const juce::Colour&, bool highlighted, bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? juce::Colour (0xff303030)
                      : highlighted ? juce::Colour (0xff1f1f1f) : juce::Colour (0xff101010));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.9f : 0.45f));
    g.drawRoundedRectangle (b, 6.0f, 1.0f);
}

void DigitalLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                           bool highlighted, bool)
{
    auto b = button.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (button.getToggleState() ? juce::Colours::white : juce::Colour (0xff101010));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.9f : 0.45f));
    g.drawRoundedRectangle (b, 6.0f, 1.0f);
    g.setColour (button.getToggleState() ? juce::Colours::black : juce::Colours::white);
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
}
