#include "FxProcessor.h"

//==============================================================================
// HyperDimension
//==============================================================================
HyperDimensionFx::HyperDimensionFx()
{
    // Initialise detune amounts per line (distributed +/- detune range)
    const float semis[NUM_LINES] = { -0.5f, -0.3f, -0.1f, 0.1f, 0.3f, 0.5f };
    for (int i = 0; i < NUM_LINES; ++i)
        detuneHz[i] = semis[i];  // multiplied by detune param at runtime
}

void HyperDimensionFx::prepare (double sampleRate, int blockSize)
{
    sr = sampleRate;
    bufSize = static_cast<int> (sampleRate * 0.05); // 50ms max delay
    bufSize = juce::nextPowerOfTwo (bufSize);

    for (int ch = 0; ch < 2; ++ch)
    {
        delayBuf[ch].assign ((size_t)bufSize, 0.0f);
    }
    for (int i = 0; i < NUM_LINES; ++i)
    {
        phases[i] = 0.0;
        delayPos[i] = 0;
    }
    updatePhaseIncs();
    juce::ignoreUnused (blockSize);
}

void HyperDimensionFx::updatePhaseIncs()
{
    // Vibrato LFO rates per line — slightly different to avoid beating
    const double baseRates[NUM_LINES] = { 0.91, 1.03, 1.17, 0.87, 1.11, 0.97 };
    float mappedDetune = detune * 0.02f; // max ~0.02 semitone shift
    for (int i = 0; i < NUM_LINES; ++i)
    {
        phaseIncs[i] = baseRates[i] * (double)(amount * 4.0f + 0.5f) / sr;
        detuneHz[i] = (i < NUM_LINES / 2 ? -mappedDetune : mappedDetune)
                    * (float)(i % 3 + 1) * 0.5f;
    }
}

void HyperDimensionFx::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!enabled) return;

    updatePhaseIncs();

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const float wetGain = mix;
    const float dryGain = 1.0f - mix * 0.5f; // partial dry retained

    for (int s = 0; s < numSamples; ++s)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            float dry = buffer.getSample (ch, s);
            float wet = 0.0f;

            // Sum contributions from each unison line
            for (int line = 0; line < NUM_LINES; ++line)
            {
                // Skip lines not used by this channel (interleave L/R)
                if ((line % 2) != ch && numCh > 1) continue;

                // LFO-modulated delay (vibrato)
                double lfo     = std::sin (phases[line] * juce::MathConstants<double>::twoPi);
                double maxDlyS = sr * 0.012; // 12ms max
                double delayS  = maxDlyS * (0.5 + 0.5 * lfo) * (double)amount;
                int    d0      = (int)delayS;
                float  frac    = (float)(delayS - d0);

                int writeIdx = delayPos[line] & (bufSize - 1);
                delayBuf[ch][(size_t)writeIdx] = dry;

                int r0 = (delayPos[line] - d0 - 1) & (bufSize - 1);
                int r1 = (r0 + 1) & (bufSize - 1);
                float delayed = delayBuf[ch][(size_t)r0] * (1.0f - frac)
                              + delayBuf[ch][(size_t)r1] * frac;

                wet += delayed;
                delayPos[line] = (delayPos[line] + 1) & (bufSize - 1);
                phases[line]   = std::fmod (phases[line] + phaseIncs[line], 1.0);
            }

            int linesPerCh = NUM_LINES / juce::jmax (1, numCh);
            if (linesPerCh > 0) wet /= (float)linesPerCh;

            float out = dry * dryGain + wet * wetGain;
            buffer.setSample (ch, s, out);
        }
    }
}

//==============================================================================
// Distortion
//==============================================================================
void DistortionFx::prepare (double sampleRate, int /*blockSize*/)
{
    sr = sampleRate;
    updateToneFilter();
}

void DistortionFx::updateToneFilter()
{
    // Tone: 0 = dark (LPF), 1 = bright (HPF transition)
    float fc = 200.0f + tone * 18000.0f;
    auto  coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, (double)fc, 0.707);
    *toneFilterL.coefficients = *coeffs;
    *toneFilterR.coefficients = *coeffs;
}

float DistortionFx::processSample (float x, int ch)
{
    float driveAmt = 1.0f + drive * 39.0f; // 1 – 40x

    switch (mode)
    {
        case DistortionMode::Tube:
        {
            // Soft-clip tube-style: asymmetric tanh
            float driven = x * driveAmt;
            float pos = std::tanh (driven * 1.2f);
            float neg = std::tanh (driven * 0.8f);
            return (driven >= 0.0f ? pos : neg) * 0.9f;
        }

        case DistortionMode::SineShaperDist:
        {
            float driven = x * driveAmt;
            float clipped = juce::jlimit (-1.0f, 1.0f, driven);
            return std::sin (clipped * juce::MathConstants<float>::halfPi);
        }

        case DistortionMode::Downsample:
        {
            // Bit-crusher style: hold sample for N steps
            int rate = juce::jmax (1, (int)(drive * 31.0f) + 1);
            if (downsampleCounter % rate == 0)
                downsampleHeld[ch] = x;
            downsampleCounter = (ch == 1) ? (downsampleCounter + 1) % rate : downsampleCounter;
            return downsampleHeld[ch];
        }

        case DistortionMode::Linear:
        {
            // Hard clip (linear in passband)
            float driven = x * driveAmt;
            return juce::jlimit (-1.0f, 1.0f, driven);
        }

        case DistortionMode::Rectangular:
        {
            // Square/rectangular wave shaper — zero crossing follower
            float driven = x * driveAmt;
            float thresh = 1.0f - drive * 0.9f;
            if      (driven >  thresh) return  1.0f;
            else if (driven < -thresh) return -1.0f;
            return driven / juce::jmax (thresh, 0.001f);
        }
    }
    return x;
}

void DistortionFx::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!enabled) return;

    updateToneFilter();
    downsampleCounter = 0;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        auto&  flt  = (ch == 0) ? toneFilterL : toneFilterR;

        for (int s = 0; s < numSamples; ++s)
        {
            float dry = data[s];
            float wet = processSample (dry, ch);

            // Apply tone filter to wet signal
            wet = flt.processSample (wet);

            data[s] = dry * (1.0f - mix) + wet * mix;
        }
    }
}

//==============================================================================
// Chorus
//==============================================================================
void ChorusFx::prepare (double sampleRate, int /*blockSize*/)
{
    sr = sampleRate;
    for (int ch = 0; ch < 2; ++ch)
    {
        delayLine[ch].assign (MAX_DELAY_SAMPLES, 0.0f);
        writePos[ch]  = 0;
        lfoPhase[ch]  = (ch == 0) ? 0.0 : 0.25; // 90° offset
        fbSample[ch]  = 0.0f;
    }
}

float ChorusFx::readDelay (int ch, float delaySamples)
{
    int   d0   = (int)delaySamples;
    float frac = delaySamples - (float)d0;
    int   r0   = (writePos[ch] - d0 - 1 + MAX_DELAY_SAMPLES) % MAX_DELAY_SAMPLES;
    int   r1   = (r0 + 1) % MAX_DELAY_SAMPLES;
    return delayLine[ch][(size_t)r0] * (1.0f - frac)
         + delayLine[ch][(size_t)r1] * frac;
}

void ChorusFx::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!enabled) return;

    const float lfoRateHz = 0.1f + rate * 7.9f;
    const double lfoInc   = (double)lfoRateHz / sr;
    const float maxDelayS = (float)(sr * 0.03 * (double)depth); // up to 30ms
    const float baseDelayS= (float)(sr * 0.015);
    const float fbAmt     = feedback * 0.7f;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);

    for (int s = 0; s < numSamples; ++s)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            float dry = buffer.getSample (ch, s);

            // Write with feedback
            delayLine[ch][(size_t)writePos[ch]] = dry + fbSample[ch] * fbAmt;

            // LFO modulation
            float lfo    = (float)std::sin (lfoPhase[ch] * juce::MathConstants<double>::twoPi);
            float delayS = juce::jlimit (1.0f, (float)(MAX_DELAY_SAMPLES - 2),
                                         baseDelayS + lfo * maxDelayS);

            float wet     = readDelay (ch, delayS);
            fbSample[ch]  = wet;

            buffer.setSample (ch, s, dry * (1.0f - mix) + wet * mix);
            writePos[ch]  = (writePos[ch] + 1) % MAX_DELAY_SAMPLES;
            lfoPhase[ch]  = std::fmod (lfoPhase[ch] + lfoInc, 1.0);
        }
    }
}

//==============================================================================
// Phaser
//==============================================================================
float PhaserFx::processAllPass (AllPassStage& st, float x, float coeff)
{
    float y = -coeff * x + st.s;
    st.s    =  coeff * y + x;
    return y;
}

void PhaserFx::prepare (double sampleRate, int /*blockSize*/)
{
    sr = sampleRate;
    for (auto& ch : stages)
        for (auto& st : ch)
            st = { 0.0f, 0.0f };
    lfoPhase = { 0.0, 0.5 };
    fbSample = { 0.0f, 0.0f };
}

void PhaserFx::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!enabled) return;

    const float lfoRateHz = 0.05f + rate * 4.95f; // 0.05 – 5Hz
    const double lfoInc   = (double)lfoRateHz / sr;
    const float fbAmt     = feedback * 0.85f;
    const float minFreq   = 100.0f, maxFreq = 8000.0f;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);

    for (int s = 0; s < numSamples; ++s)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            float dry = buffer.getSample (ch, s);

            // LFO → all-pass coefficient
            float lfo    = (float)(0.5 + 0.5 * std::sin (lfoPhase[ch] * juce::MathConstants<double>::twoPi));
            float freq   = minFreq + lfo * depth * (maxFreq - minFreq);
            float w      = 2.0f * (float)sr * std::tan (juce::MathConstants<float>::pi * freq / (float)sr);
            float coeff  = (w - 2.0f * (float)sr) / (w + 2.0f * (float)sr);

            float sig = dry + fbSample[ch] * fbAmt;
            for (int st = 0; st < NUM_STAGES; ++st)
                sig = processAllPass (stages[(size_t)ch][(size_t)st], sig, coeff);

            fbSample[ch] = sig;

            buffer.setSample (ch, s, dry * (1.0f - mix) + sig * mix);
            lfoPhase[ch] = std::fmod (lfoPhase[ch] + lfoInc, 1.0);
        }
    }
}

//==============================================================================
// Flanger
//==============================================================================
void FlangerFx::prepare (double sampleRate, int /*blockSize*/)
{
    sr = sampleRate;
    for (int ch = 0; ch < 2; ++ch)
    {
        delayLine[ch].assign (MAX_DELAY_SAMPLES, 0.0f);
        writePos[ch]  = 0;
        lfoPhase[ch]  = (ch == 0) ? 0.0 : 0.5;
        fbSample[ch]  = 0.0f;
    }
}

float FlangerFx::readDelay (int ch, float delaySamples)
{
    int   d0   = (int)delaySamples;
    float frac = delaySamples - (float)d0;
    int   r0   = (writePos[ch] - d0 - 1 + MAX_DELAY_SAMPLES) % MAX_DELAY_SAMPLES;
    int   r1   = (r0 + 1) % MAX_DELAY_SAMPLES;
    return delayLine[ch][(size_t)r0] * (1.0f - frac)
         + delayLine[ch][(size_t)r1] * frac;
}

void FlangerFx::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!enabled) return;

    const float lfoRateHz = 0.05f + rate * 4.95f;
    const double lfoInc   = (double)lfoRateHz / sr;
    const float maxDelayS = (float)(sr * 0.008 * (double)depth); // up to 8ms
    const float baseDelay = (float)(sr * 0.004);
    const float fbAmt     = feedback * 0.8f;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);

    for (int s = 0; s < numSamples; ++s)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            float dry = buffer.getSample (ch, s);
            delayLine[ch][(size_t)writePos[ch]] = dry + fbSample[ch] * fbAmt;

            float lfo    = (float)std::sin (lfoPhase[ch] * juce::MathConstants<double>::twoPi);
            float delayS = juce::jlimit (1.0f, (float)(MAX_DELAY_SAMPLES - 2),
                                         baseDelay + lfo * maxDelayS);

            float wet    = readDelay (ch, delayS);
            fbSample[ch] = wet;

            buffer.setSample (ch, s, dry * (1.0f - mix) + wet * mix);
            writePos[ch]  = (writePos[ch] + 1) % MAX_DELAY_SAMPLES;
            lfoPhase[ch]  = std::fmod (lfoPhase[ch] + lfoInc, 1.0);
        }
    }
}

//==============================================================================
// Reverb
//==============================================================================
ReverbFx::ReverbFx()
{
    reverbParams.roomSize   = 0.7f;
    reverbParams.damping    = 0.5f;
    reverbParams.width      = 0.8f;
    reverbParams.wetLevel   = 0.35f;
    reverbParams.dryLevel   = 0.65f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters (reverbParams);
}

void ReverbFx::prepare (double sampleRate, int /*blockSize*/)
{
    sr = sampleRate;
    reverb.reset();
    updateReverbParams();

    for (int ch = 0; ch < 2; ++ch)
    {
        predelayBuf[ch].assign (MAX_PREDELAY, 0.0f);
        predelayWrite[ch] = 0;
    }
}

void ReverbFx::updateReverbParams()
{
    if (mode == ReverbMode::Hall)
    {
        // Hall: large, long decay, wide
        reverbParams.roomSize = 0.5f + size * 0.5f;     // 0.5–1.0
        reverbParams.damping  = damping * 0.7f;          // halls ring bright
        reverbParams.width    = 0.6f + width * 0.4f;
    }
    else  // Plate
    {
        // Plate: dense, shorter, more damped
        reverbParams.roomSize = 0.3f + size * 0.5f;     // 0.3–0.8
        reverbParams.damping  = 0.3f + damping * 0.6f;  // plates are darker
        reverbParams.width    = 0.5f + width * 0.5f;
    }
    reverbParams.wetLevel = mix;
    reverbParams.dryLevel = 1.0f - mix * 0.5f;
    reverb.setParameters (reverbParams);
}

float ReverbFx::readPredelay (int ch, int delaySamples)
{
    int r = (predelayWrite[ch] - delaySamples + MAX_PREDELAY) % MAX_PREDELAY;
    return predelayBuf[ch][(size_t)r];
}

void ReverbFx::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!enabled) return;

    updateReverbParams();

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    int predelaySamples = (int)(predelay * 0.1 * sr); // 0–100ms
    predelaySamples = juce::jlimit (0, MAX_PREDELAY - 1, predelaySamples);

    // Pre-delay pass
    if (predelaySamples > 0)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int s = 0; s < numSamples; ++s)
            {
                predelayBuf[ch][(size_t)predelayWrite[ch]] = data[s];
                data[s] = readPredelay (ch, predelaySamples);
                predelayWrite[ch] = (predelayWrite[ch] + 1) % MAX_PREDELAY;
            }
        }
    }

    // Apply JUCE reverb
    if (numCh == 2)
    {
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);
        reverb.processStereo (L, R, numSamples);
    }
    else if (numCh == 1)
    {
        float* M = buffer.getWritePointer (0);
        reverb.processMono (M, numSamples);
    }
}

//==============================================================================
// FxChain
//==============================================================================
void FxChain::prepare (double sampleRate, int blockSize)
{
    hyperDimension.prepare (sampleRate, blockSize);
    distortion.prepare     (sampleRate, blockSize);
    chorus.prepare         (sampleRate, blockSize);
    phaser.prepare         (sampleRate, blockSize);
    flanger.prepare        (sampleRate, blockSize);
    reverb.prepare         (sampleRate, blockSize);
}

void FxChain::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    hyperDimension.process (buffer, numSamples);
    distortion.process     (buffer, numSamples);
    chorus.process         (buffer, numSamples);
    phaser.process         (buffer, numSamples);
    flanger.process        (buffer, numSamples);
    reverb.process         (buffer, numSamples);
}
