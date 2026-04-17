#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * Prophet-5 LFO
 *
 * Based on the SSM 2030 (Rev 1/2) / CEM 3340 (Rev 3) running in LFO mode.
 * Range: 0.022 Hz to 500 Hz (per manual: "from a slow .022Hz to a fast 500Hz")
 * Waveforms: Sawtooth, Triangle, Square — all can be ON simultaneously.
 *
 * LFO waveshape notes from manual:
 * - Triangle is BIPOLAR (goes positive and negative equally — good for vibrato)
 * - Sawtooth and Square are POSITIVE ONLY (unipolar, offset to 0..+1)
 *
 * Source Mix: crossfades between LFO output and pink noise as the
 * modulation source. Fully left = LFO, fully right = Noise.
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

    // Source mix: 0.0 = LFO only, 1.0 = Noise only
    void setSourceMix(float mix)     { sourceMix = juce::jlimit(0.0f, 1.0f, mix); }

    // Process returns the mixed LFO/Noise output
    float process();

    float getCurrentValue() const { return currentValue; }

private:
    double sampleRate = 44100.0;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    float currentValue = 0.0f;
    float sourceMix = 0.0f;  // 0 = LFO, 1 = Noise

    bool sawOn = false;
    bool triOn = true;
    bool sqrOn = false;

    // Simple pink noise approximation (Voss-McCartney)
    juce::Random noiseRng;
    float pinkState[3] = {};
};
