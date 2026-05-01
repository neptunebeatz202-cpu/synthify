#include "WaveformDisplay.h"
#include "PluginProcessor.h"

static const juce::String kAudioExts = "wav;aif;aiff;flac;mp3;ogg";

OscWaveformDisplay::OscWaveformDisplay (SynthifyProcessor& p, int idx)
    : proc (p), oscIndex (idx)
{
    // Timer is started in visibilityChanged() once the component is on screen.
}

OscWaveformDisplay::~OscWaveformDisplay()
{
    stopTimer();
}

void OscWaveformDisplay::timerCallback()
{
    float newPhase = proc.getEngine().getPlaybackPhase();
    if (std::abs (newPhase - playPhase) > 0.001f)
    {
        playPhase = newPhase;
        repaint();
    }
}

void OscWaveformDisplay::visibilityChanged()
{
    if (isVisible() && isShowing())
        startTimerHz (24);
    else
        stopTimer();
}

bool OscWaveformDisplay::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension (kAudioExts))
            return true;
    return false;
}

void OscWaveformDisplay::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
    {
        juce::File file (f);
        if (file.existsAsFile() && file.hasFileExtension (kAudioExts))
        {
            proc.loadSampleFile (oscIndex, file);
            break;
        }
    }
    repaint();
}

void OscWaveformDisplay::setPositionFromX (int x)
{
    float pos = (float)x / (float)juce::jmax (1, getWidth());
    proc.setWavetablePosition (oscIndex, juce::jlimit (0.0f, 1.0f, pos));
}

void OscWaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (proc.hasSampleLoaded (oscIndex))
        setPositionFromX (e.x);
}

void OscWaveformDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (proc.hasSampleLoaded (oscIndex))
        setPositionFromX (e.x);
}

void OscWaveformDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff0d0d0d));
    g.fillRoundedRectangle (area, 4.0f);

    if (dragOver)
    {
        g.setColour (juce::Colour (0xff8b0000).withAlpha (0.18f));
        g.fillRoundedRectangle (area, 4.0f);
    }

    bool loaded = proc.hasSampleLoaded (oscIndex);

    if (!loaded)
    {
        g.setColour (juce::Colour (0xff8b0000).withAlpha (0.30f));
        g.drawRoundedRectangle (area.reduced (1.0f), 4.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawFittedText ("DROP SAMPLE " + juce::String (oscIndex + 1),
                          getLocalBounds(), juce::Justification::centred, 1);
        return;
    }

    g.setColour (juce::Colour (0xff222222));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);

    const auto& buf     = proc.getSampleBuffer (oscIndex);
    int totalSamples    = buf.getNumSamples();
    int numCh           = buf.getNumChannels();
    if (totalSamples < 2 || numCh < 1) return;

    float innerW        = area.getWidth() - 4.0f;
    float innerH        = area.getHeight();
    float originX       = area.getX() + 2.0f;
    float centreY       = area.getCentreY();

    juce::Path waveform;
    for (int px = 0; px < (int)innerW; ++px)
    {
        int sIdx = juce::jlimit (0, totalSamples - 1,
                                 (int)((float)px / innerW * (float)(totalSamples - 1)));
        float val = 0.0f;
        for (int ch = 0; ch < numCh; ch++)
            val += buf.getSample (ch, sIdx);
        val /= (float)juce::jmax (1, numCh);

        float xf = originX + (float)px;
        float yf = centreY - val * innerH * 0.44f;
        if (px == 0) waveform.startNewSubPath (xf, yf);
        else         waveform.lineTo           (xf, yf);
    }

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.strokePath (waveform, juce::PathStrokeType (1.0f));

    float wtPos  = proc.getWavetablePosition (oscIndex);
    float markerX = originX + wtPos * innerW;

    float markerW = juce::jmax (2.0f, (2048.0f / (float)totalSamples) * innerW);
    float markerL = markerX - markerW * 0.5f;
    g.setColour (juce::Colour (0xff8b0000).withAlpha (0.18f));
    g.fillRect (markerL, area.getY(), markerW, area.getHeight());

    g.setColour (juce::Colour (0xff8b0000).withAlpha (0.95f));
    g.drawLine (markerX, area.getY() + 2.0f, markerX, area.getBottom() - 2.0f, 1.5f);

    if (playPhase > 0.001f)
    {
        float phaseX = originX + playPhase * innerW;
        g.setColour (juce::Colours::white.withAlpha (0.20f));
        g.drawLine (phaseX, area.getY(), phaseX, area.getBottom(), 1.0f);
    }
}
