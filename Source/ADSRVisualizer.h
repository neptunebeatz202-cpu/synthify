#pragma once
#include <JuceHeader.h>

class SynthifyProcessor;

//==============================================================================
/** Draws the ADSR shape with a live playhead showing the current envelope value. */
class ADSRVisualizer  : public juce::Component,
                        private juce::Timer
{
public:
    explicit ADSRVisualizer (SynthifyProcessor& p);
    ~ADSRVisualizer() override;

    void paint (juce::Graphics& g) override;
    void visibilityChanged() override;

private:
    SynthifyProcessor& proc;
    float envelopeValue { 0.0f };

    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ADSRVisualizer)
};
