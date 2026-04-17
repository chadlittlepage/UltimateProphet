#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * SSM 2040 4-Pole Low-Pass Filter — Authentic Model
 * (Prophet-5 Rev 1/2, recreated as SSI 2140 in Rev 4)
 *
 * Based on the Prophet-5 Rev 2 service manual schematics (SD411).
 *
 * Key circuit differences from CEM 3320:
 *
 * 1. TOPOLOGY: tanh(V+) - tanh(V-) per stage (Moog-style)
 *    Each input is clipped INDEPENDENTLY before differencing.
 *    This produces warmer, rounder, more even-harmonic distortion.
 *
 * 2. RESONANCE: External SSM 2020 VCA in the feedback path (RES FDBK VCA)
 *    The VCA has its own soft saturation that rounds the feedback signal.
 *    This gives the "liquidy" resonance quality.
 *
 * 3. CHARACTER: "Wide bandwidth, smooth, organic, warm, silky, 3D"
 *    vs CEM 3320's "direct, focused, ripping, sizzley, brassy"
 *
 * From the Rev 2 service manual:
 * - "FILT output is fed back to the input by the RES FDBK VCA"
 * - All VCAs are SSM 2020 (different saturation from CEM 3280)
 * - Input mixed through MIX OSC A/B AMT VCAs before filter
 */
class SSM2040Filter
{
public:
    void prepare(double sampleRate, int oversamplingFactor = 1);
    void setCutoff(float hz);
    void setResonance(float r);     // 0.0 - 1.0

    float process(float input);
    void reset();

    float getLastOutput() const { return lastOutput; }

private:
    double sampleRate = 44100.0;
    int oversampling = 1;

    float cutoffHz = 10000.0f;
    float resonance = 0.0f;

    float stage[4] = {};
    float lastOutput = 0.0f;

    // Feedback VCA state (SSM 2020 has its own filtering)
    float fbVcaState = 0.0f;

    // SSM 2040 constants
    static constexpr float INPUT_ATTEN = 0.4f;   // mixer VCA output level
    static constexpr float CLIP_LEVEL = 5.0f;     // SSM 2040 clips earlier than CEM
    static constexpr float FB_VCA_SLEW = 0.85f;   // feedback VCA smoothing (SSM 2020)
};
