#pragma once
#include <JuceHeader.h>

//==============================================================================
/** Analyses audio buffers to find optimal wavetable extraction points and
    detect fundamental pitch. All heavy work runs off the message thread. */
class SampleAnalyzer
{
public:
    SampleAnalyzer()  = default;
    ~SampleAnalyzer() = default;

    /** Detect pitch using autocorrelation. Returns Hz, or 0 if unpitched. */
    float detectPitch (const float* data, int size, float sampleRate);

    /** Scan the buffer and return the 0-1 position with the most stable
        pitched single cycle. cancelFlag lets the caller abort mid-scan. */
    float findBestWavetablePosition (const juce::AudioBuffer<float>& buffer,
                                     float sampleRate,
                                     std::atomic<bool>& cancelFlag);

private:
    float zeroCrossingRate      (const float* data, int size);
    float rmsEnergy             (const float* data, int size);
    float autocorrelationPeriod (const float* data, int size, float sampleRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleAnalyzer)
};
