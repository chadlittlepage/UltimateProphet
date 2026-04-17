#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * CEM 3340 Virtual Analog Oscillator — Circuit-Level Model
 *
 * Models the actual CEM 3340 VCO topology as used in the Prophet-5 Rev 3.3:
 *
 * SAW CORE:
 *   - Capacitor charges via exponential current source (slight bow)
 *   - Comparator triggers discharge transistor at threshold
 *   - Finite reset time creates the characteristic "blip" (~1.5% of period)
 *   - 4th-order polyBLEP antialiasing at the reset discontinuity
 *
 * PULSE:
 *   - Derived from the saw via comparator (not independent)
 *   - PW threshold compared against saw ramp
 *   - Inherits the saw's analog character naturally
 *   - 4th-order polyBLEP at both edges
 *
 * TRIANGLE:
 *   - Wavefolder on the saw output (not integrated pulse)
 *   - Fold point creates soft peaks with even-harmonic content
 *   - Slight asymmetry from the saw's capacitor bow
 *   - 4th-order polyBLAMP at gradient discontinuities
 *
 * SYNC:
 *   - Sub-sample accurate hard sync with fractional phase reset
 *   - Properly bandlimited at the sync reset point
 */
class CEM3340Oscillator
{
public:
    enum class Waveform { Saw, Pulse, Triangle };

    void prepare(double sampleRate);
    void setFrequency(float hz);
    void setWaveform(Waveform wf);
    void setPulseWidth(float pw);       // 0.05 - 0.95

    // Sync interface
    void hardSync(float fractionalPhase);  // sub-sample accurate sync
    void hardSync();                        // immediate reset (legacy)

    float process();

    // Process all waveforms simultaneously from the same phase.
    // The real CEM 3340 derives saw, triangle, and pulse from
    // the same capacitor — they are always in perfect phase.
    // Phase advances only once. All outputs are antialiased.
    struct AllWaveforms {
        float saw = 0.0f;
        float triangle = 0.0f;
        float pulse = 0.0f;
    };
    AllWaveforms processAll();

    // Phase state for sync detection
    float getPhase() const { return phase; }
    float getPreviousPhase() const { return previousPhase; }
    float getPhaseIncrement() const { return phaseIncrement; }
    bool hasWrapped() const { return wrapped; }

    // Get the fractional sample position where the wrap occurred
    // (for sub-sample sync accuracy)
    float getWrapFraction() const { return wrapFraction; }

private:
    // 4th-order polyBLEP (2-point is amateur hour)
    static float polyBlep4(float t, float dt);

    // 4th-order polyBLAMP for gradient discontinuities
    static float polyBlamp4(float t, float dt);

    // Saw core with analog character
    float processSawCore();

    // Derive pulse from saw (comparator model)
    float processPulseFromSaw(float sawValue);

    // Derive triangle via wavefolder on saw
    float processTriangleFromSaw(float sawValue);

    // Soft saturation (models op-amp/transistor limiting)
    static float softClip(float x);

    double sampleRate = 44100.0;
    float phase = 0.0f;
    float previousPhase = 0.0f;
    float phaseIncrement = 0.0f;
    float frequency = 440.0f;
    float pulseWidth = 0.5f;
    Waveform waveform = Waveform::Saw;
    bool wrapped = false;
    float wrapFraction = 0.0f;

    // Saw reset modeling
    static constexpr float RESET_TIME = 0.012f;  // ~1.2% of period
    static constexpr float SAW_BOW = 0.015f;     // capacitor charge curvature

    // One-pole lowpass for natural HF rolloff (models circuit bandwidth)
    float lpState = 0.0f;
    float lpCoeff = 0.0f;

    // Previous saw output for pulse edge detection
    float prevSawForPulse = -1.0f;
    bool prevPulseState = false;

    // Triangle wavefolder state
    float prevTriOutput = 0.0f;
};
