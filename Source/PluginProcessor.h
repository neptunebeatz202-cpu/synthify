#pragma once

#include <JuceHeader.h>
#include "WavetableEngine.h"
#include "SampleAnalyzer.h"

//==============================================================================
class WaveForgeProcessor  : public juce::AudioProcessor,
                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    WaveForgeProcessor();
    ~WaveForgeProcessor() override;

    //── AudioProcessor ─────────────────────────────────────────────────────────
    void prepareToPlay   (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock    (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName()   const override { return JucePlugin_Name; }
    bool acceptsMidi()             const override { return true; }
    bool producesMidi()            const override { return false; }
    bool isMidiEffect()            const override { return false; }
    double getTailLengthSeconds()  const override { return 0.0; }

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

    //── Sample / Wavetable API ──────────────────────────────────────────────────
    bool  loadSampleFile           (const juce::File& file);
    void  setWavetablePosition     (float normalizedPosition);
    float getWavetablePosition()   const { return wavetablePosition.load(); }

    void startAnalysis();
    void cancelAnalysis();          // public method to cancel background analysis
    bool isAnalyzing()     const   { return analyzing.load(); }

    void onAnalysisComplete (float bestPosition);

    bool hasSampleLoaded() const                    { return sampleLoaded.load(); }
    const juce::AudioBuffer<float>& getSampleBuffer() const { return sampleBuffer; }

    //── Engine / Analyzer access ────────────────────────────────────────────────
    WavetableEngine& getEngine()   { return engine; }
    SampleAnalyzer&  getAnalyzer() { return analyzer; }

    //── Listener interface ──────────────────────────────────────────────────────
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void analysisStarted()   {}
        virtual void analysisFinished()  {}
        virtual void analysisCancelled() {}
        virtual void sampleLoaded()      {}
    };
    void addListener    (Listener* l) { listeners.add    (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    WavetableEngine engine;
    SampleAnalyzer  analyzer;

    juce::AudioBuffer<float> sampleBuffer;
    double                   sampleFileSampleRate { 44100.0 };

    std::atomic<float> wavetablePosition { 0.5f };
    std::atomic<bool>  sampleLoaded      { false };
    std::atomic<bool>  analyzing         { false };

    juce::ListenerList<Listener> listeners;

    // Background analysis thread
    std::unique_ptr<std::thread> analysisThread;
    /** Flag set by cancelAnalysis() to request the background thread to stop. */
    std::atomic<bool> analysisCancel { false };   // renamed — was 'cancelAnalysis'

    void rebuildWavetableFromPosition (float pos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveForgeProcessor)
};
