#include "PluginEditor.h"
#include "BinaryData.h"

static const int kW = 980;
static const int kH = 680;

//==============================================================================
SynthifyEditor::SynthifyEditor (SynthifyProcessor& p)
    : AudioProcessorEditor (&p), proc (p), adsrViz (p)
{
    // Set the look and feel FIRST — before any addAndMakeVisible calls,
    // so paint() always has a valid LAF.
    setLookAndFeel (&laf);

    // ── Tab buttons ────────────────────────────────────────────────────────────
    synthTabBtn.setClickingTogglesState (false);
    fxTabBtn.setClickingTogglesState (false);
    synthTabBtn.addListener (this);
    fxTabBtn.addListener (this);
    addAndMakeVisible (synthTabBtn);
    addAndMakeVisible (fxTabBtn);

    // ── Oscillator panels ─────────────────────────────────────────────────────
    for (int i = 0; i < NUM_OSC; i++)
    {
        auto& pnl = oscPanels[(size_t)i];
        pnl = std::make_unique<OscPanel> (proc, i);

        pnl->titleLabel.setFont (juce::Font (9.0f, juce::Font::bold));
        pnl->titleLabel.setColour (juce::Label::textColourId, SynthifyLAF::kAccent);
        pnl->titleLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (pnl->titleLabel);

        addAndMakeVisible (pnl->waveform);

        // WT slider
        pnl->wtSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        pnl->wtSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (pnl->wtSlider);
        makeLabel (pnl->wtLabel, "POSITION");
        addAndMakeVisible (pnl->wtLabel);

        // Rotary knobs
        for (auto* s : { &pnl->volSlider, &pnl->panSlider, &pnl->pitchSlider,
                         &pnl->detuneSlider, &pnl->phaseSlider,
                         &pnl->unisonSlider, &pnl->unisonDetuneSlider })
            makeRotary (*s);

        pnl->unisonSlider.setNumDecimalPlacesToDisplay (0);

        makeLabel (pnl->volLabel,          "VOL");
        makeLabel (pnl->panLabel,          "PAN");
        makeLabel (pnl->pitchLabel,        "PITCH");
        makeLabel (pnl->detuneLabel,       "DETUNE");
        makeLabel (pnl->phaseLabel,        "PHASE");
        makeLabel (pnl->unisonLabel,       "UNISON");
        makeLabel (pnl->unisonDetuneLabel, "U-DET");

        // ON toggle
        pnl->oscOnBtn.addListener (this);
        addAndMakeVisible (pnl->oscOnBtn);

        // Buttons
        for (auto* b : { &pnl->openBtn, &pnl->analyzeBtn, &pnl->cancelBtn })
        {
            b->addListener (this);
            addAndMakeVisible (*b);
        }
        pnl->cancelBtn.setVisible (false);
    }

    // ── ADSR ──────────────────────────────────────────────────────────────────
    addAndMakeVisible (adsrViz);
    makeRotary (attackSlider);   makeLabel (attackLabel,  "ATK");
    makeRotary (decaySlider);    makeLabel (decayLabel,   "DCY");
    makeRotary (sustainSlider);  makeLabel (sustainLabel, "SUS");
    makeRotary (releaseSlider);  makeLabel (releaseLabel, "REL");

    // ── Global controls ────────────────────────────────────────────────────────
    makeRotary (portaSlider);  makeLabel (portaLabel,  "GLIDE");
    makeRotary (gainSlider);   makeLabel (gainLabel,   "GAIN");
    makeRotary (cutoffSlider); makeLabel (cutoffLabel, "CUTOFF");
    makeRotary (resSlider);    makeLabel (resLabel,    "RES");
    makeRotary (widthSlider);  makeLabel (widthLabel,  "WIDTH");

    monoBtn.addListener   (this); addAndMakeVisible (monoBtn);
    legatoBtn.addListener (this); addAndMakeVisible (legatoBtn);

    // Status label
    statusLabel.setFont (juce::Font (9.5f));
    statusLabel.setColour (juce::Label::textColourId, SynthifyLAF::kDim);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setText ("Drop samples into oscillators", juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    // ── FX Modules ─────────────────────────────────────────────────────────────
    buildFxModules();

    // ── APVTS Attachments — created LAST after all components are added ─────────
    // (Attachments immediately sync the slider value, which can trigger repaints.
    //  All components must already exist and be added before this point.)
    for (int i = 0; i < NUM_OSC; i++)
    {
        auto& pnl = oscPanels[(size_t)i];
        juce::String idx (i);
        pnl->wtAttach           = std::make_unique<OscPanel::SA> (proc.apvts, "wtPosition"      + idx, pnl->wtSlider);
        pnl->volAttach          = std::make_unique<OscPanel::SA> (proc.apvts, "oscVol"          + idx, pnl->volSlider);
        pnl->panAttach          = std::make_unique<OscPanel::SA> (proc.apvts, "oscPan"          + idx, pnl->panSlider);
        pnl->pitchAttach        = std::make_unique<OscPanel::SA> (proc.apvts, "oscPitch"        + idx, pnl->pitchSlider);
        pnl->detuneAttach       = std::make_unique<OscPanel::SA> (proc.apvts, "oscDetune"       + idx, pnl->detuneSlider);
        pnl->phaseAttach        = std::make_unique<OscPanel::SA> (proc.apvts, "oscPhase"        + idx, pnl->phaseSlider);
        pnl->unisonAttach       = std::make_unique<OscPanel::SA> (proc.apvts, "oscUnisonCount"  + idx, pnl->unisonSlider);
        pnl->unisonDetuneAttach = std::make_unique<OscPanel::SA> (proc.apvts, "oscUnisonDetune" + idx, pnl->unisonDetuneSlider);
        pnl->onAttach           = std::make_unique<OscPanel::BA> (proc.apvts, "oscOn"           + idx, pnl->oscOnBtn);
    }

    attackAtt  = std::make_unique<SA> (proc.apvts, "attack",  attackSlider);
    decayAtt   = std::make_unique<SA> (proc.apvts, "decay",   decaySlider);
    sustainAtt = std::make_unique<SA> (proc.apvts, "sustain", sustainSlider);
    releaseAtt = std::make_unique<SA> (proc.apvts, "release", releaseSlider);

    portaAtt  = std::make_unique<SA> (proc.apvts, "portamento",   portaSlider);
    gainAtt   = std::make_unique<SA> (proc.apvts, "gain",         gainSlider);
    cutoffAtt = std::make_unique<SA> (proc.apvts, "filterCutoff", cutoffSlider);
    resAtt    = std::make_unique<SA> (proc.apvts, "filterRes",    resSlider);
    widthAtt  = std::make_unique<SA> (proc.apvts, "width",        widthSlider);

    monoAtt   = std::make_unique<BA> (proc.apvts, "mono",   monoBtn);
    legatoAtt = std::make_unique<BA> (proc.apvts, "legato", legatoBtn);

    proc.addListener (this);

    // Size last — triggers resized() which lays out all bounds.
    setSize (kW, kH);
    setResizable (false, false);
}

SynthifyEditor::~SynthifyEditor()
{
    // Remove processor listener first so no callbacks fire during teardown.
    proc.removeListener (this);

    // Destroy all APVTS attachments explicitly before components are destroyed.
    // (Attachment destructors call removeListener on the parameter, which is safe here.)
    for (auto& pnl : oscPanels)
    {
        if (!pnl) continue;
        pnl->wtAttach.reset();
        pnl->volAttach.reset();
        pnl->panAttach.reset();
        pnl->pitchAttach.reset();
        pnl->detuneAttach.reset();
        pnl->phaseAttach.reset();
        pnl->unisonAttach.reset();
        pnl->unisonDetuneAttach.reset();
        pnl->onAttach.reset();
    }

    attackAtt.reset();  decayAtt.reset();  sustainAtt.reset();  releaseAtt.reset();
    portaAtt.reset();   gainAtt.reset();   cutoffAtt.reset();   resAtt.reset();
    widthAtt.reset();   monoAtt.reset();   legatoAtt.reset();

    setLookAndFeel (nullptr);
}

//==============================================================================
void SynthifyEditor::makeRotary (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 13);
    addAndMakeVisible (s);
}

void SynthifyEditor::makeLabel (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::Font (8.5f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, SynthifyLAF::kDim);
    addAndMakeVisible (l);
}

//==============================================================================
void SynthifyEditor::buildFxModules()
{
    // HyperDimension
    fxHyper.reset (buildFxModule ("HYPERDIMENSION", "fxHyperOn",
        { "fxHyperAmount", "fxHyperDetune", "fxHyperMix" },
        { "AMOUNT", "DETUNE", "MIX" }));

    // Distortion — has mode box
    fxDist.reset (buildFxModule ("DISTORTION", "fxDistOn",
        { "fxDistDrive", "fxDistTone", "fxDistMix" },
        { "DRIVE", "TONE", "MIX" }));
    fxDist->hasModeBox = true;
    fxDist->modeBox.addItem ("Tube",        1);
    fxDist->modeBox.addItem ("Sine Shaper", 2);
    fxDist->modeBox.addItem ("Downsample",  3);
    fxDist->modeBox.addItem ("Linear",      4);
    fxDist->modeBox.addItem ("Rectangular", 5);
    fxDist->modeBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (fxDist->modeBox);
    fxDist->modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "fxDistMode", fxDist->modeBox);

    // Chorus
    fxChorus.reset (buildFxModule ("CHORUS", "fxChorusOn",
        { "fxChorusRate", "fxChorusDepth", "fxChorusFb", "fxChorusMix" },
        { "RATE", "DEPTH", "FEEDBK", "MIX" }));

    // Phaser
    fxPhaser.reset (buildFxModule ("PHASER", "fxPhaserOn",
        { "fxPhaserRate", "fxPhaserDepth", "fxPhaserFb", "fxPhaserMix" },
        { "RATE", "DEPTH", "FEEDBK", "MIX" }));

    // Flanger
    fxFlanger.reset (buildFxModule ("FLANGER", "fxFlangerOn",
        { "fxFlangerRate", "fxFlangerDepth", "fxFlangerFb", "fxFlangerMix" },
        { "RATE", "DEPTH", "FEEDBK", "MIX" }));

    // Reverb — has mode box
    fxReverb.reset (buildFxModule ("REVERB", "fxReverbOn",
        { "fxReverbSize", "fxReverbDamp", "fxReverbWidth", "fxReverbMix", "fxReverbPredelay" },
        { "SIZE", "DAMP", "WIDTH", "MIX", "DELAY" }));
    fxReverb->hasModeBox = true;
    fxReverb->modeBox.addItem ("Hall",  1);
    fxReverb->modeBox.addItem ("Plate", 2);
    fxReverb->modeBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (fxReverb->modeBox);
    fxReverb->modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "fxReverbMode", fxReverb->modeBox);
}

SynthifyEditor::FxModule* SynthifyEditor::buildFxModule (
    const juce::String& name,
    const juce::String& enableID,
    const juce::StringArray& knobIDs,
    const juce::StringArray& knobNames)
{
    auto* m = new FxModule();

    m->enableBtn.setButtonText ("ON");
    m->enableBtn.addListener (this);
    addAndMakeVisible (m->enableBtn);

    m->nameLabel.setText (name, juce::dontSendNotification);
    m->nameLabel.setFont (juce::Font (9.5f, juce::Font::bold));
    m->nameLabel.setColour (juce::Label::textColourId, SynthifyLAF::kAccent.withAlpha (0.9f));
    m->nameLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (m->nameLabel);

    m->enableAttach = std::make_unique<BA> (proc.apvts, enableID, m->enableBtn);

    for (int i = 0; i < knobIDs.size(); ++i)
    {
        auto* knob = new juce::Slider();
        makeRotary (*knob);
        m->knobs.add (knob);

        auto* lbl = new juce::Label();
        makeLabel (*lbl, knobNames[i]);
        m->knobLabels.add (lbl);

        auto* att = new SA (proc.apvts, knobIDs[i], *knob);
        m->knobAttachments.add (att);
    }

    return m;
}

//==============================================================================
void SynthifyEditor::paint (juce::Graphics& g)
{
    // Solid black base
    g.fillAll (SynthifyLAF::kBg);

    // Full-bleed background image (dimmed for readability)
    const juce::Image bgImage = juce::ImageCache::getFromMemory (
        BinaryData::background_jpg, BinaryData::background_jpgSize);
    if (bgImage.isValid())
    {
        g.setOpacity (0.30f);
        g.drawImage (bgImage, getLocalBounds().toFloat(),
                     juce::RectanglePlacement::stretchToFit);
        g.setOpacity (1.0f);
    }

    // Dark overlay to push image back even further
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (getLocalBounds());

    // Title in top-left
    SynthifyLAF::paintSynthifyTitle (g, juce::Rectangle<int> (16, 6, 320, 44));

    // Subtle header underline
    g.setColour (SynthifyLAF::kAccent.withAlpha (0.55f));
    g.drawHorizontalLine (50, 0.0f, (float)kW);

    if (currentTab == 0)
    {
        // Oscillator section bg
        SynthifyLAF::paintGrungePanelBg (g, juce::Rectangle<int> (8, 54, kW - 16, 310), "OSCILLATORS");

        // Envelope section bg
        SynthifyLAF::paintGrungePanelBg (g, juce::Rectangle<int> (8, 372, 380, 298), "ENVELOPE");

        // Global section bg
        SynthifyLAF::paintGrungePanelBg (g, juce::Rectangle<int> (396, 372, kW - 404, 298), "GLOBAL");

        // Analysis overlay per osc
        for (int i = 0; i < NUM_OSC; i++)
        {
            if (!analysisOverlay[i]) continue;
            auto wfBounds = oscPanels[(size_t)i]->waveform.getBoundsInParent().expanded (4);
            g.setColour (juce::Colours::black.withAlpha (0.82f));
            g.fillRoundedRectangle (wfBounds.toFloat(), 4.0f);
            g.setColour (SynthifyLAF::kAccent.withAlpha (0.9f));
            g.setFont (juce::Font (10.5f, juce::Font::bold));
            g.drawFittedText ("ANALYZING...", wfBounds, juce::Justification::centred, 1);
        }
    }
    else
    {
        // FX tab background
        SynthifyLAF::paintGrungePanelBg (g, juce::Rectangle<int> (8, 54, kW - 16, kH - 62), "FX CHAIN");
    }
}

//==============================================================================
void SynthifyEditor::resized()
{
    // ── Header ────────────────────────────────────────────────────────────────
    // Title on the left — painted in paint(), just reserve space
    // Tab buttons
    int tabX = kW - 180;
    synthTabBtn.setBounds (tabX,       10, 80, 30);
    fxTabBtn.setBounds    (tabX + 88,  10, 80, 30);

    statusLabel.setBounds (160, 18, tabX - 170, 20);

    // Set tab button visual state
    // (handled in paint via toggle state, but force repaint)
    synthTabBtn.setColour (juce::TextButton::buttonColourId,
        currentTab == 0 ? SynthifyLAF::kAccent2 : SynthifyLAF::kPanel);
    fxTabBtn.setColour (juce::TextButton::buttonColourId,
        currentTab == 1 ? SynthifyLAF::kAccent2 : SynthifyLAF::kPanel);

    //── Show/hide based on tab ─────────────────────────────────────────────────
    bool showSynth = (currentTab == 0);
    bool showFx    = (currentTab == 1);

    // Synth elements
    adsrViz.setVisible (showSynth);
    attackSlider.setVisible (showSynth);  attackLabel.setVisible (showSynth);
    decaySlider.setVisible  (showSynth);  decayLabel.setVisible  (showSynth);
    sustainSlider.setVisible(showSynth);  sustainLabel.setVisible(showSynth);
    releaseSlider.setVisible(showSynth);  releaseLabel.setVisible(showSynth);
    portaSlider.setVisible  (showSynth);  portaLabel.setVisible  (showSynth);
    gainSlider.setVisible   (showSynth);  gainLabel.setVisible   (showSynth);
    cutoffSlider.setVisible (showSynth);  cutoffLabel.setVisible (showSynth);
    resSlider.setVisible    (showSynth);  resLabel.setVisible    (showSynth);
    widthSlider.setVisible  (showSynth);  widthLabel.setVisible  (showSynth);
    monoBtn.setVisible   (showSynth);
    legatoBtn.setVisible (showSynth);

    for (int i = 0; i < NUM_OSC; ++i)
    {
        auto& pnl = oscPanels[(size_t)i];
        pnl->waveform.setVisible    (showSynth);
        pnl->wtSlider.setVisible    (showSynth);
        pnl->wtLabel.setVisible     (showSynth);
        pnl->volSlider.setVisible   (showSynth);   pnl->volLabel.setVisible   (showSynth);
        pnl->panSlider.setVisible   (showSynth);   pnl->panLabel.setVisible   (showSynth);
        pnl->pitchSlider.setVisible (showSynth);   pnl->pitchLabel.setVisible (showSynth);
        pnl->detuneSlider.setVisible(showSynth);   pnl->detuneLabel.setVisible(showSynth);
        pnl->phaseSlider.setVisible (showSynth);   pnl->phaseLabel.setVisible (showSynth);
        pnl->unisonSlider.setVisible       (showSynth);   pnl->unisonLabel.setVisible       (showSynth);
        pnl->unisonDetuneSlider.setVisible (showSynth);   pnl->unisonDetuneLabel.setVisible (showSynth);
        pnl->titleLabel.setVisible  (showSynth);
        pnl->oscOnBtn.setVisible    (showSynth);
        pnl->openBtn.setVisible     (showSynth);
        pnl->analyzeBtn.setVisible  (showSynth);
        pnl->cancelBtn.setVisible   (showSynth && analysisOverlay[i]);
    }

    // FX elements
    auto showFxModule = [&] (FxModule* m, bool v)
    {
        if (!m) return;
        m->enableBtn.setVisible (v);
        m->nameLabel.setVisible (v);
        for (auto* k : m->knobs)       k->setVisible (v);
        for (auto* l : m->knobLabels)  l->setVisible (v);
        if (m->hasModeBox) m->modeBox.setVisible (v);
    };

    showFxModule (fxHyper.get(),   showFx);
    showFxModule (fxDist.get(),    showFx);
    showFxModule (fxChorus.get(),  showFx);
    showFxModule (fxPhaser.get(),  showFx);
    showFxModule (fxFlanger.get(), showFx);
    showFxModule (fxReverb.get(),  showFx);

    if (showSynth)
        layoutSynthTab();
    if (showFx)
        layoutFxTab();
}

//==============================================================================
void SynthifyEditor::layoutSynthTab()
{
    // ── Oscillator area ────────────────────────────────────────────────────────
    auto oscArea = juce::Rectangle<int> (12, 68, kW - 24, 296);
    oscArea.removeFromTop (4);

    int colW = oscArea.getWidth() / NUM_OSC;

    for (int i = 0; i < NUM_OSC; i++)
    {
        auto& pnl = oscPanels[(size_t)i];
        auto  col = oscArea.removeFromLeft (colW).reduced (4, 2);

        // Title + ON toggle side by side
        auto titleRow = col.removeFromTop (18);
        pnl->oscOnBtn.setBounds   (titleRow.removeFromRight (34).reduced (2, 2));
        pnl->titleLabel.setBounds (titleRow);

        // Waveform display
        pnl->waveform.setBounds (col.removeFromTop (110));

        // WT position label + slider
        pnl->wtLabel.setBounds  (col.removeFromTop (11));
        pnl->wtSlider.setBounds (col.removeFromTop (16).reduced (2, 2));

        // Knobs — two rows of 3 then 2
        col.removeFromTop (4);
        auto knobRow1 = col.removeFromTop (82);
        int kw3 = knobRow1.getWidth() / 3;

        auto placeKnob = [&] (juce::Rectangle<int>& row, juce::Slider& s, juce::Label& l, int w)
        {
            auto cell = row.removeFromLeft (w);
            l.setBounds (cell.removeFromTop (13));
            s.setBounds (cell);
        };

        placeKnob (knobRow1, pnl->volSlider,   pnl->volLabel,   kw3);
        placeKnob (knobRow1, pnl->panSlider,   pnl->panLabel,   kw3);
        placeKnob (knobRow1, pnl->pitchSlider, pnl->pitchLabel, kw3);

        auto knobRow2 = col.removeFromTop (82);
        int kw4 = knobRow2.getWidth() / 4;
        placeKnob (knobRow2, pnl->detuneSlider,       pnl->detuneLabel,       kw4);
        placeKnob (knobRow2, pnl->phaseSlider,        pnl->phaseLabel,        kw4);
        placeKnob (knobRow2, pnl->unisonSlider,       pnl->unisonLabel,       kw4);
        placeKnob (knobRow2, pnl->unisonDetuneSlider, pnl->unisonDetuneLabel, kw4);

        // Buttons
        auto btnRow = col.removeFromTop (22);
        int  bw     = btnRow.getWidth() / 3;
        pnl->openBtn.setBounds    (btnRow.removeFromLeft (bw).reduced (2, 1));
        pnl->analyzeBtn.setBounds (btnRow.removeFromLeft (bw).reduced (2, 1));
        pnl->cancelBtn.setBounds  (btnRow.reduced (2, 1));
    }

    // ── Envelope section ──────────────────────────────────────────────────────
    auto envSection = juce::Rectangle<int> (12, 376, 376, 290);
    envSection.removeFromTop (16);

    adsrViz.setBounds (envSection.removeFromTop (100));
    envSection.removeFromTop (8);

    auto adsrRow = envSection.removeFromTop (90);
    int  kw4     = adsrRow.getWidth() / 4;

    auto placeKnob4 = [&] (juce::Slider& s, juce::Label& l)
    {
        auto cell = adsrRow.removeFromLeft (kw4);
        l.setBounds (cell.removeFromTop (13));
        s.setBounds (cell);
    };
    placeKnob4 (attackSlider,  attackLabel);
    placeKnob4 (decaySlider,   decayLabel);
    placeKnob4 (sustainSlider, sustainLabel);
    {
        auto cell = adsrRow;
        releaseLabel.setBounds  (cell.removeFromTop (13));
        releaseSlider.setBounds (cell);
    }

    // ── Global section ────────────────────────────────────────────────────────
    auto globSection = juce::Rectangle<int> (400, 376, kW - 412, 290);
    globSection.removeFromTop (16);

    auto toggleRow = globSection.removeFromTop (26);
    monoBtn.setBounds   (toggleRow.removeFromLeft (60).reduced (2, 2));
    legatoBtn.setBounds (toggleRow.removeFromLeft (68).reduced (2, 2));

    globSection.removeFromTop (4);

    // 5 knobs row
    auto knobRow = globSection.removeFromTop (90);
    int  kw5     = knobRow.getWidth() / 5;

    auto placeGlobal = [&] (juce::Slider& s, juce::Label& l)
    {
        auto cell = knobRow.removeFromLeft (kw5);
        l.setBounds (cell.removeFromTop (13));
        s.setBounds (cell);
    };
    placeGlobal (portaSlider,  portaLabel);
    placeGlobal (gainSlider,   gainLabel);
    placeGlobal (widthSlider,  widthLabel);
    placeGlobal (cutoffSlider, cutoffLabel);
    {
        auto cell = knobRow;
        resLabel.setBounds   (cell.removeFromTop (13));
        resSlider.setBounds  (cell);
    }
}

//==============================================================================
void SynthifyEditor::layoutFxTab()
{
    // Vertical stack of FX modules, each 90px tall with knobs inside
    // Left column: module name + enable toggle + mode box
    // Right area: knobs

    int startY = 62;
    int moduleH = 95;
    int padX    = 16;
    int innerW  = kW - 32;

    auto layoutModule = [&] (FxModule* m, int y)
    {
        if (!m) return;

        auto row = juce::Rectangle<int> (padX, y, innerW, moduleH - 4);

        // Enable toggle (left)
        m->enableBtn.setBounds (row.removeFromLeft (38).reduced (2, 20));

        // Name label
        m->nameLabel.setBounds (row.removeFromLeft (130).reduced (4, 28));

        // Mode box if applicable
        if (m->hasModeBox)
            m->modeBox.setBounds (row.removeFromLeft (110).reduced (4, 28));
        else
            row.removeFromLeft (4);

        // Remaining space for knobs
        int numKnobs = m->knobs.size();
        if (numKnobs == 0) return;

        int kw = row.getWidth() / numKnobs;
        for (int i = 0; i < numKnobs; ++i)
        {
            auto cell = row.removeFromLeft (kw);
            m->knobLabels[i]->setBounds (cell.removeFromTop (13));
            m->knobs[i]->setBounds (cell.reduced (2, 0));
        }
    };

    layoutModule (fxHyper.get(),   startY + moduleH * 0);
    layoutModule (fxDist.get(),    startY + moduleH * 1);
    layoutModule (fxChorus.get(),  startY + moduleH * 2);
    layoutModule (fxPhaser.get(),  startY + moduleH * 3);
    layoutModule (fxFlanger.get(), startY + moduleH * 4);
    layoutModule (fxReverb.get(),  startY + moduleH * 5);
}

//==============================================================================
void SynthifyEditor::openFileForOsc (int oscIndex)
{
    activeFileOsc = oscIndex;
    fileChooser = std::make_unique<juce::FileChooser> (
        "Open Sample — OSC " + juce::String (oscIndex + 1),
        juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            auto f = chooser.getResult();
            if (f.existsAsFile())
            {
                if (!proc.loadSampleFile (activeFileOsc, f))
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::WarningIcon, "Synthify",
                        "Could not open: " + f.getFileName());
            }
        });
}

void SynthifyEditor::buttonClicked (juce::Button* b)
{
    if (b == &synthTabBtn)
    {
        currentTab = 0;
        resized();
        repaint();
        return;
    }
    if (b == &fxTabBtn)
    {
        currentTab = 1;
        resized();
        repaint();
        return;
    }

    for (int i = 0; i < NUM_OSC; i++)
    {
        auto& pnl = oscPanels[(size_t)i];
        if (b == &pnl->openBtn)    { openFileForOsc (i); return; }
        if (b == &pnl->analyzeBtn) { proc.startAnalysis (i); return; }
        if (b == &pnl->cancelBtn)  { proc.cancelAnalysis (i); return; }
    }
}

//==============================================================================
void SynthifyEditor::analysisStarted (int oscIndex)
{
    analysisOverlay[oscIndex] = true;
    if (currentTab == 0)
        oscPanels[(size_t)oscIndex]->cancelBtn.setVisible (true);
    statusLabel.setText ("Scanning OSC " + juce::String (oscIndex + 1) + "...",
                         juce::dontSendNotification);
    repaint();
}

void SynthifyEditor::analysisFinished (int oscIndex)
{
    analysisOverlay[oscIndex] = false;
    if (currentTab == 0)
        oscPanels[(size_t)oscIndex]->cancelBtn.setVisible (false);
    statusLabel.setText ("OSC " + juce::String (oscIndex + 1) + " tuned to C4",
                         juce::dontSendNotification);
    repaint();
}

void SynthifyEditor::analysisCancelled (int oscIndex)
{
    analysisOverlay[oscIndex] = false;
    if (currentTab == 0)
        oscPanels[(size_t)oscIndex]->cancelBtn.setVisible (false);
    statusLabel.setText ("OSC " + juce::String (oscIndex + 1) + " scan cancelled",
                         juce::dontSendNotification);
    repaint();
}

void SynthifyEditor::sampleLoaded (int oscIndex)
{
    if (currentTab == 0)
        oscPanels[(size_t)oscIndex]->waveform.repaint();
    statusLabel.setText ("OSC " + juce::String (oscIndex + 1) + " sample loaded",
                         juce::dontSendNotification);
}
