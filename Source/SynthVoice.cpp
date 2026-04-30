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
void SynthVoice::startNote (int note, float vel, float tgt, float portaC)
{
    midiNote   = note;
    velocity   = vel;
    targetNote = tgt;
    portaCoeff = portaC;

    // If instant (coeff == 1.0), snap current note to target immediately
    if (portaC >= 0.999f)
        currentNote = tgt;

    updatePhaseIncrement();

    phase = 0.0;
    for (int i = 0; i < MAX_UNISON_VOICES; ++i)
        unisonPhase[(size_t) i] = (i == 0) ? 0.0 : std::fmod ((double) i * 0.61803398875, 1.0);

    adsr.reset();
    adsr.noteOn();
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

        float s = 0.0f;

        if (unisonVoices <= 1)
        {
            double p   = phase * (double)wtSize;
            int    idx = (int)p;
            double frc = p - (double)idx;
            int    n0  = idx % wtSize;
            int    n1  = (idx + 1) % wtSize;
            s = (float)((1.0 - frc) * (double)wavetable[(size_t) n0]
                        + frc        * (double)wavetable[(size_t) n1]);
        }
        else
        {
            float sideSum = 0.0f;
            for (int u = 1; u < unisonVoices; ++u)
            {
                auto& uPhase = unisonPhase[(size_t) u];
                double p = uPhase * (double)wtSize;
                int idx = (int)p;
                double frc = p - (double)idx;
                int n0 = idx % wtSize;
                int n1 = (idx + 1) % wtSize;
                sideSum += (float)((1.0 - frc) * (double)wavetable[(size_t) n0]
                                   + frc        * (double)wavetable[(size_t) n1]);

                uPhase += unisonPhaseInc[(size_t) u];
                if (uPhase >= 1.0) uPhase -= 1.0;
            }

            double p = phase * (double)wtSize;
            int idx = (int)p;
            double frc = p - (double)idx;
            int n0 = idx % wtSize;
            int n1 = (idx + 1) % wtSize;
            float centre = (float)((1.0 - frc) * (double)wavetable[(size_t) n0]
                                   + frc        * (double)wavetable[(size_t) n1]);

            auto sideAverage = sideSum / (float) juce::jmax (1, unisonVoices - 1);
            auto centreGain = 1.0f - (unisonBlend * 0.45f);
            auto sideGain = unisonBlend;
            s = (centre * centreGain + sideAverage * sideGain) / (centreGain + sideGain);
        }

        float env = adsr.getNextSample();
        latestEnv = env;
        float out = s * env * velocity;

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

    for (int i = 0; i < MAX_UNISON_VOICES; ++i)
    {
        auto offsetSemitones = getUnisonOffset (i);
        unisonPhaseInc[(size_t) i] = midiNoteToHz (currentNote + offsetSemitones) / sampleRate;
    }
}

float SynthVoice::getUnisonOffset (int index) const
{
    if (index == 0 || unisonVoices <= 1)
        return 0.0f;

    auto sideIndex = index - 1;
    auto sideCount = juce::jmax (1, unisonVoices - 1);
    auto spread = sideCount == 1 ? 0.0f
                                 : ((float) sideIndex / (float) (sideCount - 1)) * 2.0f - 1.0f;

    return spread * unisonDetune * 0.5f;
}
