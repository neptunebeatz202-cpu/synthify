#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ADSRVisualizer : public juce::Component,
                       private juce::Timer
{
public:
    explicit ADSRVisualizer (WaveForgeProcessor& processor);
    ~ADSRVisualizer() override;

    void paint (juce::Graphics& g) override;

private:
    WaveForgeProcessor& audioProcessor;
    float envelopeValue { 0.0f };

    void timerCallback() override;
};
