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
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

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
    apvts.addParameterListener ("gain",        this);
    apvts.addParameterListener ("filterCutoff",this);
    apvts.addParameterListener ("filterRes",   this);
    apvts.addParameterListener ("wtPosition",  this);
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
}

void WaveForgeProcessor::releaseResources() {}

//==============================================================================
void WaveForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    bool  isMono       = *apvts.getRawParameterValue ("mono")        > 0.5f;
    float portamento   = *apvts.getRawParameterValue ("portamento");
    float gain         = *apvts.getRawParameterValue ("gain");
    float filterCutoff = *apvts.getRawParameterValue ("filterCutoff");
    float filterRes    = *apvts.getRawParameterValue ("filterRes");

    engine.setMono         (isMono);
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
    setWavetablePosition (0.5f);   // default to centre

    juce::MessageManager::callAsync ([this] {
        listeners.call ([] (Listener& l) { l.sampleLoaded(); });
    });

    return true;
}

//==============================================================================
void WaveForgeProcessor::setWavetablePosition (float pos)
{
    pos = juce::jlimit (0.0f, 1.0f, pos);
    wavetablePosition.store (pos);
    rebuildWavetableFromPosition (pos);

    // Sync the APVTS knob back to the new position — must be on message thread
    juce::MessageManager::callAsync ([this, pos] {
        if (auto* p = apvts.getParameter ("wtPosition"))
            p->setValueNotifyingHost (pos);
    });
}

void WaveForgeProcessor::rebuildWavetableFromPosition (float pos)
{
    if (!sampleLoaded.load()) return;

    constexpr int wtSize   = 2048;
    int totalSamples       = sampleBuffer.getNumSamples();
    int centreIdx          = static_cast<int> (pos * (float)(totalSamples - 1));
    int startIdx           = juce::jlimit (0, totalSamples - wtSize, centreIdx - wtSize / 2);
    int numCh              = sampleBuffer.getNumChannels();

    // Build mono slice
    std::vector<float> cycle (wtSize);
    for (int i = 0; i < wtSize; i++)
    {
        float s = 0.0f;
        for (int ch = 0; ch < numCh; ch++)
            s += sampleBuffer.getSample (ch, startIdx + i);
        cycle[i] = s / (float)numCh;
    }

    // Pitch-detect and resample so one cycle = C4 at the file's sample rate
    float detectedFreq = analyzer.detectPitch (cycle.data(), wtSize,
                                               (float)sampleFileSampleRate);

    if (detectedFreq > 20.0f)
    {
        float sourcePeriod  = (float)sampleFileSampleRate / detectedFreq;
        float targetPeriod  = (float)sampleFileSampleRate / 261.63f;   // C4
        float resampleRatio = sourcePeriod / targetPeriod;

        std::vector<float> resampled (wtSize);
        for (int i = 0; i < wtSize; i++)
        {
            float srcIdx = (float)i * resampleRatio;
            int   s0     = (int)srcIdx % wtSize;
            int   s1     = (s0 + 1) % wtSize;
            float frac   = srcIdx - std::floor (srcIdx);
            resampled[i] = cycle[s0] + frac * (cycle[s1] - cycle[s0]);
        }
        cycle = resampled;
    }

    // Normalise to peak 1.0
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
