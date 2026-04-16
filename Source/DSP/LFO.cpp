#include "LFO.h"

void LFO::prepare(double sr)
{
    sampleRate = sr;
    phase = 0.0f;
    currentValue = 0.0f;
}

void LFO::setFrequency(float hz)
{
    // LFO range: 0.1 Hz to ~30 Hz
    hz = juce::jlimit(0.1f, 30.0f, hz);
    phaseIncrement = static_cast<float>(hz / sampleRate);
}

float LFO::process()
{
    float output = 0.0f;
    int activeCount = 0;

    // Sawtooth: ramp from -1 to +1
    if (sawOn)
    {
        output += 2.0f * phase - 1.0f;
        activeCount++;
    }

    // Triangle: -1 to +1 to -1
    if (triOn)
    {
        float tri = (phase < 0.5f)
            ? (4.0f * phase - 1.0f)
            : (3.0f - 4.0f * phase);
        output += tri;
        activeCount++;
    }

    // Square: +1 / -1
    if (sqrOn)
    {
        output += (phase < 0.5f) ? 1.0f : -1.0f;
        activeCount++;
    }

    // Normalize when multiple waveforms are active
    if (activeCount > 1)
        output /= static_cast<float>(activeCount);

    currentValue = output;

    // Advance phase
    phase += phaseIncrement;
    if (phase >= 1.0f)
        phase -= 1.0f;

    return output;
}
