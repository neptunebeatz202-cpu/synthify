#pragma once

#include <JuceHeader.h>

class DigitalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DigitalLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool highlighted, bool down) override;
};
