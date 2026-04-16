#include "PluginEditor.h"

// Ableton Live computer keyboard layout:
//
//  Piano:   C# D#    F# G# A#    C# D#
//           W  E     T  Y  U     O  P
//        C  D  E  F  G  A  B  C  D  E
//        A  S  D  F  G  H  J  K  L  ;
//
// Home row = white keys, row above = black keys
// Z/X = octave down/up, C/V = velocity down/up
//
// Returns semitone offset from C, or -1 if not a note key.
int UltimateProphetEditor::getNoteForKey(int keyCode) const
{
    switch (keyCode)
    {
        // Home row: white keys (first octave)
        case 'a': case 'A': return 0;   // C
        case 's': case 'S': return 2;   // D
        case 'd': case 'D': return 4;   // E
        case 'f': case 'F': return 5;   // F
        case 'g': case 'G': return 7;   // G
        case 'h': case 'H': return 9;   // A
        case 'j': case 'J': return 11;  // B

        // Home row continued: white keys (second octave)
        case 'k': case 'K': return 12;  // C+
        case 'l': case 'L': return 14;  // D+
        case ';':            return 16;  // E+

        // Upper row: black keys (first octave)
        case 'w': case 'W': return 1;   // C#
        case 'e': case 'E': return 3;   // D#
        // R skipped (gap between E and F on piano)
        case 't': case 'T': return 6;   // F#
        case 'y': case 'Y': return 8;   // G#
        case 'u': case 'U': return 10;  // A#

        // Upper row: black keys (second octave)
        // I skipped (gap between B and C on piano)
        case 'o': case 'O': return 13;  // C#+
        case 'p': case 'P': return 15;  // D#+

        default: return -1;
    }
}

void UltimateProphetEditor::sendNoteOn(int midiNote)
{
    midiNote = juce::jlimit(0, 127, midiNote);
    processorRef.addKeyboardMidi(juce::MidiMessage::noteOn(1, midiNote, keyVelocity));
}

void UltimateProphetEditor::sendNoteOff(int midiNote)
{
    midiNote = juce::jlimit(0, 127, midiNote);
    processorRef.addKeyboardMidi(juce::MidiMessage::noteOff(1, midiNote, 0.0f));
}

UltimateProphetEditor::UltimateProphetEditor(UltimateProphetProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
      consolePanel(p.debugConsole)
{
    setLookAndFeel(&prophetLnf);
    setWantsKeyboardFocus(true);

    auto& apvts = processorRef.getAPVTS();

    // Oscillators
    setupCombo(oscAWaveform, "oscAWaveform", "Osc A");
    setupCombo(oscBWaveform, "oscBWaveform", "Osc B");
    setupKnob(oscALevel, "oscALevel", "A Level");
    setupKnob(oscBLevel, "oscBLevel", "B Level");
    setupKnob(oscBDetune, "oscBDetune", "Detune");
    setupKnob(pulseWidth, "pulseWidth", "PW");

    // Mixer
    setupKnob(noiseLevel, "noiseLevel", "Noise");

    // Filter
    setupKnob(filterCutoff, "filterCutoff", "Cutoff");
    setupKnob(filterResonance, "filterResonance", "Reso");
    setupKnob(filterEnvAmount, "filterEnvAmount", "Env Amt");
    setupKnob(filterKeyTrack, "filterKeyTrack", "Key Trk");

    // Filter Envelope
    setupKnob(filterAttack, "filterAttack", "F Atk");
    setupKnob(filterDecay, "filterDecay", "F Dec");
    setupKnob(filterSustain, "filterSustain", "F Sus");
    setupKnob(filterRelease, "filterRelease", "F Rel");

    // Amp Envelope
    setupKnob(ampAttack, "ampAttack", "A Atk");
    setupKnob(ampDecay, "ampDecay", "A Dec");
    setupKnob(ampSustain, "ampSustain", "A Sus");
    setupKnob(ampRelease, "ampRelease", "A Rel");

    // Poly-Mod
    setupKnob(polyModFiltEnvOscA, "polyModFiltEnvOscA", "FE>OscA");
    setupKnob(polyModFiltEnvFilter, "polyModFiltEnvFilter", "FE>Filt");
    setupKnob(polyModOscBOscA, "polyModOscBOscA", "OB>OscA");
    setupKnob(polyModOscBFilter, "polyModOscBFilter", "OB>Filt");

    // External Input
    setupKnob(extInputLevel, "extInputLevel", "Ext In");

    // Misc
    setupKnob(analogDrift, "analogDrift", "Drift");
    setupKnob(masterVolume, "masterVolume", "Volume");

    addAndMakeVisible(oscSyncButton);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "oscSync", oscSyncButton);

    // Octave indicator
    octaveLabel.setText("Oct: " + juce::String(keyboardOctave)
                      + "  Vel: " + juce::String(juce::roundToInt(keyVelocity * 127.0f)),
                      juce::dontSendNotification);
    octaveLabel.setJustificationType(juce::Justification::centred);
    octaveLabel.setFont(juce::Font(11.0f));
    octaveLabel.setColour(juce::Label::textColourId, juce::Colour(0xffD4A843));
    addAndMakeVisible(octaveLabel);

    // Debug console
    addAndMakeVisible(consolePanel);

    // Poll for key releases
    startTimerHz(30);

    setSize(920, SYNTH_PANEL_HEIGHT + DebugConsolePanel::EXPANDED_HEIGHT);
}

UltimateProphetEditor::~UltimateProphetEditor()
{
    if (auto* topLevel = getTopLevelComponent())
        topLevel->removeKeyListener(this);

    stopTimer();
    setLookAndFeel(nullptr);
}

void UltimateProphetEditor::setupKnob(KnobWithLabel& knob,
                                       const juce::String& paramId,
                                       const juce::String& labelText)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, KNOB_SIZE, 14);
    addAndMakeVisible(knob.slider);

    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setFont(juce::Font(10.0f));
    addAndMakeVisible(knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), paramId, knob.slider);
}

void UltimateProphetEditor::setupCombo(ComboWithLabel& combo,
                                        const juce::String& paramId,
                                        const juce::String& labelText)
{
    combo.combo.addItemList({"Saw", "Pulse", "Triangle"}, 1);
    addAndMakeVisible(combo.combo);

    combo.label.setText(labelText, juce::dontSendNotification);
    combo.label.setJustificationType(juce::Justification::centred);
    combo.label.setFont(juce::Font(10.0f));
    addAndMakeVisible(combo.label);

    combo.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getAPVTS(), paramId, combo.combo);
}

bool UltimateProphetEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    int juceKeyCode = key.getKeyCode();
    int textChar = key.getTextCharacter();

    // Z = octave down (Ableton style)
    if (textChar == 'z' || textChar == 'Z')
    {
        keyboardOctave = juce::jmax(0, keyboardOctave - 1);
        octaveLabel.setText("Oct: " + juce::String(keyboardOctave)
                          + "  Vel: " + juce::String(juce::roundToInt(keyVelocity * 127.0f)),
                          juce::dontSendNotification);
        return true;
    }
    // X = octave up (Ableton style)
    if (textChar == 'x' || textChar == 'X')
    {
        keyboardOctave = juce::jmin(8, keyboardOctave + 1);
        octaveLabel.setText("Oct: " + juce::String(keyboardOctave)
                          + "  Vel: " + juce::String(juce::roundToInt(keyVelocity * 127.0f)),
                          juce::dontSendNotification);
        return true;
    }
    // C = velocity down by 20 (Ableton style)
    if (textChar == 'c' || textChar == 'C')
    {
        int vel = juce::jmax(1, juce::roundToInt(keyVelocity * 127.0f) - 20);
        keyVelocity = static_cast<float>(vel) / 127.0f;
        octaveLabel.setText("Oct: " + juce::String(keyboardOctave)
                          + "  Vel: " + juce::String(vel),
                          juce::dontSendNotification);
        return true;
    }
    // V = velocity up by 20 (Ableton style)
    if (textChar == 'v' || textChar == 'V')
    {
        int vel = juce::jmin(127, juce::roundToInt(keyVelocity * 127.0f) + 20);
        keyVelocity = static_cast<float>(vel) / 127.0f;
        octaveLabel.setText("Oct: " + juce::String(keyboardOctave)
                          + "  Vel: " + juce::String(vel),
                          juce::dontSendNotification);
        return true;
    }

    // Toggle console with backtick
    if (textChar == '`')
    {
        consolePanel.toggleVisibility();
        return true;
    }

    int noteOffset = getNoteForKey(textChar);
    if (noteOffset >= 0)
    {
        // Only trigger if this physical key isn't already held
        if (heldKeys.find(juceKeyCode) == heldKeys.end())
        {
            int midiNote = juce::jlimit(0, 127, (keyboardOctave * 12) + noteOffset);
            heldKeys[juceKeyCode] = midiNote;
            sendNoteOn(midiNote);
        }
        return true;  // Always consume note keys (swallows OS key repeat)
    }

    return false;
}

bool UltimateProphetEditor::keyStateChanged(bool /*isKeyDown*/, juce::Component*)
{
    // Check for released keys using JUCE key codes (not text chars)
    std::vector<int> released;
    for (auto& [juceKeyCode, midiNote] : heldKeys)
    {
        if (!juce::KeyPress::isKeyCurrentlyDown(juceKeyCode))
            released.push_back(juceKeyCode);
    }

    for (int juceKeyCode : released)
    {
        int midiNote = heldKeys[juceKeyCode];
        heldKeys.erase(juceKeyCode);
        sendNoteOff(midiNote);
    }

    return !released.empty();
}

void UltimateProphetEditor::timerCallback()
{
    // Periodic check for stuck keys (backup for keyStateChanged)
    keyStateChanged(false, nullptr);
}

void UltimateProphetEditor::mouseDown(const juce::MouseEvent& e)
{
    // Click anywhere on the background to grab keyboard focus for QWERTY playing
    grabKeyboardFocus();
    AudioProcessorEditor::mouseDown(e);
}

void UltimateProphetEditor::parentHierarchyChanged()
{
    // Register as key listener on the top-level component so we catch
    // key events even when a knob or combo has focus
    if (auto* topLevel = getTopLevelComponent())
        topLevel->addKeyListener(this);

    grabKeyboardFocus();
}

void UltimateProphetEditor::paintWoodPanel(juce::Graphics& g, int x, int y, int w, int h)
{
    auto woodBase = juce::Colour(0xff2C1810);
    g.setColour(woodBase);
    g.fillRect(x, y, w, h);

    // Subtle grain lines
    g.setColour(woodBase.brighter(0.06f));
    for (int yy = y; yy < y + h; yy += 5)
        g.drawHorizontalLine(yy, static_cast<float>(x + 2), static_cast<float>(x + w - 2));

    g.setColour(woodBase.darker(0.1f));
    for (int yy = y + 3; yy < y + h; yy += 11)
        g.drawHorizontalLine(yy, static_cast<float>(x + 1), static_cast<float>(x + w - 1));
}

void UltimateProphetEditor::paintSectionBox(juce::Graphics& g, int x, int y, int w, int h,
                                              const juce::String& title)
{
    // Recessed panel
    g.setColour(juce::Colour(0xff121216));
    g.fillRoundedRectangle(static_cast<float>(x), static_cast<float>(y),
                           static_cast<float>(w), static_cast<float>(h), 3.0f);

    // Top highlight edge
    g.setColour(juce::Colour(0xff2A2A30));
    g.drawHorizontalLine(y, static_cast<float>(x + 3), static_cast<float>(x + w - 3));

    // Bottom shadow
    g.setColour(juce::Colour(0xff080808));
    g.drawHorizontalLine(y + h, static_cast<float>(x + 3), static_cast<float>(x + w - 3));

    // Title
    g.setColour(juce::Colour(0xffD4A843));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(title, x + 8, y + 2, w - 16, 16, juce::Justification::left);
}

void UltimateProphetEditor::paint(juce::Graphics& g)
{
    // Main metal panel
    g.fillAll(juce::Colour(0xff1A1A1E));

    // Wood side panels (Prophet-5 walnut cheeks) -- only on the synth area
    int synthH = SYNTH_PANEL_HEIGHT;
    paintWoodPanel(g, 0, 0, WOOD_WIDTH, synthH);
    paintWoodPanel(g, getWidth() - WOOD_WIDTH, 0, WOOD_WIDTH, synthH);

    int panelX = WOOD_WIDTH + 6;
    int panelW = getWidth() - 2 * (WOOD_WIDTH + 6);

    // Brand header
    g.setColour(juce::Colour(0xffD4A843));
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("ULTIMATE PROPHET", panelX, 4, panelW, 24, juce::Justification::centred);
    g.setFont(juce::Font(10.0f));
    g.drawText("CellDivision", panelX, 22, panelW, 14, juce::Justification::centred);

    // Section boxes
    paintSectionBox(g, panelX,       42,  430, 148, "OSCILLATORS");
    paintSectionBox(g, panelX + 438, 42,  panelW - 438, 148, "MIXER / MASTER");

    paintSectionBox(g, panelX,       198, 280, 148, "FILTER");
    paintSectionBox(g, panelX + 288, 198, 280, 148, "FILTER ENVELOPE");
    paintSectionBox(g, panelX + 576, 198, panelW - 576, 148, "AMP ENVELOPE");

    paintSectionBox(g, panelX,       354, 300, 118, "POLY-MOD");
    paintSectionBox(g, panelX + 308, 354, panelW - 308, 118, "EXTERNAL INPUT / MISC");

    // Keyboard hint
    g.setColour(juce::Colour(0xff4A4A50));
    g.setFont(juce::Font(9.0f));
    g.drawText("Keys: A-; = white, W/E/T/Y/U/O/P = black | Z/X = octave | C/V = velocity | ` = console",
               panelX, synthH - 18, panelW, 16, juce::Justification::centred);
}

void UltimateProphetEditor::resized()
{
    int x, y;
    int knobW = KNOB_SIZE + 4;
    int baseX = WOOD_WIDTH + 12;

    auto placeKnob = [&](KnobWithLabel& knob) {
        knob.label.setBounds(x, y, knobW, LABEL_HEIGHT);
        knob.slider.setBounds(x, y + LABEL_HEIGHT, KNOB_SIZE, KNOB_SIZE + 14);
        x += knobW;
    };

    // --- Row 1: Oscillators ---
    y = 60;
    x = baseX;

    oscAWaveform.label.setBounds(x, y, 76, LABEL_HEIGHT);
    oscAWaveform.combo.setBounds(x, y + LABEL_HEIGHT, 76, 20);
    x += 82;

    placeKnob(oscALevel);
    placeKnob(pulseWidth);

    x += SECTION_PAD;
    oscBWaveform.label.setBounds(x, y, 76, LABEL_HEIGHT);
    oscBWaveform.combo.setBounds(x, y + LABEL_HEIGHT, 76, 20);
    x += 82;

    placeKnob(oscBLevel);
    placeKnob(oscBDetune);

    // Sync button: top-right of the Oscillators section box
    oscSyncButton.setBounds(baseX + 430 - 62, 46, 56, 22);

    // Mixer / Master section
    x = baseX + 444;
    placeKnob(noiseLevel);
    placeKnob(extInputLevel);
    placeKnob(analogDrift);
    placeKnob(masterVolume);

    // --- Row 2: Filter + Envelopes ---
    y = 216;
    x = baseX;

    placeKnob(filterCutoff);
    placeKnob(filterResonance);
    placeKnob(filterEnvAmount);
    placeKnob(filterKeyTrack);

    x = baseX + 294;
    placeKnob(filterAttack);
    placeKnob(filterDecay);
    placeKnob(filterSustain);
    placeKnob(filterRelease);

    x = baseX + 582;
    placeKnob(ampAttack);
    placeKnob(ampDecay);
    placeKnob(ampSustain);
    placeKnob(ampRelease);

    // --- Row 3: Poly-Mod + Misc ---
    y = 372;
    x = baseX;

    placeKnob(polyModFiltEnvOscA);
    placeKnob(polyModFiltEnvFilter);
    placeKnob(polyModOscBOscA);
    placeKnob(polyModOscBFilter);

    // Octave display
    octaveLabel.setBounds(baseX + 314, y + 4, 120, 20);

    // --- Debug Console at the bottom ---
    int consoleH = consolePanel.isConsoleVisible()
                 ? DebugConsolePanel::EXPANDED_HEIGHT
                 : DebugConsolePanel::COLLAPSED_HEIGHT;
    consolePanel.setBounds(0, getHeight() - consoleH, getWidth(), consoleH);

    // Resize window to fit
    int totalH = SYNTH_PANEL_HEIGHT + consoleH;
    if (getHeight() != totalH)
        setSize(getWidth(), totalH);
}
