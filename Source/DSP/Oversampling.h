#pragma once
#include <JuceHeader.h>

/**
 * Oversampling wrapper using JUCE's built-in oversampling.
 * 2x or 4x with IIR half-band filters for the nonlinear filter stage.
 */
class Oversampling
{
public:
    void prepare(double sampleRate, int blockSize, int factor = 2);
    int getFactor() const { return factor; }
    double getOversampledRate() const { return oversampledRate; }

    juce::dsp::AudioBlock<float> oversampleInput(juce::dsp::AudioBlock<float>& block);
    void downsampleOutput(juce::dsp::AudioBlock<float>& block);

    void reset();

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int factor = 2;
    double oversampledRate = 88200.0;
};
