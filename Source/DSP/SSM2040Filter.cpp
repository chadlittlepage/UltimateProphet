#include "SSM2040Filter.h"

void SSM2040Filter::prepare(double sr, int oversamplingFactor)
{
    sampleRate = sr * oversamplingFactor;
    oversampling = oversamplingFactor;
    reset();
}

void SSM2040Filter::setCutoff(float hz)
{
    cutoffHz = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.49), hz);
}

void SSM2040Filter::setResonance(float r)
{
    resonance = juce::jlimit(0.0f, 1.0f, r);
}

void SSM2040Filter::reset()
{
    for (auto& s : stage)
        s = 0.0f;
    lastOutput = 0.0f;
}

float SSM2040Filter::process(float input)
{
    // TPT coefficient
    float g = std::tan(juce::MathConstants<float>::pi * cutoffHz
            / static_cast<float>(sampleRate));
    float G = g / (1.0f + g);

    // Resonance: 0-1 maps to k = 0..4.0
    float k = resonance * 4.0f;

    // --- SSM 2040 Resonance Feedback ---
    // The SSM 2040 uses an EXTERNAL VCA (SSM 2020) for feedback.
    // The VCA has its own soft saturation characteristic, which is
    // softer/rounder than the CEM 3320's direct feedback.
    // Model: the feedback signal passes through a gentle tanh with
    // higher headroom (Vt=2.0), giving the "liquidy" quality.
    float fbSignal = std::tanh(lastOutput * 0.4f) * 2.5f;

    // Gain compensation (moderate — SSM 2040 loses less bass than CEM 3320)
    float u = input * (1.0f + k * 0.5f) - k * fbSignal;

    // --- SSM 2040 / Moog-style OTA Ladder ---
    // Each stage uses: tanh(V+) - tanh(V-)
    // This means each input is clipped INDEPENDENTLY before the
    // difference is taken. This produces warmer, rounder saturation
    // with more even harmonics than the CEM 3320's tanh(V+ - V-).
    //
    // In the TPT framework:
    //   v = G * (tanh(input/Vt)*Vt - tanh(state/Vt)*Vt)
    //   output = v + state
    //   state = output + v
    static constexpr float Vt = 1.8f;

    float out = u;
    for (int i = 0; i < 4; ++i)
    {
        // Moog-style: clip each side independently
        float tanhIn    = std::tanh(out      / Vt) * Vt;
        float tanhState = std::tanh(stage[i] / Vt) * Vt;
        float v = G * (tanhIn - tanhState);
        out = v + stage[i];       // OUTPUT
        stage[i] = out + v;       // STATE (trapezoidal)
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
