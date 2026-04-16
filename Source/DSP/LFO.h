#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * Prophet-5 LFO
 *
 * Based on the CEM 3340 running in low-frequency mode.
 * Waveforms: Sawtooth, Triangle, Square — all can be ON simultaneously.
 * Output is the SUM of all engaged waveforms (Prophet-5 behavior).
 * Range: ~0.1 Hz to ~30 Hz (sub-audio).
 */
class LFO
{
public:
    void prepare(double sampleRate);
    void setFrequency(float hz);

    // Waveform toggles (can be on simultaneously)
    void setSawEnabled(bool on)      { sawOn = on; }
    void setTriangleEnabled(bool on) { triOn = on; }
    void setSquareEnabled(bool on)   { sqrOn = on; }

    float process();

    // Current output (for polling without advancing)
    float getCurrentValue() const { return currentValue; }

private:
    double sampleRate = 44100.0;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    float currentValue = 0.0f;

    bool sawOn = false;
    bool triOn = true;   // triangle is the default LFO shape
    bool sqrOn = false;
};
