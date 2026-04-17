#include "Prophet5Voice.h"

void Prophet5Voice::prepare(double sr)
{
    sampleRate = sr;
    oscA.prepare(sr);
    oscB.prepare(sr);
    filterRev3.prepare(sr, OVERSAMPLE_FACTOR);
    filterRev12.prepare(sr, OVERSAMPLE_FACTOR);
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
    // Vintage knob affects envelope timing per-voice (random variation)
    float envDrift = 1.0f + driftSmoothFilter * params.vintage * 0.15f;

    filterEnv.setAttack(params.filterAttack * envDrift);
    filterEnv.setDecay(params.filterDecay * envDrift);
    filterEnv.setSustain(params.filterSustain);
    filterEnv.setRelease(params.filterRelease * envDrift);
    ampEnv.setAttack(params.ampAttack * envDrift);
    ampEnv.setDecay(params.ampDecay * envDrift);
    ampEnv.setSustain(params.ampSustain);
    ampEnv.setRelease(params.ampRelease * envDrift);

    // Rev mode changes envelope shape + release switch
    filterEnv.setRevMode(params.filterRev);
    ampEnv.setRevMode(params.filterRev);
    filterEnv.setReleaseEnabled(params.releaseSwitch);
    ampEnv.setReleaseEnabled(params.releaseSwitch);

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
                   + params.masterTune                  // master tune +/- 1 semitone
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
    oscB.setPulseWidth(params.oscBPulseWidth);

    // CEM 3340: ONE capacitor generates the saw ramp.
    // Triangle = wavefolder on saw. Pulse = comparator on saw.
    // processAll() computes all three with full antialiasing from
    // the same phase, advances phase exactly once.
    auto oscBWaves = oscB.processAll();

    float oscBOut = 0.0f;
    int oscBWaveCount = 0;
    if (params.oscBSawOn)   { oscBOut += oscBWaves.saw;      oscBWaveCount++; }
    if (params.oscBTriOn)   { oscBOut += oscBWaves.triangle;  oscBWaveCount++; }
    if (params.oscBPulseOn) { oscBOut += oscBWaves.pulse;     oscBWaveCount++; }
    if (oscBWaveCount > 1)
        oscBOut /= static_cast<float>(oscBWaveCount);

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
    // Aftertouch adds to LFO amount when AT > LFO is enabled
    float effectiveLfoAmount = params.lfoAmount;
    if (params.atToLFO)
        effectiveLfoAmount += params.aftertouch;
    effectiveLfoAmount = juce::jlimit(0.0f, 1.0f, effectiveLfoAmount);
    float lfoMod = params.lfoValue * effectiveLfoAmount;

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

    // CEM 3340: same capacitor, same phase, all waveforms antialiased
    auto oscAWaves = oscA.processAll();

    float oscAOut = 0.0f;
    int oscAWaveCount = 0;
    if (params.oscASawOn)   { oscAOut += oscAWaves.saw;   oscAWaveCount++; }
    if (params.oscAPulseOn) { oscAOut += oscAWaves.pulse;  oscAWaveCount++; }
    if (oscAWaveCount > 1)
        oscAOut /= static_cast<float>(oscAWaveCount);

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

    // Aftertouch -> filter cutoff (opens filter when pressing harder)
    float atFilterMod = 0.0f;
    if (params.atToFilter)
        atFilterMod = params.aftertouch * 3.0f;  // up to +3 octaves

    // Poly-mod + LFO + aftertouch + vintage drift contribution to filter
    float totalFilterOctaves = envOctaves + keyOctaves + filterMod
                             + atFilterMod + driftSmoothFilter * 0.5f;

    float modulatedCutoff = params.filterCutoff * std::pow(2.0f, totalFilterOctaves);
    modulatedCutoff = juce::jlimit(20.0f, 20000.0f, modulatedCutoff);

    // Select filter based on Rev switch
    float filtered = 0.0f;
    if (params.filterRev == 0)
    {
        // Rev 1/2: SSM 2040
        filterRev12.setCutoff(modulatedCutoff);
        filterRev12.setResonance(params.filterResonance);
        for (int os = 0; os < OVERSAMPLE_FACTOR; ++os)
            filtered = filterRev12.process(mixedSignal);
        if (std::isnan(filtered) || std::isinf(filtered))
        {
            filterRev12.reset();
            filtered = mixedSignal;
        }
    }
    else
    {
        // Rev 3: CEM 3320
        filterRev3.setCutoff(modulatedCutoff);
        filterRev3.setResonance(params.filterResonance);
        for (int os = 0; os < OVERSAMPLE_FACTOR; ++os)
            filtered = filterRev3.process(mixedSignal);
        if (std::isnan(filtered) || std::isinf(filtered))
        {
            filterRev3.reset();
            filtered = mixedSignal;
        }
    }

    // --- VCA ---
    float ampEnvVal = ampEnv.process();
    float velAmpScale = params.velToAmp ? velocity : 1.0f;
    // Vintage knob affects VCA gain per-voice (slight level differences)
    float vcaDrift = 1.0f + driftSmoothA * params.vintage * 0.08f;
    float output = filtered * ampEnvVal * velAmpScale * vcaDrift;

    return output;
}
