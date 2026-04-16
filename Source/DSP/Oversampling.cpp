#include "Oversampling.h"

void Oversampling::prepare(double sampleRate, int blockSize, int factorIn)
{
    factor = factorIn;
    oversampledRate = sampleRate * factor;

    // Determine oversampling order from factor (2^order = factor)
    int order = 0;
    int f = factor;
    while (f > 1) { f >>= 1; ++order; }

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        1,      // mono per voice
        order,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true    // use maximum quality
    );

    oversampler->initProcessing(static_cast<size_t>(blockSize));
}

juce::dsp::AudioBlock<float> Oversampling::oversampleInput(juce::dsp::AudioBlock<float>& block)
{
    return oversampler->processSamplesUp(block);
}

void Oversampling::downsampleOutput(juce::dsp::AudioBlock<float>& block)
{
    oversampler->processSamplesDown(block);
}

void Oversampling::reset()
{
    if (oversampler)
        oversampler->reset();
}
