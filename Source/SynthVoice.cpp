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

void SynthVoice::setUnison (int voices, float detune, float blend)
{
    unisonVoices = juce::jlimit (1, MAX_UNISON_VOICES, voices);
    unisonDetune = juce::jlimit (0.0f, 1.0f, detune);
    unisonBlend  = juce::jlimit (0.0f, 1.0f, blend);
    updatePhaseIncrement();
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

        float outL = 0.0f;
        float outR = 0.0f;

        if (unisonVoices <= 1)
        {
            auto s = readWavetableAtPhase (phase);
            outL = s;
            outR = s;
        }
        else
        {
            float totalGain = 0.0f;

            for (int u = 0; u < unisonVoices; ++u)
            {
                auto voicePhase = (u == 0) ? phase : unisonPhase[(size_t) u];
                auto sample = readWavetableAtPhase (voicePhase);
                auto gain = getUnisonGain (u);
                auto pan = getUnisonPan (u);

                auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                auto leftGain = std::cos (angle);
                auto rightGain = std::sin (angle);

                outL += sample * gain * leftGain;
                outR += sample * gain * rightGain;
                totalGain += gain;

                if (u > 0)
                {
                    auto& uPhase = unisonPhase[(size_t) u];
                    uPhase += unisonPhaseInc[(size_t) u];
                    if (uPhase >= 1.0) uPhase -= 1.0;
                }
            }

            auto normalise = totalGain > 0.0001f ? std::sqrt (2.0f) / totalGain : 1.0f;
            outL *= normalise;
            outR *= normalise;
        }

        float env = adsr.getNextSample();
        latestEnv = env;

        L[i] += outL * env * velocity;
        if (R) R[i] += outR * env * velocity;

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
    phaseInc = midiNoteToHz (currentNote + (unisonVoices > 1 ? getUnisonOffset (0) : 0.0f)) / sampleRate;

    for (int i = 0; i < MAX_UNISON_VOICES; ++i)
    {
        auto offsetSemitones = getUnisonOffset (i);
        unisonPhaseInc[(size_t) i] = midiNoteToHz (currentNote + offsetSemitones) / sampleRate;
    }
}

void SynthVoice::resetOscillatorPhases()
{
    phase = 0.0;
    for (int i = 0; i < MAX_UNISON_VOICES; ++i)
        unisonPhase[(size_t) i] = (i == 0) ? 0.0 : std::fmod ((double) i * 0.61803398875, 1.0);
}

float SynthVoice::getUnisonOffset (int index) const
{
    if (unisonVoices <= 1)
        return 0.0f;

    auto spread = ((float) index / (float) (unisonVoices - 1)) * 2.0f - 1.0f;

    return spread * unisonDetune * 0.75f;
}

float SynthVoice::getUnisonPan (int index) const
{
    if (unisonVoices <= 1)
        return 0.0f;

    auto spread = ((float) index / (float) (unisonVoices - 1)) * 2.0f - 1.0f;
    auto width = 0.35f + unisonBlend * 0.65f;
    return juce::jlimit (-1.0f, 1.0f, spread * width);
}

float SynthVoice::getUnisonGain (int index) const
{
    if (unisonVoices <= 1)
        return 1.0f;

    auto spread = std::abs (((float) index / (float) (unisonVoices - 1)) * 2.0f - 1.0f);
    auto centreBias = 1.0f - spread;
    auto sideGain = 0.18f + unisonBlend * 0.82f;

    return juce::jmap (centreBias, sideGain, 1.0f);
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
