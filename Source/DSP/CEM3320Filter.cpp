#include "CEM3320Filter.h"
#include "FastMath.h"

void CEM3320Filter::prepare(double sr, int oversamplingFactor)
{
    sampleRate = sr * oversamplingFactor;
    oversampling = oversamplingFactor;
    reset();

    // Buffer slew rate: 1.5 V/uS from datasheet
    // At normalized ±1 signal range, this corresponds to ~18kHz bandwidth
    // Model as one-pole LPF: coeff = 1 - exp(-2*pi*fc/fs)
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
    // --- Input attenuation ---
    // Prophet-5 Rev 3: audio enters through 169k resistor (R82)
    // This reduces the signal level before the OTA stages
    float attenuatedInput = input * INPUT_ATTEN;

    // --- TPT coefficient ---
    float g = std::tan(juce::MathConstants<float>::pi * cutoffHz
            / static_cast<float>(sampleRate));
    float G = g / (1.0f + g);

    // --- Resonance ---
    // CEM 3320: resonance feedback from stage 4 output through
    // the 3k/3.6k voltage divider (RES_DIVIDER = 0.545)
    // k = 0 to 4.0 for self-oscillation
    float k = resonance * 4.0f;

    // Feedback signal through the resonance divider
    float feedback = lastOutput * RES_DIVIDER;

    // Gain compensation: boost input to counter the bass loss from
    // negative feedback. The Prophet-5 circuit has gain staging that
    // partially compensates for this.
    float compensation = 1.0f + k * 0.5f;

    float u = attenuatedInput * compensation - k * feedback;

    // --- 4 Cascaded OTA Integrator Stages ---
    //
    // CEM 3320 OTA transfer: I = Iabc * tanh((V+ - V-) / 2Vt)
    //
    // The differential input is saturated BEFORE integration.
    // Cell gain at Vcf=0 is 0.9 (not 1.0), capped at 3.0 max.
    //
    // We model this as:
    //   saturatedDiff = CELL_GAIN * tanh(diff / Vt) * Vt
    //
    // where Vt is chosen so that:
    //   - Small signals: gain ≈ CELL_GAIN (0.9)
    //   - Large signals: soft clip at ±CLIP_LEVEL
    //
    // tanh(x) has derivative 1.0 at x=0, so:
    //   CELL_GAIN * tanh(diff/Vt) * Vt gives small-signal gain = CELL_GAIN
    //   Saturation at ±(CELL_GAIN * Vt)

    static constexpr float Vt = 1.5f;  // thermal voltage (normalized)

    float out = u;
    for (int i = 0; i < 4; ++i)
    {
        // CEM 3320 OTA: differential input, then saturate
        float diff = out - stage[i];
        float saturatedDiff = CELL_GAIN * FastMath::tanh(diff / Vt) * Vt;

        // Clamp to max cell gain (3.0x from datasheet)
        float maxOut = MAX_CELL_GAIN * std::abs(diff);
        if (std::abs(saturatedDiff) > maxOut && maxOut > 0.001f)
            saturatedDiff = (saturatedDiff > 0 ? maxOut : -maxOut);

        // TPT integration
        float v = G * saturatedDiff;
        out = v + stage[i];       // OUTPUT
        stage[i] = out + v;       // STATE (trapezoidal)

        // Buffer slew rate limiting (1.5 V/uS modeled as LPF)
        slewState[i] += slewCoeff * (out - slewState[i]);
        out = slewState[i];

        // Output clipping at ±6V (12 Vpp from datasheet, normalized)
        out = juce::jlimit(-CLIP_LEVEL, CLIP_LEVEL, out);
    }

    // Scale output back up (compensate for input attenuation)
    out /= INPUT_ATTEN;

    lastOutput = out;

    // Denormal / NaN protection
    for (int i = 0; i < 4; ++i)
    {
        if (!(stage[i] > -100.0f && stage[i] < 100.0f)) stage[i] = 0.0f;
        if (!(slewState[i] > -100.0f && slewState[i] < 100.0f)) slewState[i] = 0.0f;
    }
    if (!(lastOutput > -100.0f && lastOutput < 100.0f))
        lastOutput = 0.0f;

    return out;
}
