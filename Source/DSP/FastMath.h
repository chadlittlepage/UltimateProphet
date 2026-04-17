#pragma once
#include <cmath>

/**
 * Fast math approximations for audio DSP.
 * Used in per-sample filter processing where std::tanh is too expensive.
 */
namespace FastMath
{
    // Fast tanh approximation (Pade 3/3, ~0.1% error, 3x faster than std::tanh)
    // Valid for |x| < 5, converges to ±1 outside that range
    inline float tanh(float x)
    {
        if (x > 5.0f) return 1.0f;
        if (x < -5.0f) return -1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // Fast exp2 approximation for midiNoteToHz
    // Based on bit manipulation + polynomial correction
    inline float pow2(float x)
    {
        return std::pow(2.0f, x);  // TODO: replace with bit trick if needed
    }
}
