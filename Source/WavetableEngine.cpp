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

    voiceBuf.setSize (2, blockSize, false, true, true);
    layerBuf.setSize (2, blockSize, false, true, true);

    for (auto& layer : oscLayers)
        for (auto& v : layer.voices)
            v.prepare (sr);
}

//==============================================================================
void WavetableEngine::setWavetable (int oscIndex, const std::vector<float>& table)
{
    if (oscIndex < 0 || oscIndex >= NUM_OSC) return;
    const juce::ScopedLock sl (wavetableLock);
    auto& layer = oscLayers[(size_t)oscIndex];
    for (auto& v : layer.voices)
        v.setWavetable (table);
    layer.active = true;
}

void WavetableEngine::setOscVolume (int oscIndex, float volume)
{
    if (oscIndex >= 0 && oscIndex < NUM_OSC)
        oscLayers[(size_t)oscIndex].volume = juce::jlimit (0.0f, 1.0f, volume);
}

void WavetableEngine::setOscPan (int oscIndex, float pan)
{
    if (oscIndex >= 0 && oscIndex < NUM_OSC)
        oscLayers[(size_t)oscIndex].pan = juce::jlimit (-1.0f, 1.0f, pan);
}

void WavetableEngine::setOscActive (int oscIndex, bool active)
{
    if (oscIndex >= 0 && oscIndex < NUM_OSC)
        oscLayers[(size_t)oscIndex].active = active;
}

void WavetableEngine::setOscPitch (int oscIndex, float semitones, float cents)
{
    if (oscIndex >= 0 && oscIndex < NUM_OSC)
    {
        oscLayers[(size_t)oscIndex].pitchSemis  = semitones;
        oscLayers[(size_t)oscIndex].detuneCents = cents;
    }
}

float WavetableEngine::applyPitchOffset (const OscLayer& layer, float midiNote) const
{
    return midiNote + layer.pitchSemis + layer.detuneCents / 100.0f;
}

void WavetableEngine::setADSR (float a, float d, float s, float r)
{
    adsrParams = { a, d, s, r };
    for (auto& layer : oscLayers)
        for (auto& v : layer.voices)
            v.setADSR (adsrParams);
}

void WavetableEngine::setMono (bool mono)
{
    monoMode = mono;
    if (!monoMode) monoHeldNotes.clear();
}

void WavetableEngine::setLegato (bool legato)
{
    legatoMode = legato;
}

void WavetableEngine::setPortamento (float g)
{
    double glideTimeSec = (double)g * 2.0;
    if (glideTimeSec < 0.001)
        portamentoCoeff = 1.0f;
    else
        portamentoCoeff = (float)(1.0 - std::exp (-1.0 / (glideTimeSec * sampleRate)));
}

void WavetableEngine::setFilterParams (float cutoffHz, float resonance)
{
    filter.setCutoffFrequencyHz (cutoffHz);
    filter.setResonance (juce::jlimit (0.0f, 1.0f, (resonance - 0.1f) / 17.9f));
}

void WavetableEngine::setWidth (float width)
{
    widthAmount = juce::jlimit (0.0f, 2.0f, width);
}

//==============================================================================
void WavetableEngine::renderNextBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi,
                                       int startSample, int numSamples)
{
    // Resize scratch buffers BEFORE acquiring the lock — avoids allocating on
    // the audio thread while the wavetable lock is held, which could deadlock.
    if (voiceBuf.getNumSamples() < numSamples)
    {
        voiceBuf.setSize (2, numSamples, false, true, true);
        layerBuf.setSize (2, numSamples, false, true, true);
    }

    const juce::ScopedLock sl (wavetableLock);

    // Process MIDI inside the lock so note-on/off sees a consistent wavetable state.
    for (const auto meta : midi)
    {
        auto msg = meta.getMessage();
        if      (msg.isNoteOn())  handleNoteOn  (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff()) handleNoteOff (msg.getNoteNumber());
    }

    const int safeSamples = numSamples;

    for (auto& layer : oscLayers)
    {
        if (!layer.active) continue;

        layerBuf.clear();

        float panAngle = (layer.pan + 1.0f) * 0.5f * juce::MathConstants<float>::halfPi;
        float panL = std::cos (panAngle);
        float panR = std::sin (panAngle);

        for (auto& v : layer.voices)
        {
            if (!v.isActive()) continue;
            voiceBuf.clear();
            v.renderBlock (voiceBuf, safeSamples);
            layerBuf.addFrom (0, 0, voiceBuf, 0, 0, safeSamples);
            layerBuf.addFrom (1, 0, voiceBuf, 1, 0, safeSamples);
        }

        float volL = layer.volume * panL;
        float volR = layer.volume * panR;
        buffer.addFrom (0, startSample, layerBuf, 0, 0, safeSamples, volL);
        buffer.addFrom (1, startSample, layerBuf, 1, 0, safeSamples, volR);
    }

    applyWidth (buffer, startSample, safeSamples);

    juce::dsp::AudioBlock<float> block (buffer);
    auto subBlock = block.getSubBlock ((size_t)startSample, (size_t)safeSamples);
    juce::dsp::ProcessContextReplacing<float> ctx (subBlock);
    filter.process (ctx);
}

//==============================================================================
void WavetableEngine::applyWidth (juce::AudioBuffer<float>& buf,
                                   int startSample, int numSamples)
{
    if (buf.getNumChannels() < 2) return;

    float w    = widthAmount;
    float mid  = 0.5f * (2.0f - w);
    float side = 0.5f * w;

    float* L = buf.getWritePointer (0, startSample);
    float* R = buf.getWritePointer (1, startSample);

    for (int i = 0; i < numSamples; ++i)
    {
        float m = (L[i] + R[i]) * mid;
        float s = (L[i] - R[i]) * side;
        L[i] = m + s;
        R[i] = m - s;
    }
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
        for (auto& layer : oscLayers)
            for (auto& v : layer.voices)
                if (v.isActive()) { wasActive = true; break; }

        targetMonoNote = (float)note;
        bool shouldGlide = portamentoCoeff < 0.999f && (!legatoMode || wasActive);
        if (!shouldGlide) currentMonoNote = targetMonoNote;
        float coeff = shouldGlide ? portamentoCoeff : 1.0f;
        bool  reset = !(legatoMode && wasActive);

        for (auto& layer : oscLayers)
        {
            if (!layer.active) continue;
            SynthVoice* monoVoice = nullptr;
            for (auto& v : layer.voices)
                if (v.isActive()) { monoVoice = &v; break; }
            if (monoVoice == nullptr) monoVoice = &layer.voices[0];

            float targetWithOffset = applyPitchOffset (layer, targetMonoNote);
            monoVoice->startNote (juce::roundToInt (targetMonoNote),
                                   velocity, targetWithOffset, coeff, reset, reset);
        }
    }
    else
    {
        for (auto& layer : oscLayers)
        {
            if (!layer.active) continue;

            SynthVoice* target    = nullptr;
            int         targetIdx = 0;

            for (size_t i = 0; i < layer.voices.size(); ++i)
            {
                if (!layer.voices[i].isActive())
                {
                    target    = &layer.voices[i];
                    targetIdx = i;
                    break;
                }
            }

            if (target == nullptr)
            {
                int oldest = layer.voiceStamp[0];
                targetIdx  = 0;
                for (int i = 1; i < MAX_VOICES; i++)
                    if (layer.voiceStamp[i] < oldest)
                        { oldest = layer.voiceStamp[i]; targetIdx = i; }
                target = &layer.voices[targetIdx];
            }

            layer.voiceStamp[targetIdx] = ++layer.voiceClock;
            float noteWithOffset = applyPitchOffset (layer, (float)note);
            target->startNote (note, velocity, noteWithOffset, 1.0f, true, true);
        }
    }
}

void WavetableEngine::handleNoteOff (int note)
{
    if (monoMode)
    {
        monoHeldNotes.erase (std::remove (monoHeldNotes.begin(), monoHeldNotes.end(), note),
                             monoHeldNotes.end());

        if (!monoHeldNotes.empty())
        {
            int  nextNote      = monoHeldNotes.back();
            targetMonoNote     = (float)nextNote;
            float coeff = (legatoMode && portamentoCoeff < 0.999f) ? portamentoCoeff : 1.0f;

            for (auto& layer : oscLayers)
            {
                if (!layer.active) continue;
                for (auto& v : layer.voices)
                {
                    if (v.isActive())
                    {
                        float targetWithOffset = applyPitchOffset (layer, targetMonoNote);
                        v.startNote (nextNote, 1.0f, targetWithOffset, coeff, false, false);
                        break;
                    }
                }
            }
            return;
        }

        for (auto& layer : oscLayers)
            layer.voices[0].stopNote();
        return;
    }

    for (auto& layer : oscLayers)
        for (auto& v : layer.voices)
            if (v.getMidiNote() == note && v.isActive())
                v.stopNote();
}

//==============================================================================
float WavetableEngine::getPlaybackPhase() const
{
    for (const auto& layer : oscLayers)
        for (const auto& v : layer.voices)
            if (v.isActive()) return v.getPhase();
    return 0.0f;
}

float WavetableEngine::getADSREnvelopeValue() const
{
    for (const auto& layer : oscLayers)
        for (const auto& v : layer.voices)
            if (v.isActive()) return v.getEnvelopeValue();
    return 0.0f;
}

//==============================================================================
void WavetableEngine::setOscUnison (int oscIndex, int count, float detuneAmount)
{
    if (oscIndex < 0 || oscIndex >= NUM_OSC) return;
    auto& layer = oscLayers[(size_t)oscIndex];
    for (auto& v : layer.voices)
        v.setUnison (count, detuneAmount);
}

void WavetableEngine::setOscPhase (int oscIndex, float phase01)
{
    if (oscIndex < 0 || oscIndex >= NUM_OSC) return;
    auto& layer = oscLayers[(size_t)oscIndex];
    for (auto& v : layer.voices)
        v.setPhaseOffset (phase01);
}
