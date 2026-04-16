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
    // TPT coefficient: g = tan(pi * fc / fs)
    float g = std::tan(juce::MathConstants<float>::pi * cutoffHz
            / static_cast<float>(sampleRate));
    float G = g / (1.0f + g);  // one-pole resolved gain

    // Resonance: 0-1 maps to k = 0 to 4.25
    float k = resonance * 4.25f;

    // Gain compensation so volume doesn't drop with resonance
    float compensation = 1.0f + k;

    // Feedback from 4th stage OUTPUT (not state!) - one sample delayed
    float u = input * compensation - k * lastOutput;

    // Soft clip the ladder input (OTA saturation at ~3V)
    u = std::tanh(u * 0.33f) * 3.0f;

    // 4 cascaded TPT one-pole stages
    // CRITICAL: output and state are DIFFERENT in trapezoidal integration
    //   v = G * (input - state)
    //   output = v + state          <-- feed to next stage
    //   state  = output + v         <-- store for next sample
    float out = u;
    for (int i = 0; i < 4; ++i)
    {
        float v = G * (out - stage[i]);
        out = v + stage[i];       // OUTPUT of this stage -> next stage input
        stage[i] = out + v;       // STATE update (trapezoidal)
    }

    // Store the OUTPUT (not the state) for feedback
    lastOutput = out;

    // Denormal / NaN protection
    for (auto& s : stage)
    {
        if (!(s > -100.0f && s < 100.0f))  // catches NaN and Inf too
            s = 0.0f;
    }
    if (!(lastOutput > -100.0f && lastOutput < 100.0f))
        lastOutput = 0.0f;

    return out;
}
