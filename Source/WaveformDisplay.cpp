#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay (WaveForgeProcessor& p) : audioProcessor (p)
{
    startTimerHz (30);
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

void WaveformDisplay::timerCallback()
{
    playbackPhase = audioProcessor.getEngine().getPlaybackPhase();
    repaint();
}

void WaveformDisplay::setPositionFromX (int x)
{
    auto pos = (float) x / (float) juce::jmax (1, getWidth());
    audioProcessor.setWavetablePosition (juce::jlimit (0.0f, 1.0f, pos));
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& e) { setPositionFromX (e.x); }
void WaveformDisplay::mouseDrag (const juce::MouseEvent& e) { setPositionFromX (e.x); }

void WaveformDisplay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff040404));
    auto area = getLocalBounds().toFloat().reduced (12.0f);
    g.setColour (juce::Colour (0xff202020));
    g.drawRoundedRectangle (area, 8.0f, 1.0f);

    if (! audioProcessor.hasSampleLoaded())
    {
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (15.0f);
        g.drawFittedText ("DROP OR OPEN A SAMPLE", area.toNearestInt(), juce::Justification::centred, 1);
        return;
    }

    const auto& buffer = audioProcessor.getSampleBuffer();
    auto samples = buffer.getNumSamples();
    auto channels = buffer.getNumChannels();

    juce::Path waveform;
    auto centreY = area.getCentreY();
    for (int x = 0; x < (int) area.getWidth(); ++x)
    {
        auto sampleIndex = juce::jlimit (0, samples - 1,
            (int) ((float) x / juce::jmax (1.0f, area.getWidth() - 1.0f) * (float) (samples - 1)));

        float value = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            value += buffer.getSample (ch, sampleIndex);
        value /= (float) juce::jmax (1, channels);

        auto y = centreY - value * area.getHeight() * 0.45f;
        if (x == 0) waveform.startNewSubPath (area.getX(), y);
        else        waveform.lineTo (area.getX() + (float) x, y);
    }

    g.setColour (juce::Colours::white.withAlpha (0.78f));
    g.strokePath (waveform, juce::PathStrokeType (1.2f));

    auto posX = area.getX() + area.getWidth() * audioProcessor.getWavetablePosition();
    auto wtLength = audioProcessor.apvts.getRawParameterValue ("wtLength")->load();
    auto windowW = juce::jmax (4.0f, area.getWidth() * wtLength / (float) juce::jmax (1, samples));
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.fillRect (juce::Rectangle<float> (posX - windowW * 0.5f, area.getY(), windowW, area.getHeight()));
    g.setColour (juce::Colours::white);
    g.drawLine (posX, area.getY(), posX, area.getBottom(), 2.0f);

    auto phaseX = area.getX() + area.getWidth() * playbackPhase;
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawLine (phaseX, area.getY(), phaseX, area.getBottom(), 1.0f);
}
