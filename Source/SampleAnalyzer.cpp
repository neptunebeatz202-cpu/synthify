#include "SampleAnalyzer.h"

float SampleAnalyzer::zeroCrossingRate (const float* data, int size)
{
    int crossings = 0;
    for (int i = 1; i < size; i++)
        if ((data[i - 1] >= 0.0f) != (data[i] >= 0.0f))
            ++crossings;
    return (float)crossings / (float)size;
}

float SampleAnalyzer::rmsEnergy (const float* data, int size)
{
    float sum = 0.0f;
    for (int i = 0; i < size; i++) sum += data[i] * data[i];
    return std::sqrt (sum / (float)size);
}

float SampleAnalyzer::autocorrelationPeriod (const float* data, int size, float sampleRate)
{
    int minLag = (int)(sampleRate / 2000.0f);
    int maxLag = std::min ((int)(sampleRate / 50.0f), size / 2);
    if (minLag >= maxLag) return -1.0f;

    float bestCorr = -1.0e9f;
    int   bestLag  = -1;

    for (int lag = minLag; lag <= maxLag; lag++)
    {
        float corr = 0.0f;
        for (int i = 0; i < size - lag; i++)
            corr += data[i] * data[i + lag];
        if (corr > bestCorr) { bestCorr = corr; bestLag = lag; }
    }
    return bestLag > 0 ? (float)bestLag : -1.0f;
}

float SampleAnalyzer::detectPitch (const float* data, int size, float sampleRate)
{
    float period = autocorrelationPeriod (data, size, sampleRate);
    return period > 0.0f ? sampleRate / period : 0.0f;
}

float SampleAnalyzer::findBestWavetablePosition (const juce::AudioBuffer<float>& buffer,
                                                  float sampleRate,
                                                  std::atomic<bool>& cancelFlag)
{
    constexpr int windowSize = 2048;
    constexpr int stepSize   = 512;

    int totalSamples = buffer.getNumSamples();
    int numCh        = buffer.getNumChannels();

    float bestScore = -1.0e9f;
    int   bestStart = juce::jlimit (0, std::max (0, totalSamples - windowSize),
                                    totalSamples / 2 - windowSize / 2);

    std::vector<float> mono ((size_t)windowSize);

    for (int start = 0; start <= totalSamples - windowSize; start += stepSize)
    {
        if (cancelFlag.load (std::memory_order_relaxed)) return 0.5f;

        for (int i = 0; i < windowSize; i++)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ch++)
                s += buffer.getSample (ch, start + i);
            mono[(size_t)i] = s / (float)numCh;
        }

        float rms = rmsEnergy (mono.data(), windowSize);
        if (rms < 0.01f) continue;

        float period = autocorrelationPeriod (mono.data(), windowSize, sampleRate);
        if (period <= 0.0f) continue;

        float zcr         = zeroCrossingRate (mono.data(), windowSize);
        float expectedZcr = 1.0f / period;
        float periodScore = 1.0f / (1.0f + std::abs (zcr - expectedZcr) * 100.0f);
        float score       = rms * periodScore;

        if (score > bestScore) { bestScore = score; bestStart = start; }
    }

    float bestPos = (float)(bestStart + windowSize / 2) / (float)totalSamples;
    return juce::jlimit (0.0f, 1.0f, bestPos);
}
