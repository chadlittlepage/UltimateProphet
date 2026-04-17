#pragma once
#include <JuceHeader.h>
#include "DSP/Prophet5Voice.h"
#include "DSP/CEM3320Filter.h"
#include "DSP/LFO.h"
#include "UI/DebugConsole.h"
#include "SysExLoader.h"

class UltimateProphetProcessor : public juce::AudioProcessor
{
public:
    UltimateProphetProcessor();
    ~UltimateProphetProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    void addKeyboardMidi(const juce::MidiMessage& msg);

    // Chord memory: call with currently held notes to store chord
    void storeChordMemory(const std::vector<int>& notes);
    void clearChordMemory();
    void loadBasicPreset();  // init patch — single saw osc, open filter
    bool isChordMemoryActive() const { return chordMemoryActive; }

    DebugConsole debugConsole;

    // Patch management
    std::vector<SysExLoader::Patch> loadedPatches;
    int currentPatchIndex = -1;
    void loadSysExFile(const juce::File& file);
    void selectPatch(int index);
    int getNumLoadedPatches() const { return static_cast<int>(loadedPatches.size()); }
    juce::String getPatchName(int index) const;

    // User patch save/load
    static constexpr int FACTORY_PATCH_COUNT = 200;
    void saveUserPatch(const juce::String& name);
    void loadUserPatches();
    juce::File getUserPatchDir() const;

    static constexpr int NUM_VOICES = 5;

private:
    void handleMidiMessage(const juce::MidiMessage& msg);
    int findFreeVoice() const;
    int findVoiceToSteal() const;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    Prophet5Voice voices[NUM_VOICES];
    uint64_t noteCounter = 0;
    bool unisonActive = false;
    float unisonDetuneAmount = 0.3f;
    float voiceDetuneOffset[NUM_VOICES] = { 0.0f, -0.15f, 0.15f, -0.3f, 0.3f };

    // Chord memory: stores interval offsets from root note
    bool chordMemoryActive = false;
    int chordIntervals[NUM_VOICES] = {};  // semitone offsets from lowest note
    int chordNoteCount = 0;

    // Key priority for unison: 0=Low, 1=Low-retrig, 2=Last, 3=Last-retrig
    int keyPriorityMode = 2;  // default: Last note priority

    // Track currently held notes for priority logic
    std::vector<int> heldNotes;
    int lastUnisonNote = -1;

    // Last played notes (persists after release, for chord memory capture)
    std::vector<int> lastPlayedNotes;
public:
    const std::vector<int>& getLastPlayedNotes() const { return lastPlayedNotes; }
private:

    // LFO (shared across all voices)
    LFO lfo;

    // External audio filter
    CEM3320Filter externalFilter;

    // Keyboard MIDI FIFO
    juce::MidiBuffer keyboardMidiBuffer;
    juce::SpinLock keyboardMidiLock;

    // Performance state
    float currentPitchBend = 0.0f;   // in semitones
    float currentModWheel = 0.0f;    // 0-1
    float currentAftertouch = 0.0f;  // 0-1 (channel pressure)
    int currentBank = 0;             // 0-9 (set via CC 32 Bank Select)
    bool damperPedalOn = false;      // CC 64 sustain pedal
    float brightnessOffset = 0.0f;   // CC 74 adds to filter cutoff

    // Parameter smoothing
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedResonance;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMasterVolume;

    double lastSampleRate = 44100.0;
    int lastBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UltimateProphetProcessor)
};
