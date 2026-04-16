#include "Prophet5Voice.h"

void Prophet5Voice::prepare(double sr)
{
    sampleRate = sr;
    oscA.prepare(sr);
    oscB.prepare(sr);
    filter.prepare(sr, OVERSAMPLE_FACTOR);
    filterEnv.prepare(sr);
    ampEnv.prepare(sr);
    driftCounter = 0;
    glideCurrentNote = 60.0f;
    glideTargetNote = 60.0f;
}

void Prophet5Voice::noteOn(int midiNote, float vel, uint64_t age)
{
    // Store previous note for glide
    if (currentNote >= 0)
        glideCurrentNote = glideTargetNote;

    currentNote = midiNote;
    velocity = vel;
    noteAge = age;
    glideTargetNote = static_cast<float>(midiNote);

    // If no glide, snap immediately
    if (!params.glideOn || params.glideRate < 0.001f)
        glideCurrentNote = glideTargetNote;

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
    if (++driftCounter >= 100)
    {
        driftCounter = 0;
        driftValueA = (random.nextFloat() * 2.0f - 1.0f) * params.vintage;
        driftValueB = (random.nextFloat() * 2.0f - 1.0f) * params.vintage;
        driftFilterCutoff = (random.nextFloat() * 2.0f - 1.0f) * params.vintage;
    }

    float driftSmooth = 0.001f;
    driftSmoothA += driftSmooth * (driftValueA - driftSmoothA);
    driftSmoothB += driftSmooth * (driftValueB - driftSmoothB);
    driftSmoothFilter += driftSmooth * (driftFilterCutoff - driftSmoothFilter);
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

    // --- Glide (portamento) ---
    if (params.glideOn && params.glideRate > 0.001f)
    {
        float glideSpeed = 1.0f - params.glideRate * 0.999f;
        glideCurrentNote += glideSpeed * (glideTargetNote - glideCurrentNote);
    }
    else
    {
        glideCurrentNote = glideTargetNote;
    }

    // --- Compute base pitch ---
    // Osc A: freq knob offset + keyboard + pitch bend + drift + unison detune
    float oscANote = glideCurrentNote
                   + (params.oscAFreqKnob - 60.0f)    // freq knob offset from center
                   + params.pitchBendSemitones
                   + params.unisonDetuneSemitones      // per-voice unison spread
                   + driftSmoothA;

    // Osc B: freq knob offset + fine tune + optional keyboard + pitch bend + drift
    float oscBNote;
    if (params.oscBLowFreq)
    {
        // Low Freq mode: fixed low frequency based on knob only, no keyboard
        oscBNote = params.oscBFreqKnob - 60.0f + params.oscBFineTune + driftSmoothB;
        // Shift down ~5 octaves into sub-audio range
        oscBNote -= 60.0f;
    }
    else
    {
        oscBNote = (params.oscBKbdTrack ? glideCurrentNote : 60.0f)
                 + (params.oscBFreqKnob - 60.0f)
                 + params.oscBFineTune
                 + params.pitchBendSemitones
                 + driftSmoothB;
    }

    // --- Poly-Mod sources ---
    float filterEnvVal = filterEnv.getCurrentValue();

    // Process Osc B first (needed as poly-mod source)
    oscB.setFrequency(midiNoteToHz(oscBNote));

    // Generate Osc B output: sum of active waveforms
    float oscBOut = 0.0f;
    int oscBWaveCount = 0;
    if (params.oscBSawOn)
    {
        oscB.setWaveform(CEM3340Oscillator::Waveform::Saw);
        oscBOut += oscB.process();
        oscBWaveCount++;
    }
    if (params.oscBTriOn)
    {
        oscB.setWaveform(CEM3340Oscillator::Waveform::Triangle);
        // For stacking: we need to process at the same phase
        // In reality each waveform is derived from the same saw core
        // so we use the current phase to compute each waveform
        // For now, just add the triangle (process advances phase only once)
        if (oscBWaveCount == 0) {
            oscBOut += oscB.process();
            oscBWaveCount++;
        } else {
            // Triangle from same phase — approximate by computing from saw
            float p = oscB.getPhase();
            float tri = (p < 0.5f) ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);
            oscBOut += tri * 0.8f;
            oscBWaveCount++;
        }
    }
    if (params.oscBPulseOn)
    {
        if (oscBWaveCount == 0) {
            oscB.setWaveform(CEM3340Oscillator::Waveform::Pulse);
            oscBOut += oscB.process();
            oscBWaveCount++;
        } else {
            float p = oscB.getPhase();
            float pulse = (p < params.oscBPulseWidth) ? 1.0f : -1.0f;
            oscBOut += pulse;
            oscBWaveCount++;
        }
    }
    // If no waveforms active, still advance phase
    if (oscBWaveCount == 0)
    {
        oscB.setWaveform(CEM3340Oscillator::Waveform::Saw);
        oscB.process();
    }
    else if (oscBWaveCount > 1)
    {
        oscBOut /= static_cast<float>(oscBWaveCount);
    }

    oscB.setPulseWidth(params.oscBPulseWidth);

    // --- Poly-Mod computation ---
    float polyModSignal = params.polyModFiltEnvAmt * filterEnvVal
                        + params.polyModOscBAmt * oscBOut;

    // Apply poly-mod to destinations
    float oscAFreqMod = 0.0f;
    float oscAPWMod = 0.0f;
    float filterMod = 0.0f;

    if (params.polyModToFreqA)
        oscAFreqMod += polyModSignal * 24.0f;  // +/- 2 octaves at full mod
    if (params.polyModToPWA)
        oscAPWMod += polyModSignal * 0.4f;     // +/- 40% PW modulation
    if (params.polyModToFilter)
        filterMod += polyModSignal * 7.0f;     // +/- 7 octaves of cutoff mod

    // --- LFO modulation ---
    float lfoMod = params.lfoValue * params.lfoAmount;

    if (params.lfoToFreqA)
        oscAFreqMod += lfoMod * 3.0f;         // +/- 3 semitones at full
    if (params.lfoToFreqB)
        oscBNote += lfoMod * 3.0f;
    if (params.lfoToPWA)
        oscAPWMod += lfoMod * 0.3f;
    if (params.lfoToPWB)
        oscB.setPulseWidth(juce::jlimit(0.05f, 0.95f,
            params.oscBPulseWidth + lfoMod * 0.3f));
    if (params.lfoToFilter)
        filterMod += lfoMod * 3.0f;           // +/- 3 octaves of cutoff mod

    // --- Apply mod to Osc A and generate output ---
    float modulatedOscANote = oscANote + oscAFreqMod;
    oscA.setFrequency(midiNoteToHz(modulatedOscANote));
    oscA.setPulseWidth(juce::jlimit(0.05f, 0.95f,
        params.oscAPulseWidth + oscAPWMod));

    // Osc sync
    if (params.oscSync && oscB.hasWrapped())
    {
        float syncFrac = oscB.getWrapFraction();
        float residual = syncFrac * oscA.getPhaseIncrement();
        oscA.hardSync(residual);
    }

    // Generate Osc A output: sum of active waveforms
    float oscAOut = 0.0f;
    int oscAWaveCount = 0;
    if (params.oscASawOn)
    {
        oscA.setWaveform(CEM3340Oscillator::Waveform::Saw);
        oscAOut += oscA.process();
        oscAWaveCount++;
    }
    if (params.oscAPulseOn)
    {
        if (oscAWaveCount == 0) {
            oscA.setWaveform(CEM3340Oscillator::Waveform::Pulse);
            oscAOut += oscA.process();
            oscAWaveCount++;
        } else {
            float p = oscA.getPhase();
            float pw = juce::jlimit(0.05f, 0.95f,
                params.oscAPulseWidth + oscAPWMod);
            float pulse = (p < pw) ? 1.0f : -1.0f;
            oscAOut += pulse;
            oscAWaveCount++;
        }
    }
    if (oscAWaveCount == 0)
    {
        oscA.setWaveform(CEM3340Oscillator::Waveform::Saw);
        oscA.process();  // advance phase even with no waveforms
    }
    else if (oscAWaveCount > 1)
    {
        oscAOut /= static_cast<float>(oscAWaveCount);
    }

    // --- Mixer ---
    float mixedSignal = oscAOut * params.oscALevel
                      + oscBOut * params.oscBLevel
                      + (random.nextFloat() * 2.0f - 1.0f) * params.noiseLevel;

    // --- Filter ---
    float filterEnvOut = filterEnv.process();

    // Velocity to filter env
    float velFilterScale = params.velToFilter ? velocity : 1.0f;
    float envOctaves = filterEnvOut * params.filterEnvAmount * velFilterScale * 7.0f;

    // Key tracking: OFF=0, HALF=0.5, FULL=1.0
    float keyTrackAmount = 0.0f;
    if (params.filterKeyTrack == 1) keyTrackAmount = 0.5f;
    else if (params.filterKeyTrack == 2) keyTrackAmount = 1.0f;
    float keyOctaves = (glideCurrentNote - 60.0f) / 12.0f * keyTrackAmount;

    // Poly-mod + LFO + vintage drift contribution to filter
    float totalFilterOctaves = envOctaves + keyOctaves + filterMod
                             + driftSmoothFilter * 0.5f;

    float modulatedCutoff = params.filterCutoff * std::pow(2.0f, totalFilterOctaves);
    modulatedCutoff = juce::jlimit(20.0f, 20000.0f, modulatedCutoff);

    filter.setCutoff(modulatedCutoff);
    filter.setResonance(params.filterResonance);

    // 4x oversampled filter (sample-and-hold)
    float filtered = 0.0f;
    for (int os = 0; os < OVERSAMPLE_FACTOR; ++os)
        filtered = filter.process(mixedSignal);

    // NaN protection
    if (std::isnan(filtered) || std::isinf(filtered))
    {
        filter.reset();
        filtered = mixedSignal;
    }

    // --- VCA ---
    float ampEnvVal = ampEnv.process();
    float velAmpScale = params.velToAmp ? velocity : 1.0f;
    float output = filtered * ampEnvVal * velAmpScale;

    return output;
}
