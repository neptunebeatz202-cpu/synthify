#pragma once

#include <JuceHeader.h>
#include "WavetableEngine.h"
#include "SampleAnalyzer.h"
#include "FxProcessor.h"

//==============================================================================
class SynthifyProcessor  : public juce::AudioProcessor,
                          public juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int NUM_OSC = 3;

    SynthifyProcessor();
    ~SynthifyProcessor() override;

    //── AudioProcessor ─────────────────────────────────────────────────────────
    void prepareToPlay    (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock     (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor()               const override { return true; }
    const juce::String getName()   const override { return JucePlugin_Name; }
    bool acceptsMidi()             const override { return true; }
    bool producesMidi()            const override { return false; }
    bool isMidiEffect()            const override { return false; }
    double getTailLengthSeconds()  const override { return 2.0; }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // Stereo output is required.
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
            return false;
        // Accept no input or a disabled input bus — do NOT reject non-disabled inputs
        // because some AU/VST3 hosts pass a mono or stereo sidechain input.
        return true;
    }

    int getNumPrograms()                              override { return 1; }
    int getCurrentProgram()                           override { return 0; }
    void setCurrentProgram (int)                      override {}
    const juce::String getProgramName (int)           override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //── Parameters ─────────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String& paramID, float newValue) override;

    //── Per-oscillator API ──────────────────────────────────────────────────────
    bool  loadSampleFile       (int oscIndex, const juce::File& file);
    void  setWavetablePosition (int oscIndex, float normalizedPos);
    float getWavetablePosition (int oscIndex) const;

    void  startAnalysis  (int oscIndex);
    void  cancelAnalysis (int oscIndex);
    bool  isAnalyzing    (int oscIndex) const;

    bool hasSampleLoaded (int oscIndex) const;
    const juce::AudioBuffer<float>& getSampleBuffer (int oscIndex) const;

    //── Engine / FX access ──────────────────────────────────────────────────────
    WavetableEngine& getEngine() { return engine; }
    FxChain&         getFx()     { return fxChain; }

    //── Listeners ───────────────────────────────────────────────────────────────
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void analysisStarted   (int) {}
        virtual void analysisFinished  (int) {}
        virtual void analysisCancelled (int) {}
        virtual void sampleLoaded      (int) {}
    };
    void addListener    (Listener* l) { listeners.add    (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    WavetableEngine engine;
    FxChain         fxChain;

    //── Per-oscillator state ────────────────────────────────────────────────────
    struct OscState
    {
        SampleAnalyzer             analyzer;
        juce::AudioBuffer<float>   sampleBuffer;
        double                     sampleFileSampleRate { 44100.0 };
        std::atomic<float>         wavetablePosition    { 0.5f };
        std::atomic<bool>          sampleLoaded         { false };
        std::atomic<bool>          analyzing            { false };
        std::atomic<bool>          analysisCancel       { false };
        std::unique_ptr<std::thread> analysisThread;
    };

    std::array<OscState, NUM_OSC> oscStates;
    juce::ListenerList<Listener>  listeners;

    void rebuildWavetableFromPosition (int oscIndex, float pos);
    void onAnalysisComplete           (int oscIndex, float bestPos);
    void applyFxParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthifyProcessor)
};
