#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WaveformDisplay.h"
#include "ADSRVisualizer.h"
#include "LookAndFeel.h"

//==============================================================================
// Forward declarations
class OscillatorPanel;
class FxPanel;
class MainPanel;

//==============================================================================
/** Full plugin editor — tabbed between SYNTH and FX views. */
class SynthifyEditor  : public juce::AudioProcessorEditor,
                       private juce::Button::Listener,
                       private SynthifyProcessor::Listener
{
public:
    explicit SynthifyEditor (SynthifyProcessor&);
    ~SynthifyEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    SynthifyProcessor& proc;
    SynthifyLAF        laf;

    //── Tab bar ─────────────────────────────────────────────────────────────────
    juce::TextButton synthTabBtn { "SYNTH" };
    juce::TextButton fxTabBtn    { "FX" };
    int currentTab { 0 };   // 0 = synth, 1 = fx

    //── Synth panel ──────────────────────────────────────────────────────────────
    static constexpr int NUM_OSC = 3;

    struct OscPanel
    {
        OscWaveformDisplay waveform;

        juce::Slider wtSlider;
        juce::Label  wtLabel;

        juce::Slider volSlider,  panSlider,  pitchSlider,  detuneSlider, phaseSlider;
        juce::Label  volLabel,   panLabel,   pitchLabel,   detuneLabel,  phaseLabel;
        juce::Slider unisonSlider, unisonDetuneSlider;
        juce::Label  unisonLabel,  unisonDetuneLabel;

        juce::ToggleButton oscOnBtn;
        juce::TextButton   openBtn    { "OPEN" };
        juce::TextButton   analyzeBtn { "SCAN" };
        juce::TextButton   cancelBtn  { "CANCEL" };
        juce::Label        titleLabel;

        using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
        using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

        std::unique_ptr<SA> wtAttach, volAttach, panAttach, pitchAttach, detuneAttach, phaseAttach, unisonAttach, unisonDetuneAttach;
        std::unique_ptr<BA> onAttach;

        explicit OscPanel (SynthifyProcessor& p, int idx)
            : waveform (p, idx),
              oscOnBtn    ("ON"),
              titleLabel  ("", "OSC " + juce::String (idx + 1)) {}
    };

    std::array<std::unique_ptr<OscPanel>, NUM_OSC> oscPanels;

    //── ADSR ────────────────────────────────────────────────────────────────────
    ADSRVisualizer adsrViz;
    juce::Slider   attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label    attackLabel,  decayLabel,  sustainLabel,  releaseLabel;

    //── Global controls ─────────────────────────────────────────────────────────
    juce::Slider       portaSlider, gainSlider, cutoffSlider, resSlider, widthSlider;
    juce::Label        portaLabel,  gainLabel,  cutoffLabel,  resLabel,  widthLabel;
    juce::ToggleButton monoBtn  { "MONO" };
    juce::ToggleButton legatoBtn{ "LEGATO" };

    //── Status ───────────────────────────────────────────────────────────────────
    juce::Label statusLabel;

    //── APVTS attachments ───────────────────────────────────────────────────────
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SA> attackAtt, decayAtt, sustainAtt, releaseAtt;
    std::unique_ptr<SA> portaAtt, gainAtt, cutoffAtt, resAtt, widthAtt;
    std::unique_ptr<BA> monoAtt, legatoAtt;

    //── FX Panel ─────────────────────────────────────────────────────────────────
    struct FxModule
    {
        juce::ToggleButton enableBtn;
        juce::Label        nameLabel;
        juce::OwnedArray<juce::Slider> knobs;
        juce::OwnedArray<juce::Label>  knobLabels;
        juce::ComboBox     modeBox;  // used by Distortion and Reverb
        bool hasModeBox { false };

        // Attachments
        std::unique_ptr<BA> enableAttach;
        juce::OwnedArray<SA> knobAttachments;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    };

    std::unique_ptr<FxModule> fxHyper, fxDist, fxChorus, fxPhaser, fxFlanger, fxReverb;

    //── File chooser ─────────────────────────────────────────────────────────────
    std::unique_ptr<juce::FileChooser> fileChooser;
    int  activeFileOsc { 0 };
    bool analysisOverlay[NUM_OSC] { false, false, false };

    //── Helpers ─────────────────────────────────────────────────────────────────
    void makeRotary   (juce::Slider& s);
    void makeLabel    (juce::Label& l, const juce::String& text);
    void openFileForOsc (int oscIndex);

    void layoutSynthTab();
    void layoutFxTab();
    void buildFxModules();
    FxModule* buildFxModule (const juce::String& name,
                              const juce::String& enableID,
                              const juce::StringArray& knobIDs,
                              const juce::StringArray& knobNames);

    void buttonClicked (juce::Button* b) override;

    // Processor listener
    void analysisStarted   (int oscIndex) override;
    void analysisFinished  (int oscIndex) override;
    void analysisCancelled (int oscIndex) override;
    void sampleLoaded      (int oscIndex) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthifyEditor)
};
