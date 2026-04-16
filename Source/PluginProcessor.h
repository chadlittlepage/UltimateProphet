#pragma once
#include <JuceHeader.h>
#include "DSP/Prophet5Voice.h"
#include "DSP/CEM3320Filter.h"
#include "UI/DebugConsole.h"

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

    // Called from editor GUI thread to inject keyboard MIDI
    void addKeyboardMidi(const juce::MidiMessage& msg);

    // Debug console (shared between processor + editor)
    DebugConsole debugConsole;

    static constexpr int NUM_VOICES = 5;

private:
    void handleMidiMessage(const juce::MidiMessage& msg);
    int findFreeVoice() const;
    int findVoiceToSteal() const;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    Prophet5Voice voices[NUM_VOICES];
    uint64_t noteCounter = 0;

    // External audio filter (for using plugin as an effect)
    CEM3320Filter externalFilter;

    // Keyboard MIDI FIFO: GUI thread writes, audio thread reads
    static constexpr int MIDI_FIFO_SIZE = 256;
    juce::MidiBuffer keyboardMidiBuffer;
    juce::SpinLock keyboardMidiLock;

    // Parameter smoothing for zipper-prone controls
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedResonance;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPulseWidth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMasterVolume;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOscALevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOscBLevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedNoiseLevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedExtInput;

    // For CPU measurement
    double lastSampleRate = 44100.0;
    int lastBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UltimateProphetProcessor)
};
