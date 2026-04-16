#include "Prophet5Voice.h"

void Prophet5Voice::prepare(double sr)
{
    sampleRate = sr;
    oscA.prepare(sr);
    oscB.prepare(sr);
    // Filter runs at oversampled rate
    filter.prepare(sr, OVERSAMPLE_FACTOR);
    filterEnv.prepare(sr);
    ampEnv.prepare(sr);
    driftCounter = 0;
}

void Prophet5Voice::noteOn(int midiNote, float vel, uint64_t age)
{
    currentNote = midiNote;
    velocity = vel;
    noteAge = age;

    float hz = midiNoteToHz(static_cast<float>(midiNote));

    oscA.setFrequency(hz);
    oscB.setFrequency(hz * params.oscBFreqRatio
        * std::pow(2.0f, params.oscBDetune / 12.0f));

    filterEnv.noteOn();
    ampEnv.noteOn();
}

void Prophet5Voice::noteOff()
{
    filterEnv.noteOff();
    ampEnv.noteOff();
}

bool Prophet5Voice::isActive() const
{
    return ampEnv.isActive();
}

float Prophet5Voice::midiNoteToHz(float note) const
{
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

void Prophet5Voice::updateDrift()
{
    // Update drift targets every ~100 samples (slow random wander)
    if (++driftCounter >= 100)
    {
        driftCounter = 0;
        driftValueA = (random.nextFloat() * 2.0f - 1.0f) * params.analogDrift;
        driftValueB = (random.nextFloat() * 2.0f - 1.0f) * params.analogDrift;
    }

    // Smooth the drift with a very slow one-pole filter
    float driftSmooth = 0.001f;
    driftSmoothA += driftSmooth * (driftValueA - driftSmoothA);
    driftSmoothB += driftSmooth * (driftValueB - driftSmoothB);
}

float Prophet5Voice::process()
{
    if (!isActive())
        return 0.0f;

    updateDrift();

    // --- Update envelope parameters ---
    filterEnv.setAttack(params.filterAttack);
    filterEnv.setDecay(params.filterDecay);
    filterEnv.setSustain(params.filterSustain);
    filterEnv.setRelease(params.filterRelease);

    ampEnv.setAttack(params.ampAttack);
    ampEnv.setDecay(params.ampDecay);
    ampEnv.setSustain(params.ampSustain);
    ampEnv.setRelease(params.ampRelease);

    // --- Update oscillator settings ---
    oscA.setWaveform(params.oscAWaveform);
    oscA.setPulseWidth(params.pulseWidth);
    oscB.setWaveform(params.oscBWaveform);
    oscB.setPulseWidth(params.pulseWidth);

    // Apply drift to frequencies
    float baseHz = midiNoteToHz(static_cast<float>(currentNote));
    float hzA = baseHz * std::pow(2.0f, driftSmoothA / 1200.0f);  // cents to ratio
    float hzB = baseHz * params.oscBFreqRatio
              * std::pow(2.0f, params.oscBDetune / 12.0f)
              * std::pow(2.0f, driftSmoothB / 1200.0f);

    // --- Process Poly-Mod sources ---
    float filterEnvVal = filterEnv.getCurrentValue();

    // We need Osc B output before Osc A for poly-mod routing
    oscB.setFrequency(hzB);
    float oscBOut = oscB.process();

    // Compute poly-mod
    polyMod.setFilterEnvToOscAFreq(params.polyModFilterEnvToOscA);
    polyMod.setFilterEnvToFilterFreq(params.polyModFilterEnvToFilter);
    polyMod.setOscBToOscAFreq(params.polyModOscBToOscA);
    polyMod.setOscBToFilterFreq(params.polyModOscBToFilter);

    auto modResult = polyMod.process(filterEnvVal, oscBOut, hzA, params.filterCutoff);

    // Apply poly-mod to Osc A frequency
    float modulatedOscAHz = hzA + modResult.oscAFreqMod;
    modulatedOscAHz = juce::jlimit(1.0f, 20000.0f, modulatedOscAHz);
    oscA.setFrequency(modulatedOscAHz);

    // Osc sync: sub-sample accurate hard sync
    // When Osc B wraps, calculate where in the sample it happened
    // and reset Osc A's phase to the correct residual
    if (params.oscSync && oscB.hasWrapped())
    {
        float syncFrac = oscB.getWrapFraction();
        float residual = syncFrac * oscA.getPhaseIncrement();
        oscA.hardSync(residual);
    }

    float oscAOut = oscA.process();

    // --- Mixer ---
    float mixedSignal = oscAOut * params.oscALevel
                      + oscBOut * params.oscBLevel
                      + (random.nextFloat() * 2.0f - 1.0f) * params.noiseLevel;

    // --- Filter with 4x oversampling ---
    // Modulate cutoff using exponential (octave) scaling.
    // This is how real analog synths work: modulation sources shift
    // the cutoff by octaves, not by a fixed Hz amount.
    // envAmount of 1.0 = +7 octaves of sweep, matching the Prophet-5.
    float filterEnvOut = filterEnv.process();
    float envOctaves = filterEnvOut * params.filterEnvAmount * 7.0f;

    // Key tracking: 1.0 = cutoff follows pitch exactly (1 oct/oct)
    float keyOctaves = (static_cast<float>(currentNote) - 60.0f) / 12.0f
                     * params.filterKeyTrack;

    // Poly-mod contribution (already in Hz, convert to octaves relative to cutoff)
    float polyModOctaves = 0.0f;
    if (params.filterCutoff > 20.0f && std::abs(modResult.filterFreqMod) > 0.01f)
        polyModOctaves = std::log2(juce::jmax(20.0f, params.filterCutoff + modResult.filterFreqMod)
                                 / params.filterCutoff);

    float totalOctaves = envOctaves + keyOctaves + polyModOctaves;
    float modulatedCutoff = params.filterCutoff * std::pow(2.0f, totalOctaves);
    modulatedCutoff = juce::jlimit(20.0f, 20000.0f, modulatedCutoff);

    filter.setCutoff(modulatedCutoff);
    filter.setResonance(params.filterResonance);

    // Sample-and-hold upsampling: feed the same input to the filter
    // for all oversampled iterations, then take the last output.
    float filtered = 0.0f;
    for (int os = 0; os < OVERSAMPLE_FACTOR; ++os)
        filtered = filter.process(mixedSignal);

    // NaN protection: if filter blows up, reset it and pass dry signal
    if (std::isnan(filtered) || std::isinf(filtered))
    {
        filter.reset();
        filtered = mixedSignal;
    }

    // --- VCA (Amplitude Envelope) ---
    float ampEnvVal = ampEnv.process();
    float output = filtered * ampEnvVal * velocity;

    return output;
}
