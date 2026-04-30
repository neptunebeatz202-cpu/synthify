#include "WavetableEngine.h"

WavetableEngine::WavetableEngine()
{
    adsrParams = { 0.01f, 0.1f, 0.7f, 0.3f };
}

void WavetableEngine::prepare (double sr, int blockSize)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32)blockSize;
    spec.numChannels      = 2;

    filter.prepare (spec);
    filter.setMode (juce::dsp::LadderFilterMode::LPF24);
    filter.setCutoffFrequencyHz (18000.0f);
    filter.setResonance (0.1f);

    // Pre-allocate voice mix buffer — NO heap alloc on audio thread
    voiceBuf.setSize (2, blockSize, false, true, true);

    for (auto& v : voices)
        v.prepare (sr);
}

void WavetableEngine::setWavetable (const std::vector<float>& table)
{
    for (auto& v : voices)
        v.setWavetable (table);
}

void WavetableEngine::setADSR (float a, float d, float s, float r)
{
    adsrParams = { a, d, s, r };
    for (auto& v : voices)
        v.setADSR (adsrParams);
}

void WavetableEngine::setMono (bool mono)
{
    monoMode = mono;
    if (! monoMode)
        monoHeldNotes.clear();
}

void WavetableEngine::setLegato (bool legato)
{
    legatoMode = legato;
}

void WavetableEngine::setPortamento (float g)
{
    // g == 0 → instant (coeff = 1.0), g == 1 → ~2 s glide
    double glideTimeSec = (double)g * 2.0;
    if (glideTimeSec < 0.001)
        portamentoCoeff = 1.0f;
    else
        portamentoCoeff = (float)(1.0 - std::exp (-1.0 / (glideTimeSec * sampleRate)));
}

void WavetableEngine::setFilterParams (float cutoffHz, float resonance)
{
    filter.setCutoffFrequencyHz (cutoffHz);
    // Map 0.1–18 → 0–1 for JUCE LadderFilter
    filter.setResonance (juce::jlimit (0.0f, 1.0f, (resonance - 0.1f) / 17.9f));
}

//==============================================================================
void WavetableEngine::renderNextBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi,
                                       int startSample, int numSamples)
{
    // Process MIDI events
    for (const auto meta : midi)
    {
        auto msg = meta.getMessage();
        if      (msg.isNoteOn())  handleNoteOn  (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff()) handleNoteOff (msg.getNoteNumber());
    }

    // Render each active voice into the pre-allocated temp buffer
    // then accumulate into the output buffer
    for (auto& v : voices)
    {
        if (!v.isActive()) continue;

        voiceBuf.clear();
        v.renderBlock (voiceBuf, numSamples);

        for (int ch = 0; ch < buffer.getNumChannels(); ch++)
            buffer.addFrom (ch, startSample, voiceBuf, ch, 0, numSamples);
    }

    // Apply ladder filter
    juce::dsp::AudioBlock<float> block (buffer);
    auto subBlock = block.getSubBlock ((size_t)startSample, (size_t)numSamples);
    juce::dsp::ProcessContextReplacing<float> ctx (subBlock);
    filter.process (ctx);
}

//==============================================================================
void WavetableEngine::handleNoteOn (int note, float velocity)
{
    if (monoMode)
    {
        monoHeldNotes.erase (std::remove (monoHeldNotes.begin(), monoHeldNotes.end(), note),
                             monoHeldNotes.end());
        monoHeldNotes.push_back (note);

        bool wasActive = false;
        SynthVoice* monoVoice = nullptr;
        for (auto& v : voices)
        {
            if (v.isActive())
            {
                monoVoice = &v;
                wasActive = true;
                break;
            }
        }

        if (monoVoice == nullptr) monoVoice = &voices[0];

        targetMonoNote = (float)note;
        auto shouldGlide = portamentoCoeff < 0.999f && (! legatoMode || wasActive);
        if (! shouldGlide)
            currentMonoNote = targetMonoNote;

        monoVoice->startNote ((int)juce::roundToInt (targetMonoNote),
                               velocity, targetMonoNote,
                               shouldGlide ? portamentoCoeff : 1.0f,
                               ! (legatoMode && wasActive),
                               ! (legatoMode && wasActive));
    }
    else
    {
        // Find a free voice
        SynthVoice* target = nullptr;
        int         targetIdx = 0;
        for (int i = 0; i < MAX_VOICES; i++)
        {
            if (!voices[i].isActive())
            {
                target    = &voices[i];
                targetIdx = i;
                break;
            }
        }

        // Steal the oldest voice (lowest stamp)
        if (target == nullptr)
        {
            int oldestStamp = voiceStamp[0];
            targetIdx       = 0;
            for (int i = 1; i < MAX_VOICES; i++)
            {
                if (voiceStamp[i] < oldestStamp)
                {
                    oldestStamp = voiceStamp[i];
                    targetIdx   = i;
                }
            }
            target = &voices[targetIdx];
        }

        voiceStamp[targetIdx] = ++voiceClock;
        target->startNote (note, velocity, (float)note, 1.0f, true, true);
    }
}

void WavetableEngine::handleNoteOff (int note)
{
    if (monoMode)
    {
        monoHeldNotes.erase (std::remove (monoHeldNotes.begin(), monoHeldNotes.end(), note),
                             monoHeldNotes.end());

        if (! monoHeldNotes.empty())
        {
            auto nextNote = monoHeldNotes.back();
            targetMonoNote = (float) nextNote;

            for (auto& v : voices)
            {
                if (v.isActive())
                {
                    v.startNote (nextNote, 1.0f, targetMonoNote,
                                 (legatoMode && portamentoCoeff < 0.999f) ? portamentoCoeff : 1.0f,
                                 false, false);
                    return;
                }
            }
        }

        voices[0].stopNote();
        return;
    }

    for (auto& v : voices)
        if (v.getMidiNote() == note && v.isActive())
            v.stopNote();
}

//==============================================================================
float WavetableEngine::getPlaybackPhase() const
{
    for (const auto& v : voices)
        if (v.isActive()) return v.getPhase();
    return 0.0f;
}

float WavetableEngine::getADSREnvelopeValue() const
{
    for (const auto& v : voices)
        if (v.isActive()) return v.getEnvelopeValue();
    return 0.0f;
}
