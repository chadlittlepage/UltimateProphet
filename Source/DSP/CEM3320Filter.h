#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * CEM 3320 4-Pole Ladder Filter
 *
 * Dead simple implementation: prove resonance works first,
 * then add analog modeling back in.
 */
class CEM3320Filter
{
public:
    void prepare(double sampleRate, int oversamplingFactor = 1);
    void setCutoff(float hz);
    void setResonance(float r);         // 0.0 - 1.0
    void setKeyTracking(float amount);
    void setEnvelopeAmount(float amount);

    float process(float input);
    void reset();

private:
    double sampleRate = 44100.0;
    int oversampling = 1;

    float cutoffHz = 10000.0f;
    float resonance = 0.0f;
    float keyTracking = 0.0f;
    float envelopeAmount = 0.0f;

    float stage[4] = {};
    float lastOutput = 0.0f;

public:
    // Diagnostic: expose state for debug console
    float getLastOutput() const { return lastOutput; }
    float getCutoff() const { return cutoffHz; }
    float getResonance() const { return resonance; }
};
