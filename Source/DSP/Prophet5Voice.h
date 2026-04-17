#pragma once
#include <JuceHeader.h>
#include "CEM3340Oscillator.h"
#include "CEM3320Filter.h"
#include "SSM2040Filter.h"
#include "CEM3310Envelope.h"
#include "LFO.h"

/**
 * Single Prophet-5 Voice — Matches Real Hardware Signal Flow
 *
 *  POLY-MOD:
 *    Sources: Filter Env, Osc B
 *    Destinations (toggles): Osc A Freq, Osc A PW, Filter Cutoff
 *
 *  LFO → (via amount + mod wheel) → Osc A Freq, Osc B Freq,
 *                                     Osc A PW, Osc B PW, Filter
 *
 *  [Osc A (saw+pulse)] + [Osc B (saw+tri+pulse)] + [Noise]
 *       → Mixer → VCF (CEM 3320) → VCA → Output
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

    float process();

    // All voice parameters — set from processor each sample
    struct Params
    {
        // --- Oscillator A ---
        bool oscASawOn = true;
        bool oscAPulseOn = false;
        float oscAFreqKnob = 60.0f;    // MIDI note number (0-120)
        float oscAPulseWidth = 0.5f;

        // --- Oscillator B ---
        bool oscBSawOn = true;
        bool oscBTriOn = false;
        bool oscBPulseOn = false;
        float oscBFreqKnob = 60.0f;    // MIDI note number (0-120)
        float oscBFineTune = 0.0f;     // semitones (-7 to +7)
        float oscBPulseWidth = 0.5f;
        bool oscBLowFreq = false;      // sub-audio LFO mode
        bool oscBKbdTrack = true;      // keyboard tracking on/off

        // --- Sync ---
        bool oscSync = false;

        // --- Mixer ---
        float oscALevel = 1.0f;
        float oscBLevel = 1.0f;
        float noiseLevel = 0.0f;

        // --- Filter ---
        float filterCutoff = 10000.0f;
        float filterResonance = 0.0f;
        float filterEnvAmount = 0.5f;   // bipolar: -1 to +1
        int   filterKeyTrack = 0;       // 0=OFF, 1=HALF, 2=FULL

        // --- Filter Envelope ---
        float filterAttack = 0.01f;
        float filterDecay = 0.3f;
        float filterSustain = 0.0f;
        float filterRelease = 0.3f;

        // --- Amp Envelope ---
        float ampAttack = 0.01f;
        float ampDecay = 0.3f;
        float ampSustain = 0.8f;
        float ampRelease = 0.3f;

        // --- Poly-Mod ---
        // Two source amount knobs
        float polyModFiltEnvAmt = 0.0f;  // 0-1
        float polyModOscBAmt = 0.0f;     // 0-1
        // Three destination toggles
        bool polyModToFreqA = false;
        bool polyModToPWA = false;
        bool polyModToFilter = false;

        // --- LFO (values computed in processor, passed per-sample) ---
        float lfoValue = 0.0f;          // current LFO output (-1 to +1)
        float lfoAmount = 0.0f;         // initial amount * mod wheel
        bool lfoToFreqA = false;
        bool lfoToFreqB = false;
        bool lfoToPWA = false;
        bool lfoToPWB = false;
        bool lfoToFilter = false;

        // --- Performance ---
        float masterTune = 0.0f;         // +/- 1 semitone
        float pitchBendSemitones = 0.0f; // current pitch bend in semitones
        float glideRate = 0.0f;          // 0 = instant, 1 = slowest
        bool glideOn = false;
        float vintage = 0.05f;           // analog drift amount
        float aftertouch = 0.0f;         // channel pressure 0-1
        bool atToFilter = false;         // aftertouch -> filter cutoff
        bool atToLFO = false;            // aftertouch -> LFO amount

        // --- Velocity ---
        bool velToFilter = false;
        bool velToAmp = true;

        // --- Rev switch: 0 = Rev 1/2 (SSM 2040), 1 = Rev 3 (CEM 3320) ---
        int filterRev = 1;  // default Rev 3

        // --- Release switch ---
        bool releaseSwitch = true;  // ON = use release knob, OFF = fast release

        // --- Unison detune (set per-voice by processor) ---
        float unisonDetuneSemitones = 0.0f;
    };

    Params params;

    static constexpr int OVERSAMPLE_FACTOR = 4;

private:
    float midiNoteToHz(float note) const;
    void updateDrift();

    CEM3340Oscillator oscA;
    CEM3340Oscillator oscB;
    CEM3320Filter filterRev3;      // CEM 3320 (Rev 3)
    SSM2040Filter filterRev12;     // SSM 2040 (Rev 1/2)
    CEM3310Envelope filterEnv;
    CEM3310Envelope ampEnv;

    juce::Random random;
    double sampleRate = 44100.0;
    int currentNote = -1;
    float velocity = 0.0f;
    uint64_t noteAge = 0;

    // Glide state
    float glideCurrentNote = 60.0f;
    float glideTargetNote = 60.0f;

    // Drift state
    float driftValueA = 0.0f;
    float driftValueB = 0.0f;
    float driftSmoothA = 0.0f;
    float driftSmoothB = 0.0f;
    float driftFilterCutoff = 0.0f;
    float driftSmoothFilter = 0.0f;
    int driftCounter = 0;
};
