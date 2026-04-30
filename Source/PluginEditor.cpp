#include "PluginEditor.h"

WaveForgeEditor::WaveForgeEditor (WaveForgeProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), waveform (p), adsrView (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (860, 560);

    titleLabel.setText ("WAVEFORGE", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::Font (26.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    statusLabel.setText ("Drop a sample or open one from disk", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.72f));
    addAndMakeVisible (statusLabel);

    configureControlLabel (startLabel, "LOOP START");
    configureControlLabel (endLabel, "LOOP END");
    configureControlLabel (portaLabel, "PORTA");
    configureControlLabel (gainLabel, "GAIN");
    configureControlLabel (cutoffLabel, "CUTOFF");
    configureControlLabel (resLabel, "RES");

    for (auto* b : { &browseButton, &cancelButton })
    {
        b->addListener (this);
        addAndMakeVisible (*b);
    }
    cancelButton.setVisible (false);
    addAndMakeVisible (monoButton);
    addAndMakeVisible (legatoButton);

    for (auto* s : { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider,
                     &portaSlider, &gainSlider, &cutoffSlider, &resSlider, &wtSlider,
                     &loopEndSlider })
        configureSlider (*s);

    cutoffSlider.setTextValueSuffix (" Hz");
    wtSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    loopEndSlider.setSliderStyle (juce::Slider::LinearHorizontal);

    attackAttachment  = std::make_unique<SliderAttachment> (audioProcessor.apvts, "attack", attackSlider);
    decayAttachment   = std::make_unique<SliderAttachment> (audioProcessor.apvts, "decay", decaySlider);
    sustainAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "sustain", sustainSlider);
    releaseAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "release", releaseSlider);
    portaAttachment   = std::make_unique<SliderAttachment> (audioProcessor.apvts, "portamento", portaSlider);
    gainAttachment    = std::make_unique<SliderAttachment> (audioProcessor.apvts, "gain", gainSlider);
    cutoffAttachment  = std::make_unique<SliderAttachment> (audioProcessor.apvts, "filterCutoff", cutoffSlider);
    resAttachment     = std::make_unique<SliderAttachment> (audioProcessor.apvts, "filterRes", resSlider);
    wtAttachment      = std::make_unique<SliderAttachment> (audioProcessor.apvts, "wtPosition", wtSlider);
    loopEndAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "loopEnd", loopEndSlider);
    monoAttachment    = std::make_unique<ButtonAttachment> (audioProcessor.apvts, "mono", monoButton);
    legatoAttachment  = std::make_unique<ButtonAttachment> (audioProcessor.apvts, "legato", legatoButton);

    addAndMakeVisible (waveform);
    addAndMakeVisible (adsrView);
    audioProcessor.addListener (this);
}

WaveForgeEditor::~WaveForgeEditor()
{
    audioProcessor.removeListener (this);
    setLookAndFeel (nullptr);
}

void WaveForgeEditor::configureSlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);
}

void WaveForgeEditor::configureControlLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.82f));
    label.setFont (juce::Font (12.0f, juce::Font::bold));
    addAndMakeVisible (label);
}

void WaveForgeEditor::layoutControl (juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label)
{
    label.setBounds (area.removeFromTop (18));
    slider.setBounds (area.reduced (5, 0));
}

void WaveForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff101010));
    for (int x = 0; x < getWidth(); x += 18) g.drawVerticalLine (x, 0.0f, b.getBottom());
    for (int y = 0; y < getHeight(); y += 18) g.drawHorizontalLine (y, 0.0f, b.getRight());

    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawRect (getLocalBounds().reduced (8), 1);

    if (analysisOverlayVisible)
        drawAnalysisOverlay (g);
}

void WaveForgeEditor::drawAnalysisOverlay (juce::Graphics& g)
{
    auto overlay = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.72f));
    g.fillRect (overlay);

    auto panel = overlay.withSizeKeepingCentre (330.0f, 150.0f);
    g.setColour (juce::Colour (0xff090909));
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (juce::Colours::white);
    g.drawRoundedRectangle (panel, 8.0f, 1.2f);
    g.setFont (juce::Font (19.0f, juce::Font::bold));
    g.drawText ("ANALYZING SAMPLE", panel.reduced (18.0f).toNearestInt(), juce::Justification::centredTop);
    g.setFont (13.0f);
    g.drawText ("Finding the cleanest pitched wavetable position",
                panel.reduced (18.0f, 52.0f).toNearestInt(), juce::Justification::centred);
}

void WaveForgeEditor::resized()
{
    auto area = getLocalBounds().reduced (18);
    auto top = area.removeFromTop (44);
    titleLabel.setBounds (top.removeFromLeft (220));
    statusLabel.setBounds (top);

    auto buttons = area.removeFromTop (42);
    browseButton.setBounds (buttons.removeFromLeft (92).reduced (0, 4));
    cancelButton.setBounds (buttons.removeFromLeft (92).reduced (0, 4));
    monoButton.setBounds (buttons.removeFromLeft (78).reduced (10, 4));
    legatoButton.setBounds (buttons.removeFromLeft (92).reduced (8, 4));
    auto loopControls = buttons.reduced (10, 0);
    auto loopTop = loopControls.removeFromTop (21);
    startLabel.setBounds (loopTop.removeFromLeft (88));
    wtSlider.setBounds (loopTop.reduced (4, 1));
    auto loopBottom = loopControls;
    endLabel.setBounds (loopBottom.removeFromLeft (88));
    loopEndSlider.setBounds (loopBottom.reduced (4, 1));

    waveform.setBounds (area.removeFromTop (185).reduced (0, 6));

    auto lower = area.reduced (0, 8);
    auto left = lower.removeFromLeft (360);
    adsrView.setBounds (left.removeFromTop (146));

    auto adsrControls = left.removeFromTop (120);
    auto knobW = adsrControls.getWidth() / 4;
    attackSlider.setBounds (adsrControls.removeFromLeft (knobW).reduced (5));
    decaySlider.setBounds (adsrControls.removeFromLeft (knobW).reduced (5));
    sustainSlider.setBounds (adsrControls.removeFromLeft (knobW).reduced (5));
    releaseSlider.setBounds (adsrControls.reduced (5));

    auto synth = lower;
    auto k = synth.getWidth() / 4;
    layoutControl (synth.removeFromLeft (k).reduced (6), portaSlider, portaLabel);
    layoutControl (synth.removeFromLeft (k).reduced (6), gainSlider, gainLabel);
    layoutControl (synth.removeFromLeft (k).reduced (6), cutoffSlider, cutoffLabel);
    layoutControl (synth.removeFromLeft (k).reduced (6), resSlider, resLabel);
}

bool WaveForgeEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg"))
            return true;
    return false;
}

void WaveForgeEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        auto file = juce::File (f);
        if (file.existsAsFile())
        {
            loadFile (file);
            break;
        }
    }
}

void WaveForgeEditor::loadFile (const juce::File& file)
{
    if (audioProcessor.loadSampleFile (file))
        statusLabel.setText (file.getFileName(), juce::dontSendNotification);
    else
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
            "WaveForge", "That file could not be opened as an audio sample.");
}

void WaveForgeEditor::buttonClicked (juce::Button* button)
{
    if (button == &browseButton)
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Open sample", juce::File(),
                                                           "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file.existsAsFile())
                    loadFile (file);
            });
    }
    else if (button == &cancelButton)
    {
        audioProcessor.cancelAnalysis();
    }
}

void WaveForgeEditor::analysisStarted()
{
    analysisOverlayVisible = true;
    cancelButton.setVisible (true);
    repaint();
}

void WaveForgeEditor::analysisFinished()
{
    analysisOverlayVisible = false;
    cancelButton.setVisible (false);
    statusLabel.setText ("Analysis complete - wavetable tuned around C", juce::dontSendNotification);
    repaint();
}

void WaveForgeEditor::analysisCancelled()
{
    analysisOverlayVisible = false;
    cancelButton.setVisible (false);
    statusLabel.setText ("Analysis cancelled", juce::dontSendNotification);
    repaint();
}

void WaveForgeEditor::sampleLoaded()
{
    waveform.repaint();
}
