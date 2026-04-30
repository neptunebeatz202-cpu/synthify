#pragma once
#include <JuceHeader.h>

//==============================================================================
/** Analyses audio buffers to find optimal wavetable extraction points and
    detect fundamental pitch.  All heavy work is designed to run off the
    message thread so the UI stays responsive. */
class SampleAnalyzer
{
public:
    SampleAnalyzer()  = default;
    ~SampleAnalyzer() = default;

    /** Detect pitch of a buffer using autocorrelation.
        Returns frequency in Hz, or 0 if unpitched / silent. */
    float detectPitch (const float* data, int size, float sampleRate);

    /** Scan the whole sample buffer and find the normalised position (0-1)
        that contains the most stable, pitched single cycle.
        cancelFlag lets the caller abort mid-scan. */
    float findBestWavetablePosition (const juce::AudioBuffer<float>& buffer,
                                     float sampleRate,
                                     std::atomic<bool>& cancelFlag);

private:
    float zeroCrossingRate       (const float* data, int size);
    float rmsEnergy              (const float* data, int size);
    float autocorrelationPeriod  (const float* data, int size, float sampleRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleAnalyzer)
};
