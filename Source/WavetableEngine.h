#pragma once
#include <JuceHeader.h>
#include "SynthVoice.h"

//==============================================================================
class WavetableEngine
{
public:
    static constexpr int MAX_VOICES = 8;
    static constexpr int NUM_OSC    = 3;

    WavetableEngine();
    ~WavetableEngine() = default;

    void prepare         (double sampleRate, int blockSize);
    void setWavetable    (int oscIndex, const std::vector<float>& table);
    void setOscVolume    (int oscIndex, float volume);
    void setOscPan       (int oscIndex, float pan);
    void setOscActive    (int oscIndex, bool active);

    /** semitones = coarse pitch offset; cents = fine detune offset */
    void setOscPitch     (int oscIndex, float semitones, float cents);

    /** Per-osc unison voice count (1-8) and detune amount (0-1). */
    void setOscUnison    (int oscIndex, int count, float detuneAmount);

    /** Per-osc starting phase offset (0-1). */
    void setOscPhase     (int oscIndex, float phase01);

    void setADSR         (float attack, float decay, float sustain, float release);
    void setMono         (bool mono);
    void setLegato       (bool legato);
    void setPortamento   (float glideTime0to1);
    void setFilterParams (float cutoffHz, float resonance);
    void setWidth        (float width);

    void renderNextBlock (juce::AudioBuffer<float>& buffer,
                          juce::MidiBuffer& midi,
                          int startSample, int numSamples);

    float getPlaybackPhase()     const;
    float getADSREnvelopeValue() const;

private:
    double sampleRate { 44100.0 };

    struct OscLayer
    {
        std::array<SynthVoice, MAX_VOICES> voices;
        std::array<int, MAX_VOICES>        voiceStamp { 0,0,0,0,0,0,0,0 };
        int   voiceClock { 0 };
        float volume     { 1.0f };
        float pan        { 0.0f };
        float pitchSemis { 0.0f };
        float detuneCents{ 0.0f };
        bool  active     { true };
    };

    std::array<OscLayer, NUM_OSC> oscLayers;

    bool  monoMode         { false };
    bool  legatoMode       { false };
    float portamentoCoeff  { 1.0f };
    float currentMonoNote  { 60.0f };
    float targetMonoNote   { 60.0f };
    std::vector<int> monoHeldNotes;

    juce::dsp::LadderFilter<float> filter;
    juce::AudioBuffer<float>       voiceBuf;
    juce::AudioBuffer<float>       layerBuf;

    float widthAmount { 1.0f };
    juce::ADSR::Parameters adsrParams;

    void handleNoteOn  (int note, float velocity);
    void handleNoteOff (int note);
    void applyWidth    (juce::AudioBuffer<float>& buf, int startSample, int numSamples);

    float applyPitchOffset (const OscLayer& layer, float midiNote) const;

    juce::CriticalSection wavetableLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableEngine)
};
