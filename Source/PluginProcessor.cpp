#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
WaveForgeProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ADSR
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "attack",  "Attack",
        juce::NormalisableRange<float> (0.001f, 4.0f, 0.001f, 0.4f), 0.01f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "decay",   "Decay",
        juce::NormalisableRange<float> (0.001f, 4.0f, 0.001f, 0.4f), 0.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "sustain", "Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "release", "Release",
        juce::NormalisableRange<float> (0.001f, 8.0f, 0.001f, 0.4f), 0.3f));

    // Synth
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "portamento", "Portamento",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "mono", "Mono", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "legato", "Legato", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "gain", "Gain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "filterCutoff", "Filter Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 18000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "filterRes", "Filter Resonance",
        juce::NormalisableRange<float> (0.1f, 18.0f, 0.01f), 0.707f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "wtPosition", "WT Position",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.42f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "loopEnd", "Loop End",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.58f));

    return { params.begin(), params.end() };
}

//==============================================================================
WaveForgeProcessor::WaveForgeProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WaveForge", createParameterLayout())
{
    apvts.addParameterListener ("attack",      this);
    apvts.addParameterListener ("decay",       this);
    apvts.addParameterListener ("sustain",     this);
    apvts.addParameterListener ("release",     this);
    apvts.addParameterListener ("portamento",  this);
    apvts.addParameterListener ("mono",        this);
    apvts.addParameterListener ("legato",      this);
    apvts.addParameterListener ("gain",        this);
    apvts.addParameterListener ("filterCutoff",this);
    apvts.addParameterListener ("filterRes",   this);
    apvts.addParameterListener ("wtPosition",  this);
    apvts.addParameterListener ("loopEnd",     this);
}

WaveForgeProcessor::~WaveForgeProcessor()
{
    // Signal the analysis thread to stop and wait for it
    analysisCancel.store (true);
    if (analysisThread && analysisThread->joinable())
        analysisThread->join();
}

//==============================================================================
void WaveForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setADSR (
        *apvts.getRawParameterValue ("attack"),
        *apvts.getRawParameterValue ("decay"),
        *apvts.getRawParameterValue ("sustain"),
        *apvts.getRawParameterValue ("release"));
    engine.setLegato (apvts.getRawParameterValue ("legato")->load() > 0.5f);
}

void WaveForgeProcessor::releaseResources() {}

//==============================================================================
void WaveForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    bool  isMono       = *apvts.getRawParameterValue ("mono")        > 0.5f;
    bool  isLegato     = apvts.getRawParameterValue ("legato")->load() > 0.5f;
    float portamento   = *apvts.getRawParameterValue ("portamento");
    float gain         = *apvts.getRawParameterValue ("gain");
    float filterCutoff = *apvts.getRawParameterValue ("filterCutoff");
    float filterRes    = *apvts.getRawParameterValue ("filterRes");

    engine.setMono         (isMono);
    engine.setLegato       (isLegato);
    engine.setPortamento   (portamento);
    engine.setFilterParams (filterCutoff, filterRes);

    engine.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    buffer.applyGain (gain);
}

//==============================================================================
bool WaveForgeProcessor::loadSampleFile (const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    const int maxSamples = static_cast<int> (
        std::min ((juce::int64)reader->lengthInSamples,
                  (juce::int64)(reader->sampleRate * 30.0)));   // cap at 30 s

    sampleBuffer.setSize ((int)reader->numChannels, maxSamples);
    reader->read (&sampleBuffer, 0, maxSamples, 0, true, true);
    sampleFileSampleRate = reader->sampleRate;

    sampleLoaded.store (true);
    setWavetablePosition (0.42f);
    setLoopEnd (0.58f);

    juce::MessageManager::callAsync ([this] {
        listeners.call ([] (Listener& l) { l.sampleLoaded(); });
    });

    return true;
}

//==============================================================================
void WaveForgeProcessor::setWavetablePosition (float pos)
{
    pos = juce::jlimit (0.0f, 1.0f, pos);
    auto minWidth = 0.005f;
    if (pos > loopEndPosition.load() - minWidth)
        loopEndPosition.store (juce::jlimit (0.0f, 1.0f, pos + minWidth));

    wavetablePosition.store (pos);
    rebuildWavetableFromPosition (pos);

    // Sync the APVTS knob back to the new position — must be on message thread
    juce::MessageManager::callAsync ([this, pos] {
        if (auto* p = apvts.getParameter ("wtPosition"))
            p->setValueNotifyingHost (pos);
    });
}

void WaveForgeProcessor::setLoopEnd (float pos)
{
    auto minWidth = 0.005f;
    pos = juce::jlimit (wavetablePosition.load() + minWidth, 1.0f, pos);
    loopEndPosition.store (pos);
    rebuildWavetableFromPosition (wavetablePosition.load());

    juce::MessageManager::callAsync ([this, pos] {
        if (auto* p = apvts.getParameter ("loopEnd"))
            p->setValueNotifyingHost (pos);
    });
}

void WaveForgeProcessor::rebuildWavetableFromPosition (float pos)
{
    if (!sampleLoaded.load()) return;

    int totalSamples       = sampleBuffer.getNumSamples();
    if (totalSamples <= 0) return;

    auto endPos            = juce::jmax (pos + 0.005f, loopEndPosition.load());
    int startIdx           = juce::jlimit (0, totalSamples - 1, (int)std::floor (pos * (float)(totalSamples - 1)));
    int endIdx             = juce::jlimit (startIdx + 16, totalSamples, (int)std::ceil (endPos * (float)totalSamples));
    int wtSize             = endIdx - startIdx;
    int numCh              = sampleBuffer.getNumChannels();

    std::vector<float> cycle (wtSize);
    for (int i = 0; i < wtSize; i++)
    {
        float s = 0.0f;
        for (int ch = 0; ch < numCh; ch++)
            s += sampleBuffer.getSample (ch, startIdx + i);
        cycle[i] = s / (float)numCh;
    }

    auto crossfade = juce::jlimit (8, wtSize / 2, wtSize / 32);
    for (int i = 0; i < crossfade; ++i)
    {
        auto t = (float)i / (float)juce::jmax (1, crossfade - 1);
        auto head = cycle[(size_t)i];
        auto tail = cycle[(size_t)(wtSize - crossfade + i)];
        auto blended = tail * (1.0f - t) + head * t;
        cycle[(size_t)i] = blended;
        cycle[(size_t)(wtSize - crossfade + i)] = blended;
    }

    float dc = 0.0f;
    for (float s : cycle) dc += s;
    dc /= (float)cycle.size();
    for (float& s : cycle) s -= dc;

    float peak = 0.0f;
    for (float s : cycle) peak = std::max (peak, std::abs (s));
    if (peak > 0.0001f)
        for (float& s : cycle) s /= peak;

    engine.setWavetable (cycle);
}

//==============================================================================
void WaveForgeProcessor::startAnalysis()
{
    if (!sampleLoaded.load()) return;
    if (analyzing.load())     return;

    // Join any previous thread before spawning a new one
    if (analysisThread && analysisThread->joinable())
        analysisThread->join();

    analyzing.store (true);
    analysisCancel.store (false);

    juce::MessageManager::callAsync ([this] {
        listeners.call ([] (Listener& l) { l.analysisStarted(); });
    });

    analysisThread = std::make_unique<std::thread> ([this]
    {
        float bestPos = analyzer.findBestWavetablePosition (
            sampleBuffer, (float)sampleFileSampleRate, analysisCancel);

        if (!analysisCancel.load())
        {
            onAnalysisComplete (bestPos);
        }
        else
        {
            analyzing.store (false);
            juce::MessageManager::callAsync ([this] {
                listeners.call ([] (Listener& l) { l.analysisCancelled(); });
            });
        }
    });
    // Thread is joinable — destructor will join it safely
}

void WaveForgeProcessor::cancelAnalysis()
{
    analysisCancel.store (true);
}

void WaveForgeProcessor::onAnalysisComplete (float bestPosition)
{
    analyzing.store (false);
    // setWavetablePosition posts the APVTS sync to the message thread internally
    setWavetablePosition (bestPosition);

    juce::MessageManager::callAsync ([this] {
        listeners.call ([] (Listener& l) { l.analysisFinished(); });
    });
}

//==============================================================================
void WaveForgeProcessor::parameterChanged (const juce::String& paramID, float newValue)
{
    if (paramID == "attack"  || paramID == "decay" ||
        paramID == "sustain" || paramID == "release")
    {
        engine.setADSR (
            *apvts.getRawParameterValue ("attack"),
            *apvts.getRawParameterValue ("decay"),
            *apvts.getRawParameterValue ("sustain"),
            *apvts.getRawParameterValue ("release"));
    }
    else if (paramID == "wtPosition")
    {
        // Only rebuild if the change came from the UI (not from setWavetablePosition)
        float current = wavetablePosition.load();
        if (std::abs (newValue - current) > 0.0005f)
        {
            wavetablePosition.store (newValue);
            rebuildWavetableFromPosition (newValue);
        }
    }
    else if (paramID == "loopEnd")
    {
        auto minWidth = 0.005f;
        loopEndPosition.store (juce::jlimit (wavetablePosition.load() + minWidth, 1.0f, newValue));
        rebuildWavetableFromPosition (wavetablePosition.load());
    }
}

//==============================================================================
juce::AudioProcessorEditor* WaveForgeProcessor::createEditor()
{
    return new WaveForgeEditor (*this);
}

void WaveForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WaveForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WaveForgeProcessor();
}
