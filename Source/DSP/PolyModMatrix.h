#pragma once
#include <JuceHeader.h>

/**
 * Prophet-5 Poly-Mod Matrix
 *
 * Routes modulation sources to destinations:
 *   Sources: Filter Envelope, Oscillator B
 *   Destinations: Oscillator A Frequency, Filter Cutoff
 *
 * Each source has an amount knob per destination.
 * This is what gives the Prophet-5 its metallic, FM-like timbres.
 */
class PolyModMatrix
{
public:
    void setFilterEnvToOscAFreq(float amount);   // -1.0 to 1.0
    void setFilterEnvToFilterFreq(float amount);  // -1.0 to 1.0
    void setOscBToOscAFreq(float amount);         // -1.0 to 1.0
    void setOscBToFilterFreq(float amount);       // -1.0 to 1.0

    // Compute modulated values given current source signals
    struct ModResult
    {
        float oscAFreqMod;     // Hz offset to add to Osc A frequency
        float filterFreqMod;   // Hz offset to add to filter cutoff
    };

    ModResult process(float filterEnvValue, float oscBOutput,
                      float oscABaseFreq, float filterBaseCutoff) const;

private:
    float filterEnvToOscAFreq_ = 0.0f;
    float filterEnvToFilterFreq_ = 0.0f;
    float oscBToOscAFreq_ = 0.0f;
    float oscBToFilterFreq_ = 0.0f;
};
