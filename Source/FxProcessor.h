#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

//==============================================================================
// HyperDimension — unison detune/spread effect
//==============================================================================
class HyperDimensionFx
{
public:
    HyperDimensionFx();
    void prepare (double sampleRate, int blockSize);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);

    bool  enabled  { false };
    float amount   { 0.5f };  // 0-1
    float detune   { 0.3f };  // 0-1
    float mix      { 0.7f };  // 0-1

private:
    double sr { 44100.0 };
    static constexpr int NUM_LINES = 6;
    std::array<double, NUM_LINES> phases    {};
    std::array<double, NUM_LINES> phaseIncs {};
    std::array<float,  NUM_LINES> detuneHz  {};
    std::array<std::vector<float>, 2> delayBuf;
    std::array<int, NUM_LINES> delayPos {};
    int bufSize { 0 };
    void updatePhaseIncs();
};

//==============================================================================
// Distortion — 5 modes
//==============================================================================
enum class DistortionMode { Tube = 0, SineShaperDist, Downsample, Linear, Rectangular };

class DistortionFx
{
public:
    void prepare (double sampleRate, int blockSize);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);

    bool           enabled { false };
    DistortionMode mode    { DistortionMode::Tube };
    float          drive   { 0.5f };  // 0-1
    float          tone    { 0.5f };  // 0-1
    float          mix     { 1.0f };  // 0-1

private:
    double sr { 44100.0 };
    juce::dsp::IIR::Filter<float> toneFilterL, toneFilterR;
    int downsampleCounter { 0 };
    float downsampleHeld[2] { 0.0f, 0.0f };

    float processSample (float x, int ch);
    void  updateToneFilter();
};

//==============================================================================
// Chorus
//==============================================================================
class ChorusFx
{
public:
    void prepare (double sampleRate, int blockSize);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);

    bool  enabled { false };
    float rate    { 0.5f };   // 0-1 → 0.1-8 Hz
    float depth   { 0.5f };   // 0-1
    float mix     { 0.5f };   // 0-1
    float feedback{ 0.1f };   // 0-1

private:
    double sr { 44100.0 };
    static constexpr int MAX_DELAY_SAMPLES = 4096;
    std::array<std::vector<float>, 2> delayLine;
    std::array<int, 2> writePos { 0, 0 };
    std::array<double, 2> lfoPhase { 0.0, 0.25 }; // L/R offset
    std::array<float, 2>  fbSample { 0.0f, 0.0f };

    float readDelay (int ch, float delaySamples);
};

//==============================================================================
// Phaser
//==============================================================================
class PhaserFx
{
public:
    void prepare (double sampleRate, int blockSize);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);

    bool  enabled  { false };
    float rate     { 0.3f };   // 0-1
    float depth    { 0.7f };   // 0-1
    float feedback { 0.5f };   // 0-1
    float mix      { 0.5f };   // 0-1

private:
    double sr { 44100.0 };
    static constexpr int NUM_STAGES = 6;
    struct AllPassStage { float a1 { 0.0f }; float s { 0.0f }; };
    std::array<std::array<AllPassStage, NUM_STAGES>, 2> stages;
    std::array<double, 2> lfoPhase { 0.0, 0.1 };
    std::array<float,  2> fbSample { 0.0f, 0.0f };

    float processAllPass (AllPassStage& st, float x, float coeff);
};

//==============================================================================
// Flanger
//==============================================================================
class FlangerFx
{
public:
    void prepare (double sampleRate, int blockSize);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);

    bool  enabled  { false };
    float rate     { 0.3f };   // 0-1
    float depth    { 0.5f };   // 0-1
    float feedback { 0.5f };   // 0-1
    float mix      { 0.5f };   // 0-1

private:
    double sr { 44100.0 };
    static constexpr int MAX_DELAY_SAMPLES = 2048;
    std::array<std::vector<float>, 2> delayLine;
    std::array<int,    2> writePos { 0, 0 };
    std::array<double, 2> lfoPhase { 0.0, 0.5 };
    std::array<float,  2> fbSample { 0.0f, 0.0f };

    float readDelay (int ch, float delaySamples);
};

//==============================================================================
// Reverb — Hall and Plate modes
//==============================================================================
enum class ReverbMode { Hall = 0, Plate };

class ReverbFx
{
public:
    ReverbFx();
    void prepare (double sampleRate, int blockSize);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);

    bool       enabled    { false };
    ReverbMode mode       { ReverbMode::Hall };
    float      size       { 0.7f };   // 0-1 (room size / decay)
    float      damping    { 0.5f };   // 0-1
    float      width      { 0.8f };   // 0-1
    float      mix        { 0.35f };  // 0-1
    float      predelay   { 0.0f };   // 0-1 → 0-100ms

private:
    double sr { 44100.0 };
    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;

    // Pre-delay line
    static constexpr int MAX_PREDELAY = 8820; // 200ms @ 44.1k
    std::array<std::vector<float>, 2> predelayBuf;
    std::array<int, 2> predelayWrite { 0, 0 };

    void updateReverbParams();
    float readPredelay (int ch, int delaySamples);
};

//==============================================================================
// FxChain — owns all modules, processed in order
//==============================================================================
class FxChain
{
public:
    FxChain() = default;
    ~FxChain() = default;

    void prepare (double sampleRate, int blockSize);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);

    HyperDimensionFx hyperDimension;
    DistortionFx     distortion;
    ChorusFx         chorus;
    PhaserFx         phaser;
    FlangerFx        flanger;
    ReverbFx         reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxChain)
};
