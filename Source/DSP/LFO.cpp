#include "LFO.h"

void LFO::prepare(double sr)
{
    sampleRate = sr;
    phase = 0.0f;
    currentValue = 0.0f;
    for (auto& s : pinkState) s = 0.0f;
}

void LFO::setFrequency(float hz)
{
    // Manual says range is 0.022 Hz to 500 Hz
    hz = juce::jlimit(0.022f, 500.0f, hz);
    phaseIncrement = static_cast<float>(hz / sampleRate);
}

float LFO::process()
{
    // --- LFO waveforms ---
    float lfoOut = 0.0f;
    int activeCount = 0;

    // Sawtooth: POSITIVE ONLY (0 to +1) per manual
    if (sawOn)
    {
        lfoOut += phase;
        activeCount++;
    }

    // Triangle: BIPOLAR (-1 to +1) per manual
    // "its waveshape is positive for half of its cycle and negative for the other half"
    if (triOn)
    {
        float tri = (phase < 0.5f)
            ? (4.0f * phase - 1.0f)
            : (3.0f - 4.0f * phase);
        lfoOut += tri;
        activeCount++;
    }

    // Square: POSITIVE ONLY (0 or +1) per manual
    // "generates only positive values...makes it possible to generate natural-sounding trills"
    if (sqrOn)
    {
        lfoOut += (phase < 0.5f) ? 1.0f : 0.0f;
        activeCount++;
    }

    if (activeCount > 1)
        lfoOut /= static_cast<float>(activeCount);

    // --- Pink noise (Voss-McCartney approximation) ---
    float white = noiseRng.nextFloat() * 2.0f - 1.0f;
    pinkState[0] = 0.99765f * pinkState[0] + white * 0.0990460f;
    pinkState[1] = 0.96300f * pinkState[1] + white * 0.2965164f;
    pinkState[2] = 0.57000f * pinkState[2] + white * 1.0526913f;
    float pink = (pinkState[0] + pinkState[1] + pinkState[2] + white * 0.1848f) * 0.22f;

    // --- Source Mix: crossfade LFO and Noise ---
    // 0 = pure LFO, 1 = pure noise
    currentValue = lfoOut * (1.0f - sourceMix) + pink * sourceMix;

    // Advance phase
    phase += phaseIncrement;
    if (phase >= 1.0f)
        phase -= 1.0f;

    return currentValue;
}
