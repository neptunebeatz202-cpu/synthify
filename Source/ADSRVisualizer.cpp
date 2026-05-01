#include "ADSRVisualizer.h"
#include "PluginProcessor.h"

ADSRVisualizer::ADSRVisualizer (SynthifyProcessor& p) : proc (p)
{
    // Timer started in visibilityChanged() once on screen.
}

ADSRVisualizer::~ADSRVisualizer()
{
    stopTimer();
}

void ADSRVisualizer::timerCallback()
{
    float newVal = proc.getEngine().getADSREnvelopeValue();
    if (std::abs (newVal - envelopeValue) > 0.001f)
    {
        envelopeValue = newVal;
        repaint();
    }
}

void ADSRVisualizer::visibilityChanged()
{
    if (isVisible() && isShowing())
        startTimerHz (30);
    else
        stopTimer();
}

void ADSRVisualizer::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (juce::Colour (0xff0c0c0c));
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (juce::Colour (0xff222222));
    g.drawRoundedRectangle (area, 4.0f, 1.0f);

    auto inner = area.reduced (10.0f, 8.0f);
    float W = inner.getWidth();
    float H = inner.getHeight();
    float x0 = inner.getX();
    float yB = inner.getBottom();
    float yT = inner.getY();

    auto a = proc.apvts.getRawParameterValue ("attack")->load();
    auto d = proc.apvts.getRawParameterValue ("decay")->load();
    auto s = proc.apvts.getRawParameterValue ("sustain")->load();
    auto r = proc.apvts.getRawParameterValue ("release")->load();

    float sustainWidth = 0.2f;
    float total = a + d + sustainWidth + r;
    if (total < 0.001f) total = 0.001f;

    float aX  = x0 + W * (a / total);
    float dX  = aX  + W * (d / total);
    float sX  = dX  + W * (sustainWidth / total);
    float rX  = x0 + W;
    float sY  = yT + H * (1.0f - s);

    juce::Path env;
    env.startNewSubPath (x0, yB);
    env.lineTo (aX, yT);
    env.lineTo (dX, sY);
    env.lineTo (sX, sY);
    env.lineTo (rX, yB);

    juce::Path filled = env;
    filled.lineTo (rX, yB);
    filled.lineTo (x0, yB);
    filled.closeSubPath();

    g.setColour (juce::Colour (0xff8b0000).withAlpha (0.10f));
    g.fillPath (filled);

    g.setColour (juce::Colour (0xff8b0000).withAlpha (0.85f));
    g.strokePath (env, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    if (envelopeValue > 0.001f)
    {
        float ex = x0 + W * envelopeValue;
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawLine (ex, yT, ex, yB, 1.0f);

        float ey = yB - H * envelopeValue;
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (ex - 3.0f, ey - 3.0f, 6.0f, 6.0f);
    }

    g.setColour (juce::Colour (0xff8b0000).withAlpha (0.55f));
    g.setFont (juce::Font (9.0f, juce::Font::bold));
    g.drawText ("ENVELOPE", inner.toNearestInt(), juce::Justification::topLeft, false);
}
