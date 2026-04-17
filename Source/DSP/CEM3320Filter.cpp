#include "CEM3320Filter.h"

void CEM3320Filter::prepare(double sr, int oversamplingFactor)
{
    sampleRate = sr * oversamplingFactor;
    oversampling = oversamplingFactor;
    reset();
}

void CEM3320Filter::setCutoff(float hz)
{
    cutoffHz = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.49), hz);
}

void CEM3320Filter::setResonance(float r)
{
    resonance = juce::jlimit(0.0f, 1.0f, r);
}

void CEM3320Filter::setKeyTracking(float amount)
{
    keyTracking = juce::jlimit(0.0f, 1.0f, amount);
}

void CEM3320Filter::setEnvelopeAmount(float amount)
{
    envelopeAmount = juce::jlimit(-1.0f, 1.0f, amount);
}

void CEM3320Filter::reset()
{
    for (auto& s : stage)
        s = 0.0f;
    lastOutput = 0.0f;
}

float CEM3320Filter::process(float input)
{
    // TPT coefficient
    float g = std::tan(juce::MathConstants<float>::pi * cutoffHz
            / static_cast<float>(sampleRate));
    float G = g / (1.0f + g);

    // Resonance: 0-1 maps to k = 0..4.25
    float k = resonance * 4.25f;

    // Gain compensation: moderate, allows some bass thinning
    // (authentic CEM 3320 behavior — bass drops with resonance)
    float compensation = 1.0f + k * 0.6f;

    // Feedback from 4th stage output (one sample delayed)
    float u = input * compensation - k * lastOutput;

    // --- CEM 3320 OTA Cascade ---
    //
    // The CEM 3320 uses: I = g * tanh(V+ - V-)
    // The tanh is applied to the DIFFERENCE (input - state),
    // not to the input alone. This is the key distinction from
    // the Moog ladder and gives the CEM its "cutting, sizzley"
    // resonance character with predominantly even-harmonic distortion.
    //
    // Vt controls where saturation kicks in. The CEM 3320 OTA
    // input saturates at roughly +/-50-100mV in the real circuit,
    // but our signals are normalized differently. Vt = 1.2 gives
    // gentle saturation that produces the focused, bright character.
    static constexpr float Vt = 1.2f;

    float out = u;
    for (int i = 0; i < 4; ++i)
    {
        // CEM 3320 OTA: tanh applied to the (input - state) DIFFERENCE
        float diff = out - stage[i];
        float saturatedDiff = std::tanh(diff / Vt) * Vt;
        float v = G * saturatedDiff;
        out = v + stage[i];       // OUTPUT to next stage
        stage[i] = out + v;       // STATE update (trapezoidal)
    }

    lastOutput = out;

    // Denormal / NaN protection
    for (auto& s : stage)
    {
        if (!(s > -100.0f && s < 100.0f))
            s = 0.0f;
    }
    if (!(lastOutput > -100.0f && lastOutput < 100.0f))
        lastOutput = 0.0f;

    return out;
}
