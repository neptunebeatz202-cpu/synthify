#pragma once
#include <JuceHeader.h>
#include <array>

//==============================================================================
/** A single wavetable oscillator voice with ADSR, portamento and Serum-style
    unison (up to 8 sub-oscillators detuned and panned across the stereo field). */
class SynthVoice
{
public:
    static constexpr int MAX_UNISON = 8;

    SynthVoice()  = default;
    ~SynthVoice() = default;

    void prepare      (double sampleRate);
    void setWavetable (const std::vector<float>& table);
    void setADSR      (const juce::ADSR::Parameters& params);

    /** Set unison voice count (1-8) and detune amount (0-1). */
    void setUnison    (int count, float detuneAmount);

    /** 0..1 — initial phase offset applied on note start when resetPhase=true */
    void setPhaseOffset (float phase01);

    void startNote (int midiNote, float velocity, float targetNote,
                    float portaCoeff, bool resetEnvelope, bool resetPhase);
    void stopNote();

    void renderBlock (juce::AudioBuffer<float>& buffer, int numSamples);

    bool  isActive()         const { return active; }
    int   getMidiNote()      const { return midiNote; }
    float getPhase()         const { return (float)phases[0]; }
    float getEnvelopeValue() const { return lastEnvValue.load (std::memory_order_relaxed); }

private:
    double sampleRate { 44100.0 };

    std::vector<float> wavetable;
    int  wtSize    { 2048 };

    int   midiNote  { 60 };
    float velocity  { 1.0f };
    bool  active    { false };

    // Per-unison-voice state
    int   unisonCount   { 1 };
    float unisonDetune  { 0.3f }; // 0-1
    float phaseOffset01 { 0.0f };

    std::array<double, MAX_UNISON> phases    {};
    std::array<double, MAX_UNISON> phaseIncs {};
    std::array<float,  MAX_UNISON> panL      {};
    std::array<float,  MAX_UNISON> panR      {};
    std::array<float,  MAX_UNISON> detuneSemi{};

    float currentNote { 60.0f };
    float targetNote  { 60.0f };
    float portaCoeff  { 1.0f };

    juce::ADSR         adsr;
    std::atomic<float> lastEnvValue { 0.0f };

    void  updateUnisonLayout();
    void  updatePhaseIncrements();
    float readWavetableAtPhase (double ph) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthVoice)
};
