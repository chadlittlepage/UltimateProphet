#include "PluginEditor.h"

// Ableton Live QWERTY layout
int UltimateProphetEditor::getNoteForKey(int keyCode) const
{
    switch (keyCode)
    {
        case 'a': case 'A': return 0;   case 'w': case 'W': return 1;
        case 's': case 'S': return 2;   case 'e': case 'E': return 3;
        case 'd': case 'D': return 4;   case 'f': case 'F': return 5;
        case 't': case 'T': return 6;   case 'g': case 'G': return 7;
        case 'y': case 'Y': return 8;   case 'h': case 'H': return 9;
        case 'u': case 'U': return 10;  case 'j': case 'J': return 11;
        case 'k': case 'K': return 12;  case 'o': case 'O': return 13;
        case 'l': case 'L': return 14;  case 'p': case 'P': return 15;
        case ';':            return 16;
        default: return -1;
    }
}

void UltimateProphetEditor::sendNoteOn(int n)
{
    n = juce::jlimit(0, 127, n);
    processorRef.addKeyboardMidi(juce::MidiMessage::noteOn(1, n, keyVelocity));
}

void UltimateProphetEditor::sendNoteOff(int n)
{
    n = juce::jlimit(0, 127, n);
    processorRef.addKeyboardMidi(juce::MidiMessage::noteOff(1, n, 0.0f));
}

// --- Setup helpers ---
void UltimateProphetEditor::setupKnob(Knob& k, const juce::String& id, const juce::String& name)
{
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(k.slider);
    k.label.setText(name, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    k.label.setFont(juce::Font(9.0f));
    addAndMakeVisible(k.label);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), id, k.slider);
}

void UltimateProphetEditor::setupToggle(Toggle& t, const juce::String& id, const juce::String& name)
{
    t.button.setButtonText(name);
    addAndMakeVisible(t.button);
    t.att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.getAPVTS(), id, t.button);
}

void UltimateProphetEditor::setupCombo(Combo& c, const juce::String& id, const juce::String& name,
                                        const juce::StringArray& items)
{
    c.box.addItemList(items, 1);
    addAndMakeVisible(c.box);
    c.label.setText(name, juce::dontSendNotification);
    c.label.setJustificationType(juce::Justification::centred);
    c.label.setFont(juce::Font(9.0f));
    addAndMakeVisible(c.label);
    c.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getAPVTS(), id, c.box);
}

// --- Constructor ---
UltimateProphetEditor::UltimateProphetEditor(UltimateProphetProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), consolePanel(p.debugConsole)
{
    setLookAndFeel(&prophetLnf);
    setWantsKeyboardFocus(true);

    // Poly-Mod
    setupKnob(pmFiltEnv, "pmodFiltEnv", "Filt Env");
    setupKnob(pmOscB, "pmodOscB", "Osc B");
    setupToggle(pmFreqA, "pmodToFreqA", "Freq A");
    setupToggle(pmPWA, "pmodToPWA", "PW A");
    setupToggle(pmFilter, "pmodToFilter", "Filter");

    // LFO
    setupKnob(lfoFreq, "lfoFreq", "Freq");
    setupKnob(lfoAmount, "lfoAmount", "Amount");
    setupKnob(lfoSrcMix, "lfoSrcMix", "Src Mix");
    setupToggle(lfoSaw, "lfoSaw", "Saw");
    setupToggle(lfoTri, "lfoTri", "Tri");
    setupToggle(lfoSqr, "lfoSquare", "Sqr");
    setupToggle(lfoToFreqA, "lfoToFreqA", "FreqA");
    setupToggle(lfoToFreqB, "lfoToFreqB", "FreqB");
    setupToggle(lfoToPWA, "lfoToPWA", "PW A");
    setupToggle(lfoToPWB, "lfoToPWB", "PW B");
    setupToggle(lfoToFilter, "lfoToFilter", "Filt");

    // Oscillator A
    setupKnob(oscAFreq, "oscAFreq", "Freq");
    setupKnob(oscAPW, "oscAPW", "PW");
    setupToggle(oscASaw, "oscASaw", "Saw");
    setupToggle(oscAPulse, "oscAPulse", "Pulse");

    // Oscillator B
    setupKnob(oscBFreq, "oscBFreq", "Freq");
    setupKnob(oscBFine, "oscBFineTune", "Fine");
    setupKnob(oscBPW, "oscBPW", "PW");
    setupToggle(oscBSaw, "oscBSaw", "Saw");
    setupToggle(oscBTri, "oscBTri", "Tri");
    setupToggle(oscBPulse, "oscBPulse", "Pulse");
    setupToggle(oscBLowFreq, "oscBLowFreq", "Low");
    setupToggle(oscBKbd, "oscBKbd", "Kbd");
    setupToggle(oscSync, "oscSync", "Sync");

    // Mixer
    setupKnob(mixA, "mixOscA", "Osc A");
    setupKnob(mixB, "mixOscB", "Osc B");
    setupKnob(mixNoise, "mixNoise", "Noise");

    // Filter
    setupKnob(filtCutoff, "filterCutoff", "Cutoff");
    setupKnob(filtReso, "filterReso", "Reso");
    setupKnob(filtEnvAmt, "filterEnvAmt", "Env Amt");
    setupCombo(filtKeyTrack, "filterKeyTrack", "Key Trk", {"Off", "Half", "Full"});
    setupCombo(filtRev, "filterRev", "Rev", {"1/2", "3"});

    // Filter Envelope
    setupKnob(filtAtk, "filtAtk", "A");
    setupKnob(filtDec, "filtDec", "D");
    setupKnob(filtSus, "filtSus", "S");
    setupKnob(filtRel, "filtRel", "R");

    // Amp Envelope
    setupKnob(ampAtk, "ampAtk", "A");
    setupKnob(ampDec, "ampDec", "D");
    setupKnob(ampSus, "ampSus", "S");
    setupKnob(ampRel, "ampRel", "R");

    // Performance
    setupKnob(glideRate, "glideRate", "Glide");
    setupToggle(glideOn, "glideOn", "Glide");
    setupKnob(vintage, "vintage", "Vintage");
    setupKnob(masterVol, "masterVol", "Volume");
    setupKnob(masterTune, "masterTune", "M.Tune");
    setupKnob(pitchRange, "pitchWheelRange", "PB Rng");
    setupToggle(unisonOn, "unisonOn", "Unison");
    setupKnob(unisonVoices, "unisonVoices", "Uni Vox");
    setupKnob(unisonDetune, "unisonDetune", "Uni Det");
    setupToggle(velFilt, "velToFilter", "V>Filt");
    setupToggle(velAmp, "velToAmp", "V>Amp");
    setupToggle(releaseSwitch, "releaseSwitch", "Release");
    setupToggle(atFilter, "atToFilter", "AT>Filt");
    setupToggle(atLFO, "atToLFO", "AT>LFO");
    setupCombo(keyPriority, "keyPriority", "Key Pri", {"Lo", "LoR", "Last", "LaR"});

    chordMemBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2A2A2E));
    chordMemBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffD4A843));
    chordMemBtn.onClick = [this] {
        if (processorRef.isChordMemoryActive())
        {
            processorRef.clearChordMemory();
            chordMemBtn.setButtonText("Chord");
        }
        else
        {
            // Capture from processor's active voices — NOT heldKeys,
            // because clicking this button steals keyboard focus and
            // releases all QWERTY keys before onClick fires.
            std::vector<int> notes;
            for (int i = 0; i < UltimateProphetProcessor::NUM_VOICES; ++i)
            {
                int note = processorRef.debugConsole.voiceStats[i].note.load();
                bool active = processorRef.debugConsole.voiceStats[i].active.load();
                if (active && note >= 0)
                    notes.push_back(note);
            }
            if (!notes.empty())
            {
                processorRef.storeChordMemory(notes);
                chordMemBtn.setButtonText("Chord*");
            }
        }
    };
    addAndMakeVisible(chordMemBtn);

    // Init button
    initButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2A2A2E));
    initButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffD4A843));
    initButton.onClick = [this] {
        processorRef.loadBasicPreset();
        patchNameLabel.setText("INIT", juce::dontSendNotification);
    };
    addAndMakeVisible(initButton);

    // Patch browser
    loadSyxButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2A2A2E));
    loadSyxButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffD4A843));
    loadSyxButton.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load Prophet-5 SysEx", juce::File{}, "*.syx");
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile()) {
                    processorRef.loadSysExFile(file);
                    updatePatchLabel();
                }
            });
    };
    addAndMakeVisible(loadSyxButton);

    prevPatchButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2A2A2E));
    prevPatchButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffD4A843));
    prevPatchButton.onClick = [this] { prevPatch(); };
    addAndMakeVisible(prevPatchButton);

    nextPatchButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2A2A2E));
    nextPatchButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffD4A843));
    nextPatchButton.onClick = [this] { nextPatch(); };
    addAndMakeVisible(nextPatchButton);

    patchNameLabel.setText("No patches loaded", juce::dontSendNotification);
    patchNameLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, 0));
    patchNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff40FF40));
    patchNameLabel.setJustificationType(juce::Justification::centredLeft);
    patchNameLabel.setInterceptsMouseClicks(true, false);
    patchNameLabel.addMouseListener(this, false);
    addAndMakeVisible(patchNameLabel);

    // Status label
    statusLabel.setText("Oct: " + juce::String(keyboardOctave)
                      + "  Vel: " + juce::String(juce::roundToInt(keyVelocity * 127.0f)),
                      juce::dontSendNotification);
    statusLabel.setFont(juce::Font(10.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffD4A843));
    addAndMakeVisible(statusLabel);

    addAndMakeVisible(consolePanel);
    startTimerHz(30);

    // Enable resizable — aspect ratio is managed in resized()
    constrainer.setMinimumSize(DEFAULT_W / 2, PANEL_H / 2 + DebugConsolePanel::COLLAPSED_HEIGHT);
    constrainer.setMaximumSize(DEFAULT_W * 2, PANEL_H * 2 + DebugConsolePanel::EXPANDED_HEIGHT);
    setConstrainer(&constrainer);
    setResizable(true, true);  // resizable with corner dragger
    setSize(DEFAULT_W, DEFAULT_H);

    // Refresh patch display (factory patches may have auto-loaded)
    updatePatchLabel();
}

UltimateProphetEditor::~UltimateProphetEditor()
{
    if (auto* topLevel = getTopLevelComponent())
        topLevel->removeKeyListener(this);
    stopTimer();
    setLookAndFeel(nullptr);
}

// --- Keyboard ---
bool UltimateProphetEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    int jk = key.getKeyCode();
    int tc = key.getTextCharacter();

    if (tc == 'z' || tc == 'Z') {
        keyboardOctave = juce::jmax(0, keyboardOctave - 1);
        statusLabel.setText("Oct: " + juce::String(keyboardOctave) + "  Vel: " + juce::String(juce::roundToInt(keyVelocity * 127.0f)), juce::dontSendNotification);
        return true;
    }
    if (tc == 'x' || tc == 'X') {
        keyboardOctave = juce::jmin(8, keyboardOctave + 1);
        statusLabel.setText("Oct: " + juce::String(keyboardOctave) + "  Vel: " + juce::String(juce::roundToInt(keyVelocity * 127.0f)), juce::dontSendNotification);
        return true;
    }
    if (tc == 'c' || tc == 'C') {
        int v = juce::jmax(1, juce::roundToInt(keyVelocity * 127.0f) - 20);
        keyVelocity = v / 127.0f;
        statusLabel.setText("Oct: " + juce::String(keyboardOctave) + "  Vel: " + juce::String(v), juce::dontSendNotification);
        return true;
    }
    if (tc == 'v' || tc == 'V') {
        int v = juce::jmin(127, juce::roundToInt(keyVelocity * 127.0f) + 20);
        keyVelocity = v / 127.0f;
        statusLabel.setText("Oct: " + juce::String(keyboardOctave) + "  Vel: " + juce::String(v), juce::dontSendNotification);
        return true;
    }
    if (tc == '`') { consolePanel.toggleVisibility(); return true; }

    // Arrow keys: left/right = prev/next patch
    if (jk == juce::KeyPress::leftKey)  { prevPatch(); return true; }
    if (jk == juce::KeyPress::rightKey) { nextPatch(); return true; }

    int noteOffset = getNoteForKey(tc);
    if (noteOffset >= 0) {
        if (heldKeys.find(jk) == heldKeys.end()) {
            int midiNote = juce::jlimit(0, 127, (keyboardOctave * 12) + noteOffset);
            heldKeys[jk] = midiNote;
            sendNoteOn(midiNote);
        }
        return true;
    }
    return false;
}

bool UltimateProphetEditor::keyStateChanged(bool, juce::Component*)
{
    std::vector<int> released;
    for (auto& [jk, mn] : heldKeys)
        if (!juce::KeyPress::isKeyCurrentlyDown(jk))
            released.push_back(jk);
    for (int jk : released) {
        sendNoteOff(heldKeys[jk]);
        heldKeys.erase(jk);
    }
    return !released.empty();
}

void UltimateProphetEditor::timerCallback() { keyStateChanged(false, nullptr); }

void UltimateProphetEditor::nextPatch()
{
    int idx = processorRef.currentPatchIndex + 1;
    if (idx < processorRef.getNumLoadedPatches())
    {
        processorRef.selectPatch(idx);
        updatePatchLabel();
    }
}

void UltimateProphetEditor::prevPatch()
{
    int idx = processorRef.currentPatchIndex - 1;
    if (idx >= 0)
    {
        processorRef.selectPatch(idx);
        updatePatchLabel();
    }
}

void UltimateProphetEditor::showPatchBrowser()
{
    int total = processorRef.getNumLoadedPatches();
    if (total <= 0) return;

    auto menu = juce::PopupMenu();

    // Group patches into banks of 40 (matching Prophet-5 organization)
    for (int bank = 0; bank * 40 < total; ++bank)
    {
        auto bankMenu = juce::PopupMenu();
        int start = bank * 40;
        int end = juce::jmin(start + 40, total);

        for (int i = start; i < end; ++i)
        {
            juce::String label = juce::String(i + 1) + ". " + processorRef.getPatchName(i);
            bool isCurrent = (i == processorRef.currentPatchIndex);
            bankMenu.addItem(i + 1, label, true, isCurrent);
        }

        juce::String bankName = "Bank " + juce::String(bank + 1)
                              + " (" + juce::String(start + 1) + "-" + juce::String(end) + ")";
        menu.addSubMenu(bankName, bankMenu);
    }

    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(&patchNameLabel)
        .withMinimumWidth(300),
        [this](int result) {
            if (result > 0)
            {
                processorRef.selectPatch(result - 1);
                updatePatchLabel();
            }
        });
}

void UltimateProphetEditor::updatePatchLabel()
{
    int idx = processorRef.currentPatchIndex;
    int total = processorRef.getNumLoadedPatches();
    if (idx >= 0 && total > 0)
        patchNameLabel.setText(juce::String(idx + 1) + "/" + juce::String(total)
                             + ": " + processorRef.getPatchName(idx),
                             juce::dontSendNotification);
    else
        patchNameLabel.setText("No patches loaded", juce::dontSendNotification);
}

void UltimateProphetEditor::mouseDown(const juce::MouseEvent& e)
{
    // Click on patch name opens browser
    if (e.eventComponent == &patchNameLabel)
    {
        showPatchBrowser();
        return;
    }

    grabKeyboardFocus();
    AudioProcessorEditor::mouseDown(e);
}

void UltimateProphetEditor::parentHierarchyChanged()
{
    if (auto* topLevel = getTopLevelComponent())
        topLevel->addKeyListener(this);
    grabKeyboardFocus();
}

// --- Paint ---
void UltimateProphetEditor::paintSection(juce::Graphics& g, int x, int y, int w, int h,
                                          const juce::String& title)
{
    g.setColour(juce::Colour(0xff121216));
    g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 3.0f);
    g.setColour(juce::Colour(0xffD4A843));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText(title, x + 6, y + 2, w - 12, 14, juce::Justification::left);
}

void UltimateProphetEditor::paint(juce::Graphics& g)
{
    // Scale the graphics context to match child transforms
    g.addTransform(juce::AffineTransform::scale(scaleFactor));

    g.fillAll(juce::Colour(0xff1A1A1E));

    // Wood panels
    auto wood = juce::Colour(0xff2C1810);
    g.setColour(wood);
    g.fillRect(0, 0, WOOD, SYNTH_H);
    g.fillRect(DEFAULT_W - WOOD, 0, WOOD, SYNTH_H);
    g.setColour(wood.brighter(0.06f));
    for (int yy = 0; yy < SYNTH_H; yy += 5)
    {
        g.drawHorizontalLine(yy, 2.0f, (float)(WOOD - 2));
        g.drawHorizontalLine(yy, (float)(DEFAULT_W - WOOD + 2), (float)(DEFAULT_W - 2));
    }

    int px = WOOD + 4;
    int pw = DEFAULT_W - 2 * (WOOD + 4);

    // Header
    g.setColour(juce::Colour(0xffD4A843));
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText("ULTIMATE PROPHET", px, 2, 200, 20, juce::Justification::left);
    g.drawText("Kings of Confetti", px, 2, pw, 20, juce::Justification::right);

    // Center LCD display background (like real Prophet-5)
    int lcdX = px + (pw - 360) / 2;
    g.setColour(juce::Colour(0xff0A1408));
    g.fillRoundedRectangle((float)lcdX, 2.0f, 360.0f, 20.0f, 3.0f);
    g.setColour(juce::Colour(0xff1A2A18));
    g.drawRoundedRectangle((float)lcdX, 2.0f, 360.0f, 20.0f, 3.0f, 1.0f);

    // Section boxes (matching Prophet-5 panel order left to right)
    int sy = 24;
    int sh1 = 260;  // taller to fit 3 rows of knobs + value text
    paintSection(g, px,       sy, 140, sh1, "POLY-MOD");
    paintSection(g, px + 144, sy, 170, sh1, "LFO");
    paintSection(g, px + 318, sy, 148, sh1, "OSCILLATOR A");
    paintSection(g, px + 470, sy, 196, sh1, "OSCILLATOR B");
    paintSection(g, px + 670, sy, 100, sh1, "MIXER");
    paintSection(g, px + 774, sy, 158, sh1, "FILTER");
    paintSection(g, px + 936, sy, pw - 936, sh1, "MASTER");

    int sy2 = sy + sh1 + 4;
    int sh2 = 204;
    paintSection(g, px,        sy2, 280, sh2, "FILTER ENVELOPE");
    paintSection(g, px + 284,  sy2, 280, sh2, "AMPLIFIER ENVELOPE");
    paintSection(g, px + 568,  sy2, pw - 568, sh2, "PERFORMANCE");

    // Keyboard hint
    g.setColour(juce::Colour(0xff4A4A50));
    g.setFont(juce::Font(9.0f));
    g.drawText("A-; = white | W/E/T/Y/U/O/P = black | Z/X = oct | C/V = vel | ` = console",
               px, SYNTH_H - 16, pw, 14, juce::Justification::centred);
}

void UltimateProphetEditor::resized()
{
    // Scale factor based on width
    scaleFactor = static_cast<float>(getWidth()) / static_cast<float>(DEFAULT_W);
    int scaledPanelH = static_cast<int>(PANEL_H * scaleFactor);

    // Console goes BELOW the synth panel, not over it
    int consoleH = consolePanel.isConsoleVisible()
                 ? DebugConsolePanel::EXPANDED_HEIGHT
                 : DebugConsolePanel::COLLAPSED_HEIGHT;
    int totalH = scaledPanelH + consoleH;

    // Resize window to fit panel + console
    if (getHeight() != totalH)
    {
        constrainer.setFixedAspectRatio(0);  // temporarily disable aspect ratio
        setSize(getWidth(), totalH);
        constrainer.setFixedAspectRatio(static_cast<double>(DEFAULT_W) / PANEL_H);
        return;  // resized() will be called again with the new size
    }

    // Apply transform to synth panel children
    auto transform = juce::AffineTransform::scale(scaleFactor);
    for (auto* child : getChildren())
    {
        if (child != &consolePanel)
            child->setTransform(transform);
    }

    // Console: full width, below the scaled panel, no transform
    consolePanel.setTransform({});
    consolePanel.setBounds(0, scaledPanelH, getWidth(), consoleH);

    // Layout everything at DEFAULT coordinates (transform handles scaling)
    int px = WOOD + 10;
    int row1 = 42;

    auto placeKnob = [&](Knob& k, int x, int y) {
        k.label.setBounds(x, y, KW, 12);
        k.slider.setBounds(x, y + 12, KW - 4, KW);
    };
    auto placeToggle = [&](Toggle& t, int x, int y) {
        t.button.setBounds(x, y, 64, TH);
    };

    // ===== POLY-MOD =====
    int sx = px;
    placeKnob(pmFiltEnv, sx, row1);
    placeKnob(pmOscB, sx + KW, row1);
    placeToggle(pmFreqA, sx, row1 + KH + 6);
    placeToggle(pmPWA, sx, row1 + KH + TH + 10);
    placeToggle(pmFilter, sx, row1 + KH + 2 * TH + 14);

    // ===== LFO =====
    sx = px + 150;
    placeKnob(lfoFreq, sx, row1);
    placeKnob(lfoAmount, sx + KW, row1);
    int lfoR2 = row1 + KH + 2;
    placeKnob(lfoSrcMix, sx, lfoR2);
    placeToggle(lfoSaw, sx + KW + 4, lfoR2);
    placeToggle(lfoTri, sx + KW + 4, lfoR2 + TH + 2);
    placeToggle(lfoSqr, sx + KW + 4, lfoR2 + 2 * (TH + 2));
    int lfoR3 = lfoR2 + KH + 2;
    placeToggle(lfoToFreqA, sx, lfoR3);
    placeToggle(lfoToFreqB, sx + 68, lfoR3);
    placeToggle(lfoToPWA, sx, lfoR3 + TH + 2);
    placeToggle(lfoToPWB, sx + 68, lfoR3 + TH + 2);
    placeToggle(lfoToFilter, sx, lfoR3 + 2 * (TH + 2));

    // ===== OSCILLATOR A =====
    sx = px + 324;
    placeKnob(oscAFreq, sx, row1);
    placeKnob(oscAPW, sx + KW, row1);
    placeToggle(oscASaw, sx, row1 + KH + 6);
    placeToggle(oscAPulse, sx, row1 + KH + TH + 10);

    // ===== OSCILLATOR B =====
    sx = px + 476;
    placeKnob(oscBFreq, sx, row1);
    placeKnob(oscBFine, sx + KW, row1);
    placeKnob(oscBPW, sx + KW * 2, row1);
    placeToggle(oscBSaw, sx, row1 + KH + 6);
    placeToggle(oscBTri, sx + 70, row1 + KH + 6);
    placeToggle(oscBPulse, sx, row1 + KH + TH + 10);
    placeToggle(oscBLowFreq, sx + 70, row1 + KH + TH + 10);
    placeToggle(oscBKbd, sx, row1 + KH + 2 * TH + 14);
    placeToggle(oscSync, sx + 70, row1 + KH + 2 * TH + 14);

    // ===== MIXER =====
    sx = px + 676;
    placeKnob(mixA, sx, row1);
    placeKnob(mixB, sx, row1 + KH + 4);
    placeKnob(mixNoise, sx, row1 + 2 * (KH + 4));

    // ===== FILTER =====
    sx = px + 780;
    placeKnob(filtCutoff, sx, row1);
    placeKnob(filtReso, sx + KW, row1);
    placeKnob(filtEnvAmt, sx, row1 + KH + 4);
    filtKeyTrack.label.setBounds(sx, row1 + 2 * KH + 12, KW, 12);
    filtKeyTrack.box.setBounds(sx, row1 + 2 * KH + 24, KW - 4, 22);
    filtRev.label.setBounds(sx + KW, row1 + 2 * KH + 12, KW, 12);
    filtRev.box.setBounds(sx + KW, row1 + 2 * KH + 24, KW - 4, 22);

    // ===== MASTER =====
    sx = px + 942;
    placeKnob(masterVol, sx, row1);
    placeKnob(vintage, sx, row1 + KH + 4);
    placeKnob(pitchRange, sx, row1 + 2 * (KH + 4));

    // ===== ROW 2: ENVELOPES + PERFORMANCE =====
    int row2 = 24 + 260 + 4 + 18;  // sy + sh1 + gap + header

    // Filter Envelope
    sx = px;
    placeKnob(filtAtk, sx, row2);
    placeKnob(filtDec, sx + KW, row2);
    placeKnob(filtSus, sx + KW * 2, row2);
    placeKnob(filtRel, sx + KW * 3, row2);

    // Amp Envelope
    sx = px + 284;
    placeKnob(ampAtk, sx, row2);
    placeKnob(ampDec, sx + KW, row2);
    placeKnob(ampSus, sx + KW * 2, row2);
    placeKnob(ampRel, sx + KW * 3, row2);

    // Performance
    sx = px + 568;
    placeKnob(glideRate, sx, row2);
    placeKnob(masterTune, sx + KW, row2);
    placeKnob(unisonVoices, sx + KW * 2, row2);
    placeKnob(unisonDetune, sx + KW * 3, row2);

    int perfTogY = row2 + KH + 4;
    placeToggle(glideOn, sx, perfTogY);
    placeToggle(unisonOn, sx + 68, perfTogY);
    placeToggle(releaseSwitch, sx + 136, perfTogY);
    placeToggle(velFilt, sx, perfTogY + TH + 4);
    placeToggle(velAmp, sx + 68, perfTogY + TH + 4);
    placeToggle(atFilter, sx + 136, perfTogY + TH + 4);
    placeToggle(atLFO, sx + 204, perfTogY + TH + 4);

    keyPriority.label.setBounds(sx, perfTogY + 2 * TH + 12, 60, 12);
    keyPriority.box.setBounds(sx, perfTogY + 2 * TH + 24, 60, 20);
    chordMemBtn.setBounds(sx + 68, perfTogY + 2 * TH + 20, 60, 24);

    statusLabel.setBounds(sx, perfTogY + 3 * TH + 30, 200, 16);

    // Patch browser: top center LCD display (at default coordinates)
    int lcdX = px + (DEFAULT_W - 2 * (WOOD + 4) - 440) / 2;
    loadSyxButton.setBounds(lcdX, 2, 52, 20);
    initButton.setBounds(lcdX + 54, 2, 34, 20);
    prevPatchButton.setBounds(lcdX + 90, 2, 24, 20);
    nextPatchButton.setBounds(lcdX + 116, 2, 24, 20);
    patchNameLabel.setBounds(lcdX + 144, 2, 292, 20);
}
