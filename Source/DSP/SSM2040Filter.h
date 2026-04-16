#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * SSM 2040 4-Pole Low-Pass Filter (Prophet-5 Rev 1/2)
 *
 * Dave Rossum's original filter design, used in the Prophet-5 Rev1 and Rev2.
 * The new Prophet-5 Rev4 uses the SSI 2140 (modern recreation of the 2040).
 *
 * Key differences from CEM 3320 (Rev 3):
 *
 * 1. TOPOLOGY: The SSM 2040 uses tanh(V+) - tanh(V-) at each stage
 *    (Moog-style). The CEM 3320 uses tanh(V+ - V-). This means the
 *    SSM clips each input independently, producing a warmer, rounder
 *    distortion with more even harmonics.
 *
 * 2. RESONANCE: External feedback through a separate SSM 2020 VCA.
 *    This VCA has its own soft saturation, which smooths the feedback
 *    signal and gives the SSM 2040 its characteristic "liquidy" resonance.
 *    The CEM 3320's internal feedback is more direct and "cutting."
 *
 * 3. CHARACTER: "Wide bandwidth, smooth, organic, warm, silky, 3D"
 *    vs the CEM 3320's "direct, focused, ripping, sizzley, brassy."
 */
class SSM2040Filter
{
public:
    void prepare(double sampleRate, int oversamplingFactor = 1);
    void setCutoff(float hz);
    void setResonance(float r);     // 0.0 - 1.0

    float process(float input);
    void reset();

    float getLastOutput() const { return lastOutput; }

private:
    double sampleRate = 44100.0;
    int oversampling = 1;

    float cutoffHz = 10000.0f;
    float resonance = 0.0f;

    // 4 integrator states
    float stage[4] = {};
    float lastOutput = 0.0f;
};
