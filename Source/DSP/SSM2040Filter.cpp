#include "SSM2040Filter.h"
#include "FastMath.h"

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
    for (auto& s : stage) s = 0.0f;
    lastOutput = 0.0f;
    fbVcaState = 0.0f;
}

float SSM2040Filter::process(float input)
{
    // --- TPT coefficient ---
    float g = std::tan(juce::MathConstants<float>::pi * cutoffHz
            / static_cast<float>(sampleRate));
    float G = g / (1.0f + g);

    float k = resonance * 4.0f;

    // --- SSM 2020 Resonance Feedback VCA ---
    // Soft saturation + bandwidth limiting on the feedback signal
    // lastOutput is at INTERNAL level (not amplified)
    float saturatedFb = FastMath::tanh(lastOutput * 0.4f) * 2.5f;
    fbVcaState += FB_VCA_SLEW * (saturatedFb - fbVcaState);
    float feedback = fbVcaState;

    // Gain compensation
    float compensation = 1.0f + k * 0.4f;

    // Input attenuation + compensation - feedback
    float u = input * INPUT_ATTEN * compensation - k * feedback;

    // --- SSM 2040 OTA Ladder: tanh(V+) - tanh(V-) ---
    static constexpr float Vt = 1.8f;

    float out = u;
    for (int i = 0; i < 4; ++i)
    {
        float tanhIn    = FastMath::tanh(out      / Vt) * Vt;
        float tanhState = FastMath::tanh(stage[i] / Vt) * Vt;
        float v = G * (tanhIn - tanhState);
        out = v + stage[i];
        stage[i] = out + v;

        out = juce::jlimit(-CLIP_LEVEL, CLIP_LEVEL, out);
    }

    // Store at INTERNAL level for feedback
    lastOutput = out;

    // Scale output back up
    float finalOut = out / INPUT_ATTEN;

    // Denormal / NaN protection
    for (auto& s : stage)
        if (!(s > -100.0f && s < 100.0f)) s = 0.0f;
    if (!(fbVcaState > -100.0f && fbVcaState < 100.0f)) fbVcaState = 0.0f;
    if (!(lastOutput > -100.0f && lastOutput < 100.0f)) lastOutput = 0.0f;

    return finalOut;
}
