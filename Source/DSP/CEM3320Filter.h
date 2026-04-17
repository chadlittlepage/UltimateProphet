#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * CEM 3320 Voltage Controlled Filter — Authentic Model
 *
 * Based on the Doug Curtis CEM 3320 datasheet and Prophet-5 Rev 3 schematics.
 *
 * Datasheet specs modeled:
 * - Cell gain at Vcf=0: 0.9 (not 1.0)
 * - Max cell gain: 3.0
 * - Output swing at clipping: 12 Vpp (±6V normalized)
 * - Buffer slew rate: 1.5 V/uS (modeled as one-pole LPF)
 * - Resonance input impedance: 3.6k internal
 * - Prophet-5 resonance divider: 3k external (halves feedback signal)
 * - Frequency control: 60mV/decade (18mV/octave)
 * - Self-oscillation: 0.5-1.5% THD sine at max resonance
 *
 * Prophet-5 Rev 3 circuit:
 * - Input attenuated through 169k resistor (R82)
 * - 4 cascaded OTA integrator stages
 * - Resonance via negative feedback from stage 4 output
 * - Feedback through 3k/3.6k voltage divider
 *
 * OTA topology: I = g * tanh((V+ - V-) / 2Vt)
 * This differential-first-then-saturate approach gives the CEM its
 * characteristic "cutting, sizzley" resonance (vs Moog's warmer tone).
 */
class CEM3320Filter
{
public:
    void prepare(double sampleRate, int oversamplingFactor = 1);
    void setCutoff(float hz);
    void setResonance(float r);         // 0.0 - 1.0
    void setKeyTracking(float amount);
    void setEnvelopeAmount(float amount);

    float process(float input);
    void reset();

    float getLastOutput() const { return lastOutput; }
    float getCutoff() const { return cutoffHz; }
    float getResonance() const { return resonance; }

private:
    double sampleRate = 44100.0;
    int oversampling = 1;

    float cutoffHz = 10000.0f;
    float resonance = 0.0f;
    float keyTracking = 0.0f;
    float envelopeAmount = 0.0f;

    // 4 integrator states
    float stage[4] = {};
    float lastOutput = 0.0f;

    // Buffer slew rate limiting (one-pole LPF per stage)
    float slewState[4] = {};
    float slewCoeff = 1.0f;

    // CEM 3320 datasheet constants (normalized to ±1 signal range)
    static constexpr float CELL_GAIN = 0.9f;         // gain at Vcf=0
    static constexpr float MAX_CELL_GAIN = 3.0f;     // maximum gain
    static constexpr float CLIP_LEVEL = 6.0f;        // ±6V output swing (normalized)
    static constexpr float RES_DIVIDER = 0.545f;     // 3k / (3k + 3.6k) = feedback attenuation
    static constexpr float INPUT_ATTEN = 0.35f;      // 169k input resistor attenuation
};
