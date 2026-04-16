#pragma once
#include <JuceHeader.h>
#include "CEM3340Oscillator.h"
#include "CEM3320Filter.h"
#include "CEM3310Envelope.h"
#include "PolyModMatrix.h"

/**
 * Single Prophet-5 Voice
 *
 * Signal flow:
 *   [Osc A] + [Osc B] + [Noise] -> Mixer -> [4x Oversample Filter] -> [VCA] -> Output
 *        ^                              ^         ^
 *   [Poly-Mod from Osc B / Filter Env] [Filter Env]  [Amp Env]
 */
class Prophet5Voice
{
public:
    void prepare(double sampleRate);

    void noteOn(int midiNote, float velocity, uint64_t age);
    void noteOff();
    bool isActive() const;
    int getCurrentNote() const { return currentNote; }
    uint64_t getNoteAge() const { return noteAge; }
    float getAmpEnvValue() const { return ampEnv.getCurrentValue(); }
    bool isInRelease() const { return ampEnv.getStage() == CEM3310Envelope::Stage::Release; }

    // Per-sample processing
    float process();

    // Voice parameters (set from processor per-sample via smoothed values)
    struct Params
    {
        // Oscillator A
        CEM3340Oscillator::Waveform oscAWaveform = CEM3340Oscillator::Waveform::Saw;
        float oscALevel = 1.0f;

        // Oscillator B
        CEM3340Oscillator::Waveform oscBWaveform = CEM3340Oscillator::Waveform::Saw;
        float oscBLevel = 1.0f;
        float oscBDetune = 0.0f;        // semitones
        float oscBFreqRatio = 1.0f;     // frequency ratio (for fixed intervals)

        // Pulse width (shared for now)
        float pulseWidth = 0.5f;

        // Mixer
        float noiseLevel = 0.0f;

        // Filter
        float filterCutoff = 10000.0f;  // Hz
        float filterResonance = 0.0f;   // 0-1
        float filterEnvAmount = 0.5f;   // 0-1
        float filterKeyTrack = 0.5f;    // 0-1

        // Filter Envelope
        float filterAttack = 0.01f;
        float filterDecay = 0.3f;
        float filterSustain = 0.0f;
        float filterRelease = 0.3f;

        // Amp Envelope
        float ampAttack = 0.01f;
        float ampDecay = 0.3f;
        float ampSustain = 0.8f;
        float ampRelease = 0.3f;

        // Poly-Mod
        float polyModFilterEnvToOscA = 0.0f;
        float polyModFilterEnvToFilter = 0.0f;
        float polyModOscBToOscA = 0.0f;
        float polyModOscBToFilter = 0.0f;

        // Osc Sync
        bool oscSync = false;

        // Analog drift (per-voice random detuning)
        float analogDrift = 0.05f;      // cents of random drift
    };

    Params params;

    static constexpr int OVERSAMPLE_FACTOR = 4;

private:
    float midiNoteToHz(float note) const;
    void updateDrift();

    CEM3340Oscillator oscA;
    CEM3340Oscillator oscB;
    CEM3320Filter filter;
    CEM3310Envelope filterEnv;
    CEM3310Envelope ampEnv;
    PolyModMatrix polyMod;

    juce::Random random;
    double sampleRate = 44100.0;
    int currentNote = -1;
    float velocity = 0.0f;
    uint64_t noteAge = 0;
    float driftValueA = 0.0f;
    float driftValueB = 0.0f;
    float driftSmoothA = 0.0f;
    float driftSmoothB = 0.0f;
    int driftCounter = 0;
};
