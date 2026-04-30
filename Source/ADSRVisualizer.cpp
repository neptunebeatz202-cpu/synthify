#include "ADSRVisualizer.h"

ADSRVisualizer::ADSRVisualizer (WaveForgeProcessor& p) : audioProcessor (p)
{
    startTimerHz (30);
}

ADSRVisualizer::~ADSRVisualizer()
{
    stopTimer();
}

void ADSRVisualizer::timerCallback()
{
    envelopeValue = audioProcessor.getEngine().getADSREnvelopeValue();
    repaint();
}

void ADSRVisualizer::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (12.0f);
    g.fillAll (juce::Colour (0xff050505));
    g.setColour (juce::Colour (0xff202020));
    g.drawRoundedRectangle (area, 8.0f, 1.0f);

    auto attack = audioProcessor.apvts.getRawParameterValue ("attack")->load();
    auto decay = audioProcessor.apvts.getRawParameterValue ("decay")->load();
    auto sustain = audioProcessor.apvts.getRawParameterValue ("sustain")->load();
    auto release = audioProcessor.apvts.getRawParameterValue ("release")->load();
    auto total = juce::jmax (0.001f, attack + decay + release + 0.35f);

    auto x0 = area.getX();
    auto y0 = area.getBottom();
    auto w = area.getWidth();
    auto h = area.getHeight();
    auto attackX = x0 + w * (attack / total);
    auto decayX = attackX + w * (decay / total);
    auto sustainX = decayX + w * (0.35f / total);
    auto sustainY = y0 - h * sustain;

    juce::Path env;
    env.startNewSubPath (x0, y0);
    env.lineTo (attackX, area.getY());
    env.lineTo (decayX, sustainY);
    env.lineTo (sustainX, sustainY);
    env.lineTo (area.getRight(), y0);

    g.setColour (juce::Colours::white);
    g.strokePath (env, juce::PathStrokeType (2.0f));

    auto lineX = juce::jmap (envelopeValue, 0.0f, 1.0f, area.getX(), area.getRight());
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (lineX, area.getY(), lineX, area.getBottom(), 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (11.0f);
    g.drawText ("ADSR", area.toNearestInt().reduced (8), juce::Justification::topLeft);
}
