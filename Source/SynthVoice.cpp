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
            wavetable[i] = std::sin (juce::MathConstants<float>::twoPi
                                     * (float)i / 2048.0f);
        wtSize = 2048;
    }
}

void SynthVoice::setWavetable (const std::vector<float>& table)
{
    wavetable = table;
    wtSize    = (int)table.size();
}

void SynthVoice::setADSR (const juce::ADSR::Parameters& params)
{
    adsr.setParameters (params);
}

//==============================================================================
void SynthVoice::startNote (int note, float vel, float tgt,
                            float portaC, bool resetEnvelope, bool resetPhase)
{
    midiNote   = note;
    velocity   = vel;
    targetNote = tgt;
    portaCoeff = portaC;

    // If instant (coeff == 1.0), snap current note to target immediately
    if (portaC >= 0.999f)
        currentNote = tgt;

    updatePhaseIncrement();

    if (resetPhase)
        resetOscillatorPhases();

    if (resetEnvelope)
    {
        adsr.reset();
        adsr.noteOn();
    }
    else if (! active)
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
void SynthVoice::renderBlock (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!active) return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    float latestEnv = 0.0f;

    for (int i = 0; i < numSamples; i++)
    {
        // Portamento: one-pole glide applied per-sample
        if (std::abs (currentNote - targetNote) > 0.0005f)
        {
            currentNote = targetNote + (currentNote - targetNote) * (1.0f - portaCoeff);
            updatePhaseIncrement();
        }

        auto s = readWavetableAtPhase (phase);

        float env = adsr.getNextSample();
        latestEnv = env;

        auto out = s * env * velocity;
        L[i] += out;
        if (R) R[i] += out;

        phase += phaseInc;
        if (phase >= 1.0) phase -= 1.0;

        if (!adsr.isActive())
        {
            active = false;
            break;
        }
    }

    lastEnvValue.store (latestEnv, std::memory_order_relaxed);
}

//==============================================================================
void SynthVoice::updatePhaseIncrement()
{
    phaseInc = midiNoteToHz (currentNote) / sampleRate;
}

void SynthVoice::resetOscillatorPhases()
{
    phase = 0.0;
}

float SynthVoice::readWavetableAtPhase (double ph) const
{
    double p = ph * (double)wtSize;
    int idx = (int)p;
    double frc = p - (double)idx;
    int n0 = idx % wtSize;
    int n1 = (idx + 1) % wtSize;

    return (float)((1.0 - frc) * (double)wavetable[(size_t) n0]
                  + frc        * (double)wavetable[(size_t) n1]);
}
