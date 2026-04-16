#pragma once
#include <JuceHeader.h>
#include <map>
#include "PluginProcessor.h"
#include "UI/ProphetLookAndFeel.h"
#include "UI/DebugConsole.h"

class UltimateProphetEditor : public juce::AudioProcessorEditor,
                               public juce::Timer,
                               public juce::KeyListener
{
public:
    explicit UltimateProphetEditor(UltimateProphetProcessor&);
    ~UltimateProphetEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // KeyListener overrides (catches keys even when child components have focus)
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    bool keyStateChanged(bool isKeyDown, juce::Component* originatingComponent) override;

    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void parentHierarchyChanged() override;

private:
    UltimateProphetProcessor& processorRef;
    ProphetLookAndFeel prophetLnf;

    // Debug console panel
    DebugConsolePanel consolePanel;

    // Computer keyboard -> MIDI mapping (Ableton Live style)
    int keyboardOctave = 3;
    float keyVelocity = 100.0f / 127.0f;  // default ~80%, adjustable via C/V
    std::map<int, int> heldKeys;   // JUCE keyCode -> midiNote that was triggered
    int getNoteForKey(int textChar) const;
    void sendNoteOn(int midiNote);
    void sendNoteOff(int midiNote);

    // Sliders + attachments for every parameter
    struct KnobWithLabel
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct ComboWithLabel
    {
        juce::ComboBox combo;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    // Oscillators
    ComboWithLabel oscAWaveform, oscBWaveform;
    KnobWithLabel oscALevel, oscBLevel, oscBDetune, pulseWidth;

    // Mixer
    KnobWithLabel noiseLevel;

    // Filter
    KnobWithLabel filterCutoff, filterResonance, filterEnvAmount, filterKeyTrack;

    // Filter Envelope
    KnobWithLabel filterAttack, filterDecay, filterSustain, filterRelease;

    // Amp Envelope
    KnobWithLabel ampAttack, ampDecay, ampSustain, ampRelease;

    // Poly-Mod
    KnobWithLabel polyModFiltEnvOscA, polyModFiltEnvFilter;
    KnobWithLabel polyModOscBOscA, polyModOscBFilter;

    // External Input
    KnobWithLabel extInputLevel;

    // Misc
    KnobWithLabel analogDrift, masterVolume;
    juce::ToggleButton oscSyncButton{"Sync"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;

    // Octave display
    juce::Label octaveLabel;

    void setupKnob(KnobWithLabel& knob, const juce::String& paramId, const juce::String& labelText);
    void setupCombo(ComboWithLabel& combo, const juce::String& paramId, const juce::String& labelText);
    void paintWoodPanel(juce::Graphics& g, int x, int y, int w, int h);
    void paintSectionBox(juce::Graphics& g, int x, int y, int w, int h, const juce::String& title);

    static constexpr int KNOB_SIZE = 64;
    static constexpr int LABEL_HEIGHT = 16;
    static constexpr int SECTION_PAD = 8;
    static constexpr int WOOD_WIDTH = 28;
    static constexpr int SYNTH_PANEL_HEIGHT = 490;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UltimateProphetEditor)
};
