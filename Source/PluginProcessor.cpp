#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
SynthifyProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── ADSR ──────────────────────────────────────────────────────────────────
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

    // ── Global synth ──────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "portamento", "Portamento",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "mono",   "Mono",   false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "legato", "Legato", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "gain",  "Gain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "width", "Width",
        juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "filterCutoff", "Filter Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 18000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "filterRes", "Filter Resonance",
        juce::NormalisableRange<float> (0.1f, 18.0f, 0.01f), 0.707f));

    // ── Per-oscillator params ──────────────────────────────────────────────────
    for (int i = 0; i < 3; i++)
    {
        juce::String idx (i);
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "wtPosition" + idx, "WT Position " + idx,
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "oscVol" + idx, "Osc " + idx + " Volume",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "oscPan" + idx, "Osc " + idx + " Pan",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "oscPitch" + idx, "Osc " + idx + " Pitch (semitones)",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "oscDetune" + idx, "Osc " + idx + " Detune (cents)",
            juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "oscPhase" + idx, "Osc " + idx + " Phase",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            "oscOn" + idx, "Osc " + idx + " On", true));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "oscUnisonCount" + idx, "Osc " + idx + " Unison Voices",
            juce::NormalisableRange<float> (1.0f, 8.0f, 1.0f), 1.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "oscUnisonDetune" + idx, "Osc " + idx + " Unison Detune",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    }

    // ── FX: HyperDimension ─────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("fxHyperOn",     "HyperDim On",    false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxHyperAmount", "HyperDim Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxHyperDetune", "HyperDim Detune",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxHyperMix",    "HyperDim Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));

    // ── FX: Distortion ────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("fxDistOn",   "Dist On",    false));
    params.push_back (std::make_unique<juce::AudioParameterInt>   ("fxDistMode", "Dist Mode",  0, 4, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxDistDrive","Dist Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxDistTone", "Dist Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxDistMix",  "Dist Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    // ── FX: Chorus ─────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("fxChorusOn",    "Chorus On",       false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxChorusRate",  "Chorus Rate",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxChorusDepth", "Chorus Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxChorusFb",    "Chorus Feedback",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxChorusMix",   "Chorus Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    // ── FX: Phaser ─────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("fxPhaserOn",    "Phaser On",       false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxPhaserRate",  "Phaser Rate",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxPhaserDepth", "Phaser Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxPhaserFb",    "Phaser Feedback",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxPhaserMix",   "Phaser Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    // ── FX: Flanger ────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("fxFlangerOn",    "Flanger On",      false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxFlangerRate",  "Flanger Rate",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxFlangerDepth", "Flanger Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxFlangerFb",    "Flanger Feedback",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxFlangerMix",   "Flanger Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    // ── FX: Reverb ─────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("fxReverbOn",       "Reverb On",        false));
    params.push_back (std::make_unique<juce::AudioParameterInt>   ("fxReverbMode",     "Reverb Mode",      0, 1, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxReverbSize",     "Reverb Size",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxReverbDamp",     "Reverb Damping",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxReverbWidth",    "Reverb Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxReverbMix",      "Reverb Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fxReverbPredelay", "Reverb Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
SynthifyProcessor::SynthifyProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Synthify", createParameterLayout())
{
    // ADSR + global
    for (auto id : { "attack", "decay", "sustain", "release",
                     "portamento", "mono", "legato", "width",
                     "filterCutoff", "filterRes", "gain" })
        apvts.addParameterListener (id, this);

    // Per-osc
    for (int i = 0; i < 3; i++)
    {
        juce::String idx (i);
        for (auto pfx : { "wtPosition", "oscVol", "oscPan", "oscPitch",
                          "oscDetune", "oscPhase", "oscOn",
                          "oscUnisonCount", "oscUnisonDetune" })
            apvts.addParameterListener (pfx + idx, this);
    }

    // FX
    for (auto id : { "fxHyperOn", "fxHyperAmount", "fxHyperDetune", "fxHyperMix",
                     "fxDistOn", "fxDistMode", "fxDistDrive", "fxDistTone", "fxDistMix",
                     "fxChorusOn", "fxChorusRate", "fxChorusDepth", "fxChorusFb", "fxChorusMix",
                     "fxPhaserOn", "fxPhaserRate", "fxPhaserDepth", "fxPhaserFb", "fxPhaserMix",
                     "fxFlangerOn", "fxFlangerRate", "fxFlangerDepth", "fxFlangerFb", "fxFlangerMix",
                     "fxReverbOn", "fxReverbMode", "fxReverbSize", "fxReverbDamp",
                     "fxReverbWidth", "fxReverbMix", "fxReverbPredelay" })
        apvts.addParameterListener (id, this);
}

SynthifyProcessor::~SynthifyProcessor()
{
    // Remove all parameter listeners FIRST — prevents parameterChanged() callbacks
    // firing while we are in the middle of teardown.
    apvts.removeParameterListener ("attack",  this);
    apvts.removeParameterListener ("decay",   this);
    apvts.removeParameterListener ("sustain", this);
    apvts.removeParameterListener ("release", this);
    apvts.removeParameterListener ("portamento", this);
    apvts.removeParameterListener ("mono",    this);
    apvts.removeParameterListener ("legato",  this);
    apvts.removeParameterListener ("width",   this);
    apvts.removeParameterListener ("filterCutoff", this);
    apvts.removeParameterListener ("filterRes",    this);
    apvts.removeParameterListener ("gain",    this);

    for (int i = 0; i < 3; i++)
    {
        juce::String idx (i);
        for (auto pfx : { "wtPosition", "oscVol", "oscPan", "oscPitch",
                          "oscDetune", "oscPhase", "oscOn",
                          "oscUnisonCount", "oscUnisonDetune" })
            apvts.removeParameterListener (pfx + idx, this);
    }

    for (auto id : { "fxHyperOn", "fxHyperAmount", "fxHyperDetune", "fxHyperMix",
                     "fxDistOn", "fxDistMode", "fxDistDrive", "fxDistTone", "fxDistMix",
                     "fxChorusOn", "fxChorusRate", "fxChorusDepth", "fxChorusFb", "fxChorusMix",
                     "fxPhaserOn", "fxPhaserRate", "fxPhaserDepth", "fxPhaserFb", "fxPhaserMix",
                     "fxFlangerOn", "fxFlangerRate", "fxFlangerDepth", "fxFlangerFb", "fxFlangerMix",
                     "fxReverbOn", "fxReverbMode", "fxReverbSize", "fxReverbDamp",
                     "fxReverbWidth", "fxReverbMix", "fxReverbPredelay" })
        apvts.removeParameterListener (id, this);

    // Now stop and join all analysis threads.
    for (auto& state : oscStates)
    {
        state.analysisCancel.store (true);
        if (state.analysisThread && state.analysisThread->joinable())
            state.analysisThread->join();
    }
}

//==============================================================================
void SynthifyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    fxChain.prepare (sampleRate, samplesPerBlock);

    engine.setADSR (
        *apvts.getRawParameterValue ("attack"),
        *apvts.getRawParameterValue ("decay"),
        *apvts.getRawParameterValue ("sustain"),
        *apvts.getRawParameterValue ("release"));

    engine.setLegato (*apvts.getRawParameterValue ("legato") > 0.5f);
    engine.setWidth  (*apvts.getRawParameterValue ("width"));

    for (int i = 0; i < 3; i++)
    {
        juce::String idx (i);
        engine.setOscVolume (i, *apvts.getRawParameterValue ("oscVol"  + idx));
        engine.setOscPan    (i, *apvts.getRawParameterValue ("oscPan"  + idx));
        engine.setOscPitch  (i, *apvts.getRawParameterValue ("oscPitch"  + idx),
                                *apvts.getRawParameterValue ("oscDetune" + idx));
        engine.setOscUnison (i, (int)*apvts.getRawParameterValue ("oscUnisonCount"  + idx),
                                     *apvts.getRawParameterValue ("oscUnisonDetune" + idx));
        engine.setOscPhase  (i, *apvts.getRawParameterValue ("oscPhase" + idx));
        engine.setOscActive (i, *apvts.getRawParameterValue ("oscOn"    + idx) > 0.5f);
    }

    applyFxParams();
}

void SynthifyProcessor::releaseResources() {}

//==============================================================================
void SynthifyProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Guard: only process if we have a valid buffer.
    if (buffer.getNumSamples() == 0)
        return;

    // Clear any extra channels the host may have given us (AU can pass sidechain inputs).
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Bail if we don't have at least stereo output.
    if (buffer.getNumChannels() < 2)
    {
        buffer.clear();
        return;
    }

    buffer.clear();

    bool  isMono   = *apvts.getRawParameterValue ("mono")   > 0.5f;
    bool  isLegato = *apvts.getRawParameterValue ("legato") > 0.5f;
    float porta    = *apvts.getRawParameterValue ("portamento");
    float gain     = *apvts.getRawParameterValue ("gain");
    float cutoff   = *apvts.getRawParameterValue ("filterCutoff");
    float res      = *apvts.getRawParameterValue ("filterRes");
    float width    = *apvts.getRawParameterValue ("width");

    engine.setMono         (isMono);
    engine.setLegato       (isLegato);
    engine.setPortamento   (porta);
    engine.setFilterParams (cutoff, res);
    engine.setWidth        (width);

    for (int i = 0; i < 3; i++)
    {
        juce::String idx (i);
        engine.setOscVolume (i, *apvts.getRawParameterValue ("oscVol"  + idx));
        engine.setOscPan    (i, *apvts.getRawParameterValue ("oscPan"  + idx));
        engine.setOscPitch  (i, *apvts.getRawParameterValue ("oscPitch"  + idx),
                                *apvts.getRawParameterValue ("oscDetune" + idx));
        engine.setOscUnison (i, (int)*apvts.getRawParameterValue ("oscUnisonCount"  + idx),
                                     *apvts.getRawParameterValue ("oscUnisonDetune" + idx));
        engine.setOscPhase  (i, *apvts.getRawParameterValue ("oscPhase" + idx));
    }

    engine.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    buffer.applyGain (gain);

    fxChain.process (buffer, buffer.getNumSamples());
}

//==============================================================================
void SynthifyProcessor::applyFxParams()
{
    auto& hd = fxChain.hyperDimension;
    hd.enabled = *apvts.getRawParameterValue ("fxHyperOn") > 0.5f;
    hd.amount  = *apvts.getRawParameterValue ("fxHyperAmount");
    hd.detune  = *apvts.getRawParameterValue ("fxHyperDetune");
    hd.mix     = *apvts.getRawParameterValue ("fxHyperMix");

    auto& dist = fxChain.distortion;
    dist.enabled = *apvts.getRawParameterValue ("fxDistOn") > 0.5f;
    dist.mode    = static_cast<DistortionMode> (
        (int)*apvts.getRawParameterValue ("fxDistMode"));
    dist.drive   = *apvts.getRawParameterValue ("fxDistDrive");
    dist.tone    = *apvts.getRawParameterValue ("fxDistTone");
    dist.mix     = *apvts.getRawParameterValue ("fxDistMix");

    auto& ch = fxChain.chorus;
    ch.enabled  = *apvts.getRawParameterValue ("fxChorusOn") > 0.5f;
    ch.rate     = *apvts.getRawParameterValue ("fxChorusRate");
    ch.depth    = *apvts.getRawParameterValue ("fxChorusDepth");
    ch.feedback = *apvts.getRawParameterValue ("fxChorusFb");
    ch.mix      = *apvts.getRawParameterValue ("fxChorusMix");

    auto& ph = fxChain.phaser;
    ph.enabled  = *apvts.getRawParameterValue ("fxPhaserOn") > 0.5f;
    ph.rate     = *apvts.getRawParameterValue ("fxPhaserRate");
    ph.depth    = *apvts.getRawParameterValue ("fxPhaserDepth");
    ph.feedback = *apvts.getRawParameterValue ("fxPhaserFb");
    ph.mix      = *apvts.getRawParameterValue ("fxPhaserMix");

    auto& fl = fxChain.flanger;
    fl.enabled  = *apvts.getRawParameterValue ("fxFlangerOn") > 0.5f;
    fl.rate     = *apvts.getRawParameterValue ("fxFlangerRate");
    fl.depth    = *apvts.getRawParameterValue ("fxFlangerDepth");
    fl.feedback = *apvts.getRawParameterValue ("fxFlangerFb");
    fl.mix      = *apvts.getRawParameterValue ("fxFlangerMix");

    auto& rv = fxChain.reverb;
    rv.enabled   = *apvts.getRawParameterValue ("fxReverbOn") > 0.5f;
    rv.mode      = static_cast<ReverbMode> (
        (int)*apvts.getRawParameterValue ("fxReverbMode"));
    rv.size      = *apvts.getRawParameterValue ("fxReverbSize");
    rv.damping   = *apvts.getRawParameterValue ("fxReverbDamp");
    rv.width     = *apvts.getRawParameterValue ("fxReverbWidth");
    rv.mix       = *apvts.getRawParameterValue ("fxReverbMix");
    rv.predelay  = *apvts.getRawParameterValue ("fxReverbPredelay");
}

//==============================================================================
bool SynthifyProcessor::loadSampleFile (int oscIndex, const juce::File& file)
{
    if (oscIndex < 0 || oscIndex >= NUM_OSC) return false;
    auto& state = oscStates[(size_t)oscIndex];

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    const int maxSamples = static_cast<int> (
        std::min ((juce::int64)reader->lengthInSamples,
                  (juce::int64)(reader->sampleRate * 30.0)));

    state.sampleBuffer.setSize ((int)reader->numChannels, maxSamples);
    reader->read (&state.sampleBuffer, 0, maxSamples, 0, true, true);
    state.sampleFileSampleRate = reader->sampleRate;
    state.sampleLoaded.store (true);

    setWavetablePosition (oscIndex, 0.5f);

    juce::MessageManager::callAsync ([this, oscIndex] {
        listeners.call ([oscIndex] (Listener& l) { l.sampleLoaded (oscIndex); });
    });

    return true;
}

void SynthifyProcessor::setWavetablePosition (int oscIndex, float pos)
{
    if (oscIndex < 0 || oscIndex >= NUM_OSC) return;
    pos = juce::jlimit (0.0f, 1.0f, pos);
    oscStates[(size_t)oscIndex].wavetablePosition.store (pos);

    // Always rebuild on the message thread to avoid audio-thread reallocation.
    juce::MessageManager::callAsync ([this, oscIndex, pos] {
        rebuildWavetableFromPosition (oscIndex, pos);

        juce::String paramID = "wtPosition" + juce::String (oscIndex);
        if (auto* p = apvts.getParameter (paramID))
            p->setValueNotifyingHost (pos);
    });
}

float SynthifyProcessor::getWavetablePosition (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= NUM_OSC) return 0.5f;
    return oscStates[(size_t)oscIndex].wavetablePosition.load();
}

void SynthifyProcessor::rebuildWavetableFromPosition (int oscIndex, float pos)
{
    auto& state = oscStates[(size_t)oscIndex];
    if (!state.sampleLoaded.load()) return;

    constexpr int wtSize = 2048;
    int totalSamples = state.sampleBuffer.getNumSamples();
    if (totalSamples < wtSize) return;

    int numCh     = state.sampleBuffer.getNumChannels();
    int centreIdx = static_cast<int> (pos * (float)(totalSamples - 1));
    int startIdx  = juce::jlimit (0, totalSamples - wtSize, centreIdx - wtSize / 2);

    std::vector<float> cycle ((size_t)wtSize);
    for (int i = 0; i < wtSize; i++)
    {
        float s = 0.0f;
        for (int ch = 0; ch < numCh; ch++)
            s += state.sampleBuffer.getSample (ch, startIdx + i);
        cycle[(size_t)i] = s / (float)numCh;
    }

    float detectedFreq = state.analyzer.detectPitch (cycle.data(), wtSize,
                                                     (float)state.sampleFileSampleRate);
    if (detectedFreq > 20.0f)
    {
        float sourcePeriod = (float)state.sampleFileSampleRate / detectedFreq;
        float targetPeriod = (float)state.sampleFileSampleRate / 261.63f;
        float ratio        = sourcePeriod / targetPeriod;

        std::vector<float> resampled ((size_t)wtSize);
        for (int i = 0; i < wtSize; i++)
        {
            float srcIdx = (float)i * ratio;
            int   s0     = (int)srcIdx % wtSize;
            int   s1     = (s0 + 1) % wtSize;
            float frac   = srcIdx - std::floor (srcIdx);
            resampled[(size_t)i] = cycle[(size_t)s0] + frac * (cycle[(size_t)s1] - cycle[(size_t)s0]);
        }
        cycle = resampled;
    }

    float peak = 0.0f;
    for (float s : cycle) peak = std::max (peak, std::abs (s));
    if (peak > 0.0001f)
        for (float& s : cycle) s /= peak;

    engine.setWavetable (oscIndex, cycle);
}

//==============================================================================
void SynthifyProcessor::startAnalysis (int oscIndex)
{
    if (oscIndex < 0 || oscIndex >= NUM_OSC) return;
    auto& state = oscStates[(size_t)oscIndex];
    if (!state.sampleLoaded.load() || state.analyzing.load()) return;

    if (state.analysisThread && state.analysisThread->joinable())
        state.analysisThread->join();

    state.analyzing.store (true);
    state.analysisCancel.store (false);

    juce::MessageManager::callAsync ([this, oscIndex] {
        listeners.call ([oscIndex] (Listener& l) { l.analysisStarted (oscIndex); });
    });

    state.analysisThread = std::make_unique<std::thread> ([this, oscIndex]
    {
        auto& st = oscStates[(size_t)oscIndex];
        float bestPos = st.analyzer.findBestWavetablePosition (
            st.sampleBuffer, (float)st.sampleFileSampleRate, st.analysisCancel);

        if (!st.analysisCancel.load())
            onAnalysisComplete (oscIndex, bestPos);
        else
        {
            st.analyzing.store (false);
            juce::MessageManager::callAsync ([this, oscIndex] {
                listeners.call ([oscIndex] (Listener& l) { l.analysisCancelled (oscIndex); });
            });
        }
    });
}

void SynthifyProcessor::cancelAnalysis (int oscIndex)
{
    if (oscIndex >= 0 && oscIndex < NUM_OSC)
        oscStates[(size_t)oscIndex].analysisCancel.store (true);
}

bool SynthifyProcessor::isAnalyzing (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= NUM_OSC) return false;
    return oscStates[(size_t)oscIndex].analyzing.load();
}

bool SynthifyProcessor::hasSampleLoaded (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= NUM_OSC) return false;
    return oscStates[(size_t)oscIndex].sampleLoaded.load();
}

const juce::AudioBuffer<float>& SynthifyProcessor::getSampleBuffer (int oscIndex) const
{
    return oscStates[(size_t)oscIndex].sampleBuffer;
}

void SynthifyProcessor::onAnalysisComplete (int oscIndex, float bestPos)
{
    oscStates[(size_t)oscIndex].analyzing.store (false);
    setWavetablePosition (oscIndex, bestPos);

    juce::MessageManager::callAsync ([this, oscIndex] {
        listeners.call ([oscIndex] (Listener& l) { l.analysisFinished (oscIndex); });
    });
}

//==============================================================================
void SynthifyProcessor::parameterChanged (const juce::String& paramID, float newValue)
{
    if (paramID == "attack" || paramID == "decay" ||
        paramID == "sustain" || paramID == "release")
    {
        engine.setADSR (
            *apvts.getRawParameterValue ("attack"),
            *apvts.getRawParameterValue ("decay"),
            *apvts.getRawParameterValue ("sustain"),
            *apvts.getRawParameterValue ("release"));
        return;
    }

    if (paramID == "width")  { engine.setWidth (newValue); return; }
    if (paramID == "legato") { engine.setLegato (newValue > 0.5f); return; }

    for (int i = 0; i < 3; i++)
    {
        juce::String idx (i);
        if (paramID == "wtPosition" + idx)
        {
            float current = oscStates[(size_t)i].wavetablePosition.load();
            if (std::abs (newValue - current) > 0.0005f)
            {
                oscStates[(size_t)i].wavetablePosition.store (newValue);
                const int capturedI = i;
                const float capturedVal = newValue;
                juce::MessageManager::callAsync ([this, capturedI, capturedVal] {
                    rebuildWavetableFromPosition (capturedI, capturedVal);
                });
            }
            return;
        }
        if (paramID == "oscVol"    + idx) { engine.setOscVolume (i, newValue); return; }
        if (paramID == "oscPan"    + idx) { engine.setOscPan    (i, newValue); return; }
        if (paramID == "oscOn"     + idx) { engine.setOscActive (i, newValue > 0.5f); return; }
        if (paramID == "oscPitch"  + idx || paramID == "oscDetune" + idx)
        {
            engine.setOscPitch (i,
                *apvts.getRawParameterValue ("oscPitch"  + idx),
                *apvts.getRawParameterValue ("oscDetune" + idx));
            return;
        }
        if (paramID == "oscUnisonCount" + idx || paramID == "oscUnisonDetune" + idx)
        {
            engine.setOscUnison (i,
                (int)*apvts.getRawParameterValue ("oscUnisonCount"  + idx),
                     *apvts.getRawParameterValue ("oscUnisonDetune" + idx));
            return;
        }
        if (paramID == "oscPhase" + idx)
        {
            engine.setOscPhase (i, newValue);
            return;
        }
    }

    // Any FX param change → sync all FX params
    if (paramID.startsWith ("fx"))
    {
        applyFxParams();
        return;
    }
}

//==============================================================================
juce::AudioProcessorEditor* SynthifyProcessor::createEditor()
{
    return new SynthifyEditor (*this);
}

void SynthifyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SynthifyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthifyProcessor();
}
