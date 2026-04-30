#pragma once
#include <JuceHeader.h>
#include "SynthVoice.h"

//==============================================================================
/** Manages a pool of SynthVoices and applies a global ladder filter. */
class WavetableEngine
{
public:
    static constexpr int MAX_VOICES = 8;

    WavetableEngine();
    ~WavetableEngine() = default;

    void prepare         (double sampleRate, int blockSize);
    void setWavetable    (const std::vector<float>& table);
    void setADSR         (float attack, float decay, float sustain, float release);
    void setMono         (bool mono);
    void setLegato       (bool legato);
    void setPortamento   (float glideTime0to1);
    void setUnison       (int voices, float detune, float blend);
    void setFilterParams (float cutoffHz, float resonance);

    void renderNextBlock (juce::AudioBuffer<float>& buffer,
                          juce::MidiBuffer& midi,
                          int startSample, int numSamples);

    /** Returns 0-1 phase of the oldest active voice (for waveform display). */
    float getPlaybackPhase() const;
    /** Returns current ADSR envelope value of the first active voice. */
    float getADSREnvelopeValue() const;

private:
    double sampleRate { 44100.0 };

    std::array<SynthVoice, MAX_VOICES> voices;

    // Round-robin voice stamp — higher = more recently triggered
    std::array<int, MAX_VOICES> voiceStamp { 0,0,0,0,0,0,0,0 };
    int voiceClock { 0 };

    bool  monoMode { false };
    bool  legatoMode { false };
    int   unisonVoices { 1 };
    float unisonDetune { 0.0f };
    float unisonBlend  { 0.0f };

    juce::dsp::LadderFilter<float> filter;

    // Pre-allocated mix buffer (avoids heap allocation on audio thread)
    juce::AudioBuffer<float> voiceBuf;

    // Mono portamento state
    float currentMonoNote { 60.0f };
    float targetMonoNote  { 60.0f };
    float portamentoCoeff { 1.0f };
    std::vector<int> monoHeldNotes;

    juce::ADSR::Parameters adsrParams;

    void handleNoteOn  (int note, float velocity);
    void handleNoteOff (int note);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableEngine)
};
