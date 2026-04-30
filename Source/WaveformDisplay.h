#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class WaveformDisplay : public juce::Component,
                        private juce::Timer
{
public:
    explicit WaveformDisplay (WaveForgeProcessor& processor);
    ~WaveformDisplay() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    WaveForgeProcessor& audioProcessor;
    float playbackPhase { 0.0f };
    bool draggingEndMarker { false };

    void setLoopPointFromX (int x);
    void timerCallback() override;
};
