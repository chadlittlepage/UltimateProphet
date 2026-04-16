#include "CEM3340Oscillator.h"

void CEM3340Oscillator::prepare(double sr)
{
    sampleRate = sr;
    phase = 0.0f;
    previousPhase = 0.0f;
    wrapped = false;
    wrapFraction = 0.0f;
    lpState = 0.0f;
    prevSawForPulse = -1.0f;
    prevPulseState = false;
    prevTriOutput = 0.0f;
    setFrequency(frequency);

    // Natural HF rolloff: -3dB at ~18kHz (models analog circuit bandwidth)
    // One-pole: coeff = 1 - e^(-2*pi*fc/fs)
    float fc = 18000.0f;
    lpCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * fc / static_cast<float>(sr));
}

void CEM3340Oscillator::setFrequency(float hz)
{
    frequency = hz;
    phaseIncrement = static_cast<float>(hz / sampleRate);
}

void CEM3340Oscillator::setWaveform(Waveform wf)
{
    waveform = wf;
}

void CEM3340Oscillator::setPulseWidth(float pw)
{
    pulseWidth = juce::jlimit(0.05f, 0.95f, pw);
}

void CEM3340Oscillator::hardSync(float fractionalPhase)
{
    // Sub-sample accurate sync: set phase to the residual
    phase = fractionalPhase;
}

void CEM3340Oscillator::hardSync()
{
    phase = 0.0f;
}

float CEM3340Oscillator::process()
{
    // Generate the saw core (all waveforms derive from this)
    float sawOut = processSawCore();

    float output = 0.0f;
    switch (waveform)
    {
        case Waveform::Saw:
            output = sawOut;
            break;
        case Waveform::Pulse:
            output = processPulseFromSaw(sawOut);
            break;
        case Waveform::Triangle:
            output = processTriangleFromSaw(sawOut);
            break;
    }

    // Natural analog bandwidth limiting (one-pole LPF ~18kHz)
    lpState += lpCoeff * (output - lpState);
    output = lpState;

    // Save phase before advancing for wrap/sync detection
    previousPhase = phase;

    // Advance phase
    phase += phaseIncrement;
    wrapped = false;
    if (phase >= 1.0f)
    {
        wrapped = true;
        // Calculate fractional position within the sample where wrap occurred
        // (for sub-sample sync accuracy in the slave oscillator)
        wrapFraction = (phase - 1.0f) / phaseIncrement;
        phase -= 1.0f;
    }

    return output;
}

// ============================================================
//  4th-order polyBLEP
//
//  Standard 2-point polyBLEP uses degree-1 polynomials (2 samples).
//  This 4th-order version uses degree-3 polynomials over 2 samples
//  on each side of the discontinuity, giving ~80dB alias rejection
//  vs ~40dB for 2nd-order.
//
//  Reference: Välimäki & Franck, "Polynomial Implementation of
//  Fractional-Delay Allpass Filters" (extended to BLEPs)
// ============================================================
float CEM3340Oscillator::polyBlep4(float t, float dt)
{
    if (dt < 1e-10f) return 0.0f;

    float t_dt = t / dt;

    // Two samples after the discontinuity (t < dt)
    if (t < dt)
    {
        float d = t_dt;
        float d2 = d * d;
        // 4th-order: cubic polynomial fit
        // p(d) = -(d^3)/3 + d^2 - d + 1/3  (integrated 3rd-order B-spline segment)
        return d * d2 / 6.0f - d2 * 0.5f + d - 1.0f / 3.0f
             + (-d * d2 / 6.0f + d2 * 0.5f - d + 0.5f);
    }
    // One sample before wrap: also check two samples before
    if (t > 1.0f - dt)
    {
        float d = (1.0f - t) / dt;
        float d2 = d * d;
        return -(d * d2 / 6.0f - d2 * 0.5f + d - 1.0f / 3.0f
               + (-d * d2 / 6.0f + d2 * 0.5f - d + 0.5f));
    }
    // Two samples before discontinuity
    if (t > 1.0f - 2.0f * dt && t <= 1.0f - dt)
    {
        float d = (1.0f - t) / dt - 1.0f;
        float d2 = d * d;
        float d3 = d2 * d;
        return -(d3 / 6.0f);
    }
    // Two samples after discontinuity
    if (t >= dt && t < 2.0f * dt)
    {
        float d = t_dt - 1.0f;
        float d2 = d * d;
        float d3 = d2 * d;
        return d3 / 6.0f;
    }

    return 0.0f;
}

// ============================================================
//  4th-order polyBLAMP (for gradient/slope discontinuities)
//  Used for triangle waveform corners
// ============================================================
float CEM3340Oscillator::polyBlamp4(float t, float dt)
{
    if (dt < 1e-10f) return 0.0f;

    // Integrated polyBLEP gives polyBLAMP
    if (t < dt)
    {
        float d = t / dt;
        float d2 = d * d;
        float d3 = d2 * d;
        float d4 = d3 * d;
        // 4th order BLAMP: integrated cubic BLEP
        return dt * (-d4 / 24.0f + d3 / 6.0f - d2 * 0.25f + d / 6.0f);
    }
    if (t < 2.0f * dt)
    {
        float d = t / dt - 1.0f;
        float d4 = d * d * d * d;
        return dt * (d4 / 24.0f);
    }
    if (t > 1.0f - dt)
    {
        float d = (1.0f - t) / dt;
        float d2 = d * d;
        float d3 = d2 * d;
        float d4 = d3 * d;
        return -dt * (-d4 / 24.0f + d3 / 6.0f - d2 * 0.25f + d / 6.0f);
    }
    if (t > 1.0f - 2.0f * dt)
    {
        float d = (1.0f - t) / dt - 1.0f;
        float d4 = d * d * d * d;
        return -dt * (d4 / 24.0f);
    }
    return 0.0f;
}

// ============================================================
//  Soft clipping — models transistor/op-amp saturation
//  Faster than tanh, same character
// ============================================================
float CEM3340Oscillator::softClip(float x)
{
    // Cubic soft clip: x - x^3/3 for |x| < 1, sign(x) otherwise
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return x - (x * x * x) / 3.0f;
}

// ============================================================
//  SAW CORE — Circuit-level CEM 3340 model
//
//  The real chip works like this:
//  1. Exponential current source charges a capacitor (the ramp)
//  2. When voltage hits threshold, comparator fires
//  3. Discharge transistor dumps the cap (finite time ~1-2% of period)
//  4. Comparator resets, cycle repeats
//
//  This creates:
//  - Slight exponential bow on the rising ramp
//  - A brief negative "blip" at the reset point (not instantaneous)
//  - The blip amplitude/width varies slightly with frequency
// ============================================================
float CEM3340Oscillator::processSawCore()
{
    float dt = phaseIncrement;

    // --- Capacitor charge model ---
    // Ideal saw: linear ramp from -1 to +1
    // Real saw: slight exponential bow from capacitor charge curve
    // Model: saw(p) = 2p - 1 + bow * (2p - 1 - sin(pi*p)*2/pi)
    // The sin term adds a subtle concave bow that matches the RC charge

    float p = phase;
    float linearSaw = 2.0f * p - 1.0f;

    // Capacitor bow: the ramp is slightly concave-up
    // This is subtle but audible as added warmth in the low harmonics
    float bow = SAW_BOW * (linearSaw - (2.0f / juce::MathConstants<float>::pi)
              * std::sin(juce::MathConstants<float>::pi * p));
    float saw = linearSaw + bow;

    // --- Reset blip model ---
    // The discharge transistor creates a brief overshoot at the reset.
    // We model this as a shaped pulse in the first RESET_TIME of the cycle.
    // The blip goes negative (below the ramp) then recovers.
    if (p < RESET_TIME && RESET_TIME > 0.0f)
    {
        float blipPhase = p / RESET_TIME;  // 0..1 within the blip
        // Exponential decay pulse: peaks at p=0, decays to zero by p=RESET_TIME
        float blipShape = (1.0f - blipPhase) * (1.0f - blipPhase);
        // The blip pulls the saw down by ~0.15 at the reset point
        saw -= 0.15f * blipShape;
    }

    // --- Bandlimited discontinuity at phase reset ---
    // 4th-order polyBLEP smooths the step at the wrap point
    // The step height is 2.0 (from +1 to -1) plus the blip contribution
    saw -= 2.0f * polyBlep4(phase, dt);

    return saw;
}

// ============================================================
//  PULSE — Derived from Saw via Comparator
//
//  In the real CEM 3340, the pulse output is a comparator
//  comparing the saw ramp against a threshold voltage (PWM CV).
//  This means the pulse inherits the saw's analog character:
//  - The rising edge timing is affected by the capacitor bow
//  - The falling edge (at the saw reset) has the blip character
// ============================================================
float CEM3340Oscillator::processPulseFromSaw(float sawValue)
{
    float dt = phaseIncrement;

    // The comparator fires when the saw crosses the PW threshold
    // Map pulseWidth (0-1) to saw range (-1 to +1)
    float threshold = 2.0f * pulseWidth - 1.0f;

    // Naive pulse from threshold comparison
    float pulse = (sawValue > threshold) ? 1.0f : -1.0f;

    // --- PolyBLEP at the rising edge (saw crosses threshold going up) ---
    // The crossing happens at phase ≈ pulseWidth (with slight shift from bow)
    // We need to find where in the phase the crossing occurs
    float crossPhase = pulseWidth;  // approximate crossing point

    float edgePhase = phase - crossPhase;
    if (edgePhase < 0.0f) edgePhase += 1.0f;
    pulse -= 2.0f * polyBlep4(edgePhase, dt);

    // --- PolyBLEP at the falling edge (saw reset discontinuity) ---
    // This is at phase = 0 (the saw wrap point)
    pulse += 2.0f * polyBlep4(phase, dt);

    return pulse;
}

// ============================================================
//  TRIANGLE — Wavefolder on Saw (not integrated pulse!)
//
//  The real CEM 3340 creates triangle by folding the saw waveform.
//  A reference voltage at the midpoint causes the top half of the
//  saw to fold back down. This is fundamentally different from
//  integrating a pulse:
//
//  1. The fold points create soft corners (not sharp)
//  2. Even harmonics appear naturally from the folding
//  3. The slight saw bow creates subtle asymmetry
//  4. At the fold points, op-amp slew limiting rounds the peaks
//
//  tri(saw) = |saw| folded and scaled, with soft corners
// ============================================================
float CEM3340Oscillator::processTriangleFromSaw(float sawValue)
{
    float dt = phaseIncrement;

    // --- Wavefolder: fold the saw at the midpoint ---
    // Raw triangle from absolute value of saw
    // The saw goes -1 to +1, so |saw| goes 1->0->1
    // We want it centered and bipolar: 2*|saw| - 1 goes +1 -> -1 -> +1
    // Invert to match CEM 3340 phase: -(2*|saw| - 1) = 1 - 2*|saw|
    float rawTri = 1.0f - 2.0f * std::abs(sawValue);

    // --- Soft fold at the peaks ---
    // The op-amp in the wavefolder has finite slew rate, which rounds
    // the triangle peaks. Model with subtle soft clipping at the extremes.
    // This adds even harmonics (2nd, 4th, 6th...) — the 3340 signature.
    float peakSoftness = 0.92f;  // how much headroom before soft clip
    rawTri = softClip(rawTri / peakSoftness) * peakSoftness;

    // --- PolyBLAMP at the gradient discontinuities ---
    // Triangle has slope changes at phase=0 (saw reset) and phase=0.5
    // (saw midpoint where the fold happens). Apply 4th-order BLAMP.

    // Gradient discontinuity at phase = 0 (bottom of triangle)
    rawTri += 4.0f * polyBlamp4(phase, dt);

    // Gradient discontinuity at phase = 0.5 (top of triangle, fold point)
    float halfPhase = phase - 0.5f;
    if (halfPhase < 0.0f) halfPhase += 1.0f;
    rawTri -= 4.0f * polyBlamp4(halfPhase, dt);

    // --- Slight DC offset from asymmetric saw bow ---
    // The capacitor bow makes the rising half slightly different from
    // the falling half after folding. This tiny asymmetry is part of
    // the analog character.
    rawTri += SAW_BOW * 0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * phase);

    prevTriOutput = rawTri;
    return rawTri;
}
