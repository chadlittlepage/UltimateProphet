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
    for (auto& s : stage)
        s = 0.0f;
    lastOutput = 0.0f;
    fbVcaState = 0.0f;
}

float SSM2040Filter::process(float input)
{
    // --- Input attenuation (mixer VCA output level) ---
    float attenuatedInput = input * INPUT_ATTEN;

    // --- TPT coefficient ---
    float g = std::tan(juce::MathConstants<float>::pi * cutoffHz
            / static_cast<float>(sampleRate));
    float G = g / (1.0f + g);

    // --- Resonance: 0-1 maps to k = 0..4.0 ---
    float k = resonance * 4.0f;

    // --- SSM 2020 Resonance Feedback VCA ---
    // The Rev 2 schematic shows a dedicated SSM 2020 VCA (RES FDBK VCA)
    // in the feedback path. The SSM 2020 has:
    // - Softer saturation than CEM 3280 (warmer clipping)
    // - Its own bandwidth limiting (acts as a one-pole smoother)
    // - This is what gives the SSM 2040 its "liquidy" resonance
    //
    // Model: tanh for soft clip + one-pole smoothing

    // Feedback signal through the SSM 2020 VCA
    float rawFeedback = lastOutput;
    // SSM 2020 soft saturation (Vt=2.5 — wider linear range than CEM)
    float saturatedFb = FastMath::tanh(rawFeedback * 0.4f) * 2.5f;
    // VCA bandwidth limiting (smooths the feedback, prevents harshness)
    fbVcaState += FB_VCA_SLEW * (saturatedFb - fbVcaState);
    float feedback = fbVcaState;

    // Gain compensation (SSM 2040 loses less bass than CEM 3320)
    float compensation = 1.0f + k * 0.4f;

    float u = attenuatedInput * compensation - k * feedback;

    // --- SSM 2040 OTA Ladder: tanh(V+) - tanh(V-) ---
    //
    // This is the fundamental difference from the CEM 3320.
    // Each OTA stage clips its inputs INDEPENDENTLY:
    //   I = g * (tanh(Vin/Vt) - tanh(Vstate/Vt))
    //
    // When the input is large, tanh(Vin) saturates but tanh(Vstate)
    // may not, creating asymmetric clipping. This produces:
    // - Predominantly even-harmonic distortion (2nd, 4th)
    // - Warmer, rounder tone than CEM's odd-harmonic character
    // - The signal "folds" more gradually into saturation
    //
    // Vt = 1.8 gives the wider linear range of the SSM OTAs

    static constexpr float Vt = 1.8f;

    float out = u;
    for (int i = 0; i < 4; ++i)
    {
        // SSM/Moog-style: saturate each side independently
        float tanhIn    = FastMath::tanh(out      / Vt) * Vt;
        float tanhState = FastMath::tanh(stage[i] / Vt) * Vt;
        float v = G * (tanhIn - tanhState);
        out = v + stage[i];       // OUTPUT
        stage[i] = out + v;       // STATE (trapezoidal)

        // Output clipping (SSM 2040 clips slightly earlier than CEM)
        out = juce::jlimit(-CLIP_LEVEL, CLIP_LEVEL, out);
    }

    // Scale output back up
    out /= INPUT_ATTEN;

    lastOutput = out;

    // Denormal / NaN protection
    for (auto& s : stage)
    {
        if (!(s > -100.0f && s < 100.0f))
            s = 0.0f;
    }
    if (!(fbVcaState > -100.0f && fbVcaState < 100.0f))
        fbVcaState = 0.0f;
    if (!(lastOutput > -100.0f && lastOutput < 100.0f))
        lastOutput = 0.0f;

    return out;
}
