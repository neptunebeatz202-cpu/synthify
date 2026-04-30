#pragma once
#include <JuceHeader.h>

//==============================================================================
/** A single wavetable oscillator voice with ADSR. */
class SynthVoice
{
public:
    static constexpr int MAX_UNISON_VOICES = 16;

    SynthVoice()  = default;
    ~SynthVoice() = default;

    void prepare     (double sampleRate);
    void setWavetable (const std::vector<float>& table);
    void setADSR     (const juce::ADSR::Parameters& params);
    void setUnison   (int voices, float detune, float blend);

    /** portaCoeff: 1.0 = instant, <1 = glide (one-pole coefficient per sample). */
    void startNote (int midiNote, float velocity,
                    float targetNote, float portaCoeff);
    void stopNote();

    void renderBlock (juce::AudioBuffer<float>& buffer, int numSamples);

    bool  isActive()         const { return active; }
    int   getMidiNote()      const { return midiNote; }
    float getPhase()         const { return (float)phase; }
    /** Safe to call from any thread – updated atomically each block. */
    float getEnvelopeValue() const { return lastEnvValue.load (std::memory_order_relaxed); }

private:
    double sampleRate   { 44100.0 };
    std::vector<float> wavetable;
    int    wtSize       { 2048 };

    int    midiNote     { 60 };
    float  velocity     { 1.0f };
    bool   active       { false };

    double phase        { 0.0 };
    double phaseInc     { 0.0 };

    int   unisonVoices { 1 };
    float unisonDetune { 0.0f };
    float unisonBlend  { 0.0f };
    std::array<double, MAX_UNISON_VOICES> unisonPhase {};
    std::array<double, MAX_UNISON_VOICES> unisonPhaseInc {};

    // Portamento
    float  currentNote  { 60.0f };
    float  targetNote   { 60.0f };
    float  portaCoeff   { 1.0f };   // one-pole coeff; 1.0 = instant

    juce::ADSR adsr;

    // Written on audio thread, read safely from UI thread
    std::atomic<float> lastEnvValue { 0.0f };

    void updatePhaseIncrement();
    float getUnisonOffset (int index) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthVoice)
};
