#pragma once
#include <JuceHeader.h>

class SynthifyProcessor;

//==============================================================================
/** Waveform display for a single oscillator. Accepts file drag-drop,
    shows the sample waveform with a WT-position marker the user can drag. */
class OscWaveformDisplay  : public juce::Component,
                             public juce::FileDragAndDropTarget,
                             private juce::Timer
{
public:
    OscWaveformDisplay (SynthifyProcessor& p, int oscIndex);
    ~OscWaveformDisplay() override;

    void paint        (juce::Graphics& g) override;
    void mouseDown    (const juce::MouseEvent& e) override;
    void mouseDrag    (const juce::MouseEvent& e) override;
    void visibilityChanged() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true;  repaint(); }
    void fileDragExit  (const juce::StringArray&)            override { dragOver = false; repaint(); }

private:
    SynthifyProcessor& proc;
    int  oscIndex;
    bool dragOver     { false };
    float playPhase   { 0.0f };

    void timerCallback() override;
    void setPositionFromX (int x);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscWaveformDisplay)
};
