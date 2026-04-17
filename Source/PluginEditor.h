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

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    bool keyStateChanged(bool isKeyDown, juce::Component* originatingComponent) override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void parentHierarchyChanged() override;

private:
    UltimateProphetProcessor& processorRef;
    ProphetLookAndFeel prophetLnf;
    DebugConsolePanel consolePanel;

    // QWERTY keyboard (Ableton Live layout)
    int keyboardOctave = 3;
    float keyVelocity = 100.0f / 127.0f;
    std::map<int, int> heldKeys;
    int getNoteForKey(int textChar) const;
    void sendNoteOn(int midiNote);
    void sendNoteOff(int midiNote);

    // --- UI Controls ---
    // Helper types
    struct Knob {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Toggle {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };
    struct Combo {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };

    void setupKnob(Knob& k, const juce::String& paramId, const juce::String& name);
    void setupToggle(Toggle& t, const juce::String& paramId, const juce::String& name);
    void setupCombo(Combo& c, const juce::String& paramId, const juce::String& name,
                    const juce::StringArray& items);

    // Poly-Mod
    Knob pmFiltEnv, pmOscB;
    Toggle pmFreqA, pmPWA, pmFilter;

    // LFO
    Knob lfoFreq, lfoAmount, lfoSrcMix;
    Toggle lfoSaw, lfoTri, lfoSqr;
    Toggle lfoToFreqA, lfoToFreqB, lfoToPWA, lfoToPWB, lfoToFilter;

    // Oscillator A
    Knob oscAFreq, oscAPW;
    Toggle oscASaw, oscAPulse;

    // Oscillator B
    Knob oscBFreq, oscBFine, oscBPW;
    Toggle oscBSaw, oscBTri, oscBPulse, oscBLowFreq, oscBKbd, oscSync;

    // Mixer
    Knob mixA, mixB, mixNoise;

    // Filter
    Knob filtCutoff, filtReso, filtEnvAmt;
    Combo filtKeyTrack, filtRev;

    // Filter Envelope
    Knob filtAtk, filtDec, filtSus, filtRel;

    // Amp Envelope
    Knob ampAtk, ampDec, ampSus, ampRel;

    // Performance
    Knob glideRate, vintage, pitchRange, masterVol, masterTune, unisonVoices, unisonDetune;
    Toggle glideOn, unisonOn, velFilt, velAmp, releaseSwitch, atFilter, atLFO;
    Combo keyPriority;
    juce::TextButton chordMemBtn{"Chord"};

    // Patch browser
    juce::TextButton loadSyxButton{"Load .syx"};
    juce::TextButton prevPatchButton{"<"};
    juce::TextButton nextPatchButton{">"};
    juce::Label patchNameLabel;
    void showPatchBrowser();
    void nextPatch();
    void prevPatch();

    // Octave/velocity display
    juce::Label statusLabel;

    void paintSection(juce::Graphics& g, int x, int y, int w, int h, const juce::String& title);
    void updatePatchLabel();

    static constexpr int KW = 62;  // knob width
    static constexpr int KH = 76;  // knob height (including label)
    static constexpr int TH = 22;  // toggle height
    static constexpr int WOOD = 28;
    static constexpr int SYNTH_H = 520;

    // Resizable scaling
    static constexpr int DEFAULT_W = 1150;
    static constexpr int PANEL_H = 520;    // synth panel only
    static constexpr int DEFAULT_H = 500 + DebugConsolePanel::COLLAPSED_HEIGHT;
    float scaleFactor = 1.0f;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UltimateProphetEditor)
};
