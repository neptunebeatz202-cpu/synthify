#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WaveformDisplay.h"
#include "ADSRVisualizer.h"
#include "LookAndFeel.h"

class WaveForgeEditor : public juce::AudioProcessorEditor,
                        public juce::FileDragAndDropTarget,
                        private juce::Button::Listener,
                        private WaveForgeProcessor::Listener
{
public:
    explicit WaveForgeEditor (WaveForgeProcessor&);
    ~WaveForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    WaveForgeProcessor& audioProcessor;
    DigitalLookAndFeel lookAndFeel;

    WaveformDisplay waveform;
    ADSRVisualizer adsrView;

    juce::TextButton browseButton { "OPEN" };
    juce::TextButton analyzeButton { "ANALYZE" };
    juce::TextButton cancelButton { "CANCEL" };
    juce::ToggleButton monoButton { "MONO" };

    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Slider portaSlider, gainSlider, cutoffSlider, resSlider, wtSlider;
    juce::Slider unisonSlider, detuneSlider, blendSlider;
    juce::Label titleLabel, statusLabel;
    juce::Label unisonLabel, detuneLabel, blendLabel, portaLabel, gainLabel, cutoffLabel, resLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> attackAttachment, decayAttachment, sustainAttachment, releaseAttachment;
    std::unique_ptr<SliderAttachment> portaAttachment, gainAttachment, cutoffAttachment, resAttachment, wtAttachment;
    std::unique_ptr<SliderAttachment> unisonAttachment, detuneAttachment, blendAttachment;
    std::unique_ptr<ButtonAttachment> monoAttachment;
    std::unique_ptr<juce::FileChooser> fileChooser;

    bool analysisOverlayVisible { false };

    void configureSlider (juce::Slider& slider);
    void configureControlLabel (juce::Label& label, const juce::String& text);
    void layoutControl (juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label);
    void loadFile (const juce::File& file);
    void buttonClicked (juce::Button* button) override;
    void drawAnalysisOverlay (juce::Graphics& g);

    void analysisStarted() override;
    void analysisFinished() override;
    void analysisCancelled() override;
    void sampleLoaded() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveForgeEditor)
};
