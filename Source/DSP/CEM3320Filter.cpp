#include "CEM3320Filter.h"
#include "FastMath.h"

void CEM3320Filter::prepare(double sr, int oversamplingFactor)
{
    sampleRate = sr * oversamplingFactor;
    oversampling = oversamplingFactor;
    reset();

    // Buffer slew rate: 1.5 V/uS from datasheet -> ~18kHz bandwidth
    float slewBW = 18000.0f;
    slewCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
              * slewBW / static_cast<float>(sampleRate));
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
    for (auto& s : stage) s = 0.0f;
    for (auto& s : slewState) s = 0.0f;
    lastOutput = 0.0f;
}

float CEM3320Filter::process(float input)
{
    // --- TPT coefficient ---
    float g = std::tan(juce::MathConstants<float>::pi * cutoffHz
            / static_cast<float>(sampleRate));
    float G = g / (1.0f + g);

    // --- Resonance ---
    // k = 0 to 4.0 (self-oscillation at 4.0)
    float k = resonance * 4.0f;

    // Feedback from stage 4 output through the 3k/3.6k resonance divider.
    // The divider halves the signal before it re-enters the filter input.
    // lastOutput is stored at INTERNAL level (not amplified).
    float feedback = lastOutput * RES_DIVIDER;

    // Gain compensation: moderate, allows authentic bass thinning
    float compensation = 1.0f + k * 0.6f;

    // Input attenuation (169k resistor) + compensation - resonance feedback
    float u = input * INPUT_ATTEN * compensation - k * feedback;

    // --- 4 Cascaded OTA Integrator Stages ---
    // CEM 3320: tanh applied to (input - state) DIFFERENCE
    // Cell gain at Vcf=0 = 0.9 (datasheet)
    static constexpr float Vt = 1.5f;

    float out = u;
    for (int i = 0; i < 4; ++i)
    {
        float diff = out - stage[i];
        float saturatedDiff = CELL_GAIN * FastMath::tanh(diff / Vt) * Vt;

        // TPT integration
        float v = G * saturatedDiff;
        out = v + stage[i];       // OUTPUT
        stage[i] = out + v;       // STATE (trapezoidal)

        // Buffer slew rate limiting
        slewState[i] += slewCoeff * (out - slewState[i]);
        out = slewState[i];

        // Output clipping at ±6V
        out = juce::jlimit(-CLIP_LEVEL, CLIP_LEVEL, out);
    }

    // Store at INTERNAL level for feedback (before output scaling)
    lastOutput = out;

    // Scale output back up to match input level
    float finalOut = out / INPUT_ATTEN;

    // Denormal / NaN protection
    for (int i = 0; i < 4; ++i)
    {
        if (!(stage[i] > -100.0f && stage[i] < 100.0f)) stage[i] = 0.0f;
        if (!(slewState[i] > -100.0f && slewState[i] < 100.0f)) slewState[i] = 0.0f;
    }
    if (!(lastOutput > -100.0f && lastOutput < 100.0f))
        lastOutput = 0.0f;

    return finalOut;
}
