#include "SynthVoice.h"

static inline double midiNoteToHz (float note)
{
    return 440.0 * std::pow (2.0, (note - 69.0) / 12.0);
}

//==============================================================================
void SynthVoice::prepare (double sr)
{
    sampleRate = sr;
    adsr.setSampleRate (sr);

    if (wavetable.empty())
    {
        wavetable.resize (2048);
        for (int i = 0; i < 2048; i++)
            wavetable[(size_t)i] = std::sin (juce::MathConstants<float>::twoPi
                                             * (float)i / 2048.0f);
        wtSize = 2048;
    }

    updateUnisonLayout();
}

void SynthVoice::setWavetable (const std::vector<float>& table)
{
    wavetable = table;
    wtSize    = (int)table.size();
    if (wtSize < 2) wtSize = 2;
}

void SynthVoice::setADSR (const juce::ADSR::Parameters& params)
{
    adsr.setParameters (params);
}

void SynthVoice::setUnison (int count, float detuneAmount)
{
    int newCount = juce::jlimit (1, MAX_UNISON, count);
    float newDet = juce::jlimit (0.0f, 1.0f, detuneAmount);
    if (newCount != unisonCount || std::abs (newDet - unisonDetune) > 0.0001f)
    {
        unisonCount  = newCount;
        unisonDetune = newDet;
        updateUnisonLayout();
        updatePhaseIncrements();
    }
}

void SynthVoice::setPhaseOffset (float phase01)
{
    phaseOffset01 = juce::jlimit (0.0f, 1.0f, phase01);
}

void SynthVoice::updateUnisonLayout()
{
    // Distribute detune symmetrically around 0, max ±50 cents at detune=1
    // Pan: alternate L/R, widening with voice count (Serum-like)
    const float maxCents = 50.0f * unisonDetune;

    for (int i = 0; i < MAX_UNISON; ++i)
    {
        if (i >= unisonCount)
        {
            detuneSemi[(size_t)i] = 0.0f;
            panL[(size_t)i]       = 0.0f;
            panR[(size_t)i]       = 0.0f;
            continue;
        }

        if (unisonCount == 1)
        {
            detuneSemi[(size_t)i] = 0.0f;
            panL[(size_t)i]       = 0.7071f;
            panR[(size_t)i]       = 0.7071f;
        }
        else
        {
            // -1 .. +1 across voices
            float t = (float)i / (float)(unisonCount - 1) * 2.0f - 1.0f;
            detuneSemi[(size_t)i] = (t * maxCents) / 100.0f; // semitones
            // pan equal-power, full spread at unison>=2
            float panAngle = (t * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
            panL[(size_t)i] = std::cos (panAngle);
            panR[(size_t)i] = std::sin (panAngle);
        }
    }
}

//==============================================================================
void SynthVoice::startNote (int note, float vel, float tgt,
                             float portaC, bool resetEnvelope, bool resetPhase)
{
    midiNote   = note;
    velocity   = vel;
    targetNote = tgt;
    portaCoeff = portaC;

    if (portaC >= 0.999f)
        currentNote = tgt;

    updatePhaseIncrements();

    if (resetPhase)
    {
        // Spread starting phases across unison voices for thicker sound,
        // anchored at user phase offset.
        for (int i = 0; i < MAX_UNISON; ++i)
        {
            double base = (double)phaseOffset01;
            double spread = (unisonCount > 1)
                              ? (double)i / (double)unisonCount
                              : 0.0;
            phases[(size_t)i] = std::fmod (base + spread, 1.0);
        }
    }

    if (resetEnvelope)
    {
        adsr.reset();
        adsr.noteOn();
    }
    else if (!active)
    {
        adsr.noteOn();
    }

    active = true;
}

void SynthVoice::stopNote()
{
    adsr.noteOff();
}

//==============================================================================
void SynthVoice::updatePhaseIncrements()
{
    for (int i = 0; i < MAX_UNISON; ++i)
    {
        float n = currentNote + detuneSemi[(size_t)i];
        phaseIncs[(size_t)i] = midiNoteToHz (n) / sampleRate;
    }
}

void SynthVoice::renderBlock (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!active) return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    float latestEnv = 0.0f;
    const float invCount = 1.0f / std::sqrt ((float)unisonCount);

    for (int i = 0; i < numSamples; ++i)
    {
        // Glide
        if (std::abs (currentNote - targetNote) > 0.0005f)
        {
            currentNote = targetNote + (currentNote - targetNote) * (1.0f - portaCoeff);
            updatePhaseIncrements();
        }

        float sumL = 0.0f, sumR = 0.0f;

        for (int u = 0; u < unisonCount; ++u)
        {
            float s = readWavetableAtPhase (phases[(size_t)u]);
            sumL += s * panL[(size_t)u];
            sumR += s * panR[(size_t)u];

            phases[(size_t)u] += phaseIncs[(size_t)u];
            if (phases[(size_t)u] >= 1.0) phases[(size_t)u] -= 1.0;
        }

        float env = adsr.getNextSample();
        latestEnv = env;
        float gain = env * velocity * invCount;

        L[i] += sumL * gain;
        if (R) R[i] += sumR * gain;

        if (!adsr.isActive())
        {
            active = false;
            break;   // Remaining samples are already zeroed (buffer was cleared before render)
        }
    }

    lastEnvValue.store (latestEnv, std::memory_order_relaxed);
}

//==============================================================================
float SynthVoice::readWavetableAtPhase (double ph) const
{
    double p   = ph * (double)wtSize;
    int    idx = (int)p;
    double frc = p - (double)idx;
    int    n0  = idx % wtSize;
    if (n0 < 0) n0 += wtSize;
    int    n1  = (n0 + 1) % wtSize;
    return (float)((1.0 - frc) * (double)wavetable[(size_t)n0]
                   + frc       * (double)wavetable[(size_t)n1]);
}
