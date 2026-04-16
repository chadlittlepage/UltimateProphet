#include "PolyModMatrix.h"

void PolyModMatrix::setFilterEnvToOscAFreq(float amount)
{
    filterEnvToOscAFreq_ = juce::jlimit(-1.0f, 1.0f, amount);
}

void PolyModMatrix::setFilterEnvToFilterFreq(float amount)
{
    filterEnvToFilterFreq_ = juce::jlimit(-1.0f, 1.0f, amount);
}

void PolyModMatrix::setOscBToOscAFreq(float amount)
{
    oscBToOscAFreq_ = juce::jlimit(-1.0f, 1.0f, amount);
}

void PolyModMatrix::setOscBToFilterFreq(float amount)
{
    oscBToFilterFreq_ = juce::jlimit(-1.0f, 1.0f, amount);
}

PolyModMatrix::ModResult PolyModMatrix::process(
    float filterEnvValue, float oscBOutput,
    float oscABaseFreq, float filterBaseCutoff) const
{
    ModResult result;

    // Modulation depth is proportional to the base frequency/cutoff
    // This models voltage-controlled exponential pitch/frequency scaling
    // (1 unit of mod = roughly 1 octave at full depth)

    // Osc A frequency modulation
    float oscAMod = filterEnvToOscAFreq_ * filterEnvValue
                  + oscBToOscAFreq_ * oscBOutput;
    // Scale: full mod range = +/- 2 octaves
    result.oscAFreqMod = oscABaseFreq * oscAMod * 4.0f;

    // Filter cutoff modulation
    float filterMod = filterEnvToFilterFreq_ * filterEnvValue
                    + oscBToFilterFreq_ * oscBOutput;
    // Scale: full mod range = +/- 4 octaves of cutoff
    result.filterFreqMod = filterBaseCutoff * filterMod * 16.0f;

    return result;
}
