#include "PluginProcessor.h"
#include "PluginEditor.h"

static const char* midiNoteName(int note)
{
    static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    if (note < 0 || note > 127) return "??";
    return names[note % 12];
}

UltimateProphetProcessor::UltimateProphetProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), false)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    debugConsole.log("[INIT] UltimateProphet v0.3.0 — Prophet-5 Compatible");

    // Auto-load factory patches if found
    auto exePath = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    // Search several likely locations for the factory .syx
    juce::StringArray searchPaths = {
        "/Users/chadlittlepage/Documents/APPs/UltimateProphet/Patches",
        exePath.getParentDirectory().getChildFile("Patches").getFullPathName(),
        exePath.getParentDirectory().getParentDirectory().getChildFile("Patches").getFullPathName(),
    };

    for (auto& path : searchPaths)
    {
        juce::File factoryFile(path + "/P5_Factory_Programs_FACTORY_v1.03.syx");
        if (factoryFile.existsAsFile())
        {
            loadSysExFile(factoryFile);
            break;
        }
    }
}

UltimateProphetProcessor::~UltimateProphetProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
UltimateProphetProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // ===== OSCILLATOR A =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "oscAFreq", "Osc A Frequency",
        juce::NormalisableRange<float>(36.0f, 84.0f, 1.0f), 60.0f));  // 4 octaves, semitone steps
    p.push_back(std::make_unique<juce::AudioParameterBool>("oscASaw", "Osc A Saw", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("oscAPulse", "Osc A Pulse", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "oscAPW", "Osc A Pulse Width",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.5f));

    // ===== OSCILLATOR B =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "oscBFreq", "Osc B Frequency",
        juce::NormalisableRange<float>(36.0f, 84.0f, 1.0f), 60.0f));  // 4 octaves, semitone steps
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "oscBFineTune", "Osc B Fine Tune",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("oscBSaw", "Osc B Saw", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("oscBTri", "Osc B Triangle", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("oscBPulse", "Osc B Pulse", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "oscBPW", "Osc B Pulse Width",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.5f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("oscBLowFreq", "Osc B Low Freq", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("oscBKbd", "Osc B Kbd Track", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("oscSync", "Osc Sync", false));

    // ===== MIXER =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mixOscA", "Mixer Osc A",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mixOscB", "Mixer Osc B",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mixNoise", "Mixer Noise",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // ===== FILTER =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterCutoff", "Filter Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 10000.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterReso", "Filter Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterEnvAmt", "Filter Env Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        "filterKeyTrack", "Filter Key Track",
        juce::StringArray{"Off", "Half", "Full"}, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        "filterRev", "Filter Rev",
        juce::StringArray{"Rev 1/2", "Rev 3"}, 1));  // default Rev 3

    // ===== LFO =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lfoFreq", "LFO Frequency",
        juce::NormalisableRange<float>(0.022f, 500.0f, 0.001f, 0.25f), 5.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lfoAmount", "LFO Initial Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lfoSaw", "LFO Saw", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lfoTri", "LFO Triangle", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lfoSquare", "LFO Square", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lfoToFreqA", "LFO > Freq A", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lfoToFreqB", "LFO > Freq B", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lfoToPWA", "LFO > PW A", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lfoToPWB", "LFO > PW B", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lfoToFilter", "LFO > Filter", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lfoSrcMix", "LFO Source Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));  // 0=LFO, 1=Noise

    // ===== POLY-MOD =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pmodFiltEnv", "Poly-Mod Filt Env Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pmodOscB", "Poly-Mod Osc B Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("pmodToFreqA", "PMod > Freq A", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("pmodToPWA", "PMod > PW A", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("pmodToFilter", "PMod > Filter", false));

    // ===== FILTER ENVELOPE =====
    auto envRange = juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f);
    p.push_back(std::make_unique<juce::AudioParameterFloat>("filtAtk", "Filter Attack", envRange, 0.01f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("filtDec", "Filter Decay", envRange, 0.3f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filtSus", "Filter Sustain", 0.0f, 1.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("filtRel", "Filter Release", envRange, 0.3f));

    // ===== AMP ENVELOPE =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ampAtk", "Amp Attack", envRange, 0.01f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ampDec", "Amp Decay", envRange, 0.3f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ampSus", "Amp Sustain", 0.0f, 1.0f, 0.8f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ampRel", "Amp Release", envRange, 0.3f));

    // ===== RELEASE SWITCH =====
    p.push_back(std::make_unique<juce::AudioParameterBool>("releaseSwitch", "Release", true));

    // ===== MASTER TUNE =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "masterTune", "Master Tune",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));  // +/- 1 semitone

    // ===== PERFORMANCE =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "glideRate", "Glide Rate",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("glideOn", "Glide On", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("unisonOn", "Unison On", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "unisonDetune", "Unison Detune",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "unisonVoices", "Unison Voices",
        juce::NormalisableRange<float>(1.0f, 5.0f, 1.0f), 5.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        "keyPriority", "Key Priority",
        juce::StringArray{"Low", "Low Retrig", "Last", "Last Retrig"}, 2));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "vintage", "Vintage",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.05f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pitchWheelRange", "Pitch Wheel Range",
        juce::NormalisableRange<float>(1.0f, 12.0f, 1.0f), 2.0f));

    // ===== VELOCITY & AFTERTOUCH =====
    p.push_back(std::make_unique<juce::AudioParameterBool>("velToFilter", "Vel > Filter", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("velToAmp", "Vel > Amp", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("atToFilter", "AT > Filter", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("atToLFO", "AT > LFO", false));

    // ===== MASTER =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "masterVol", "Master Volume", 0.0f, 1.0f, 0.7f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "extInput", "Ext Input Level", 0.0f, 1.0f, 0.0f));

    return { p.begin(), p.end() };
}

void UltimateProphetProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lastSampleRate = sampleRate;
    lastBlockSize = samplesPerBlock;

    for (auto& voice : voices)
        voice.prepare(sampleRate);

    lfo.prepare(sampleRate);
    externalFilter.prepare(sampleRate, Prophet5Voice::OVERSAMPLE_FACTOR);

    smoothedCutoff.reset(sampleRate, 0.02);
    smoothedResonance.reset(sampleRate, 0.02);
    smoothedMasterVolume.reset(sampleRate, 0.05);

    debugConsole.currentSampleRate.store(sampleRate);
    debugConsole.currentBlockSize.store(samplesPerBlock);
    debugConsole.oversampleFactor.store(Prophet5Voice::OVERSAMPLE_FACTOR);
    debugConsole.log("[AUDIO] prepareToPlay: %.0f Hz, %d smp, %dx OS",
                     sampleRate, samplesPerBlock, Prophet5Voice::OVERSAMPLE_FACTOR);
}

void UltimateProphetProcessor::releaseResources() {}

bool UltimateProphetProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto outSet = layouts.getMainOutputChannelSet();
    if (outSet != juce::AudioChannelSet::mono()
     && outSet != juce::AudioChannelSet::stereo())
        return false;
    auto inSet = layouts.getMainInputChannelSet();
    if (!inSet.isDisabled()
     && inSet != juce::AudioChannelSet::mono()
     && inSet != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void UltimateProphetProcessor::addKeyboardMidi(const juce::MidiMessage& msg)
{
    const juce::SpinLock::ScopedLockType lock(keyboardMidiLock);
    keyboardMidiBuffer.addEvent(msg, 0);
}

void UltimateProphetProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto startTime = juce::Time::getHighResolutionTicks();

    // Merge keyboard MIDI
    {
        const juce::SpinLock::ScopedLockType lock(keyboardMidiLock);
        for (const auto metadata : keyboardMidiBuffer)
            midiMessages.addEvent(metadata.getMessage(), metadata.samplePosition);
        keyboardMidiBuffer.clear();
    }

    int numInputChannels = getTotalNumInputChannels();
    const float* inputLeft = (numInputChannels > 0) ? buffer.getReadPointer(0) : nullptr;
    buffer.clear();

    // ===== Read all parameters once per block =====
    auto& a = apvts;
    auto load = [&](const char* id) { return a.getRawParameterValue(id)->load(); };
    auto loadBool = [&](const char* id) { return load(id) > 0.5f; };

    // Osc A
    float oscAFreq = load("oscAFreq");
    bool oscASaw = loadBool("oscASaw");
    bool oscAPulse = loadBool("oscAPulse");
    float oscAPW = load("oscAPW");

    // Osc B
    float oscBFreq = load("oscBFreq");
    float oscBFine = load("oscBFineTune");
    bool oscBSaw = loadBool("oscBSaw");
    bool oscBTri = loadBool("oscBTri");
    bool oscBPulse = loadBool("oscBPulse");
    float oscBPW = load("oscBPW");
    bool oscBLowFreq = loadBool("oscBLowFreq");
    bool oscBKbd = loadBool("oscBKbd");
    bool oscSync = loadBool("oscSync");

    // Mixer
    float mixA = load("mixOscA");
    float mixB = load("mixOscB");
    float mixNoise = load("mixNoise");

    // Filter
    smoothedCutoff.setTargetValue(load("filterCutoff"));
    smoothedResonance.setTargetValue(load("filterReso"));
    int filterRev = static_cast<int>(load("filterRev"));
    float filtEnvAmt = load("filterEnvAmt");
    int filtKeyTrack = static_cast<int>(load("filterKeyTrack"));

    // LFO
    lfo.setFrequency(load("lfoFreq"));
    lfo.setSawEnabled(loadBool("lfoSaw"));
    lfo.setTriangleEnabled(loadBool("lfoTri"));
    lfo.setSquareEnabled(loadBool("lfoSquare"));
    lfo.setSourceMix(load("lfoSrcMix"));
    float lfoAmt = load("lfoAmount");
    bool lfoFreqA = loadBool("lfoToFreqA");
    bool lfoFreqB = loadBool("lfoToFreqB");
    bool lfoPWA = loadBool("lfoToPWA");
    bool lfoPWB = loadBool("lfoToPWB");
    bool lfoFilt = loadBool("lfoToFilter");

    // Poly-Mod
    float pmodFiltEnv = load("pmodFiltEnv");
    float pmodOscB = load("pmodOscB");
    bool pmodFreqA = loadBool("pmodToFreqA");
    bool pmodPWA = loadBool("pmodToPWA");
    bool pmodFilter = loadBool("pmodToFilter");

    // Envelopes
    float filtAtk = load("filtAtk"), filtDec = load("filtDec");
    float filtSus = load("filtSus"), filtRel = load("filtRel");
    float ampAtk = load("ampAtk"), ampDec = load("ampDec");
    float ampSus = load("ampSus"), ampRel = load("ampRel");

    // Performance
    float glideRate = load("glideRate");
    bool glideOn = loadBool("glideOn");
    float vintage = load("vintage");
    float pitchWheelRange = load("pitchWheelRange");
    bool velToFilter = loadBool("velToFilter");
    bool velToAmp = loadBool("velToAmp");
    bool releaseSwitch = loadBool("releaseSwitch");
    bool atToFilter = loadBool("atToFilter");
    bool atToLFO = loadBool("atToLFO");
    float masterTune = load("masterTune");
    int unisonVoiceCount = static_cast<int>(load("unisonVoices"));

    smoothedMasterVolume.setTargetValue(load("masterVol"));
    float extInputLevel = load("extInput");

    // Unison
    unisonActive = loadBool("unisonOn");
    unisonDetuneAmount = load("unisonDetune");
    // Compute per-voice detune offsets (symmetric spread in semitones)
    // Voice 0 = center, 1/2 = slight spread, 3/4 = wider spread
    float detuneScale = unisonDetuneAmount * 0.5f;  // max 0.25 semitones per side
    voiceDetuneOffset[0] = 0.0f;
    voiceDetuneOffset[1] = -detuneScale;
    voiceDetuneOffset[2] =  detuneScale;
    voiceDetuneOffset[3] = -detuneScale * 2.0f;
    voiceDetuneOffset[4] =  detuneScale * 2.0f;

    // Process MIDI
    for (const auto metadata : midiMessages)
        handleMidiMessage(metadata.getMessage());

    // Render
    int numSamples = buffer.getNumSamples();
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    float blockPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Advance LFO (shared across voices)
        float lfoVal = lfo.process();
        float lfoModAmount = lfoAmt + currentModWheel;  // mod wheel adds to LFO
        lfoModAmount = juce::jlimit(0.0f, 1.0f, lfoModAmount);

        float cutoff = smoothedCutoff.getNextValue();
        float reso = smoothedResonance.getNextValue();
        float vol = smoothedMasterVolume.getNextValue();

        // Update all voice params
        for (int vi = 0; vi < NUM_VOICES; ++vi)
        {
            auto& voice = voices[vi];
            voice.params.unisonDetuneSemitones = unisonActive ? voiceDetuneOffset[vi] : 0.0f;
            voice.params.oscASawOn = oscASaw;
            voice.params.oscAPulseOn = oscAPulse;
            voice.params.oscAFreqKnob = oscAFreq;
            voice.params.oscAPulseWidth = oscAPW;

            voice.params.oscBSawOn = oscBSaw;
            voice.params.oscBTriOn = oscBTri;
            voice.params.oscBPulseOn = oscBPulse;
            voice.params.oscBFreqKnob = oscBFreq;
            voice.params.oscBFineTune = oscBFine;
            voice.params.oscBPulseWidth = oscBPW;
            voice.params.oscBLowFreq = oscBLowFreq;
            voice.params.oscBKbdTrack = oscBKbd;
            voice.params.oscSync = oscSync;

            voice.params.oscALevel = mixA;
            voice.params.oscBLevel = mixB;
            voice.params.noiseLevel = mixNoise;

            voice.params.filterCutoff = cutoff;
            voice.params.filterResonance = reso;
            voice.params.filterRev = filterRev;
            voice.params.filterEnvAmount = filtEnvAmt;
            voice.params.filterKeyTrack = filtKeyTrack;

            voice.params.filterAttack = filtAtk;
            voice.params.filterDecay = filtDec;
            voice.params.filterSustain = filtSus;
            voice.params.filterRelease = filtRel;
            voice.params.ampAttack = ampAtk;
            voice.params.ampDecay = ampDec;
            voice.params.ampSustain = ampSus;
            voice.params.ampRelease = ampRel;

            voice.params.polyModFiltEnvAmt = pmodFiltEnv;
            voice.params.polyModOscBAmt = pmodOscB;
            voice.params.polyModToFreqA = pmodFreqA;
            voice.params.polyModToPWA = pmodPWA;
            voice.params.polyModToFilter = pmodFilter;

            voice.params.lfoValue = lfoVal;
            voice.params.lfoAmount = lfoModAmount;
            voice.params.lfoToFreqA = lfoFreqA;
            voice.params.lfoToFreqB = lfoFreqB;
            voice.params.lfoToPWA = lfoPWA;
            voice.params.lfoToPWB = lfoPWB;
            voice.params.lfoToFilter = lfoFilt;

            voice.params.pitchBendSemitones = currentPitchBend;
            voice.params.glideRate = glideRate;
            voice.params.glideOn = glideOn;
            voice.params.vintage = vintage;
            voice.params.velToFilter = velToFilter;
            voice.params.velToAmp = velToAmp;
            voice.params.masterTune = masterTune;
            voice.params.releaseSwitch = releaseSwitch;
            voice.params.aftertouch = currentAftertouch;
            voice.params.atToFilter = atToFilter;
            voice.params.atToLFO = atToLFO;
        }

        float sum = 0.0f;
        for (auto& voice : voices)
            sum += voice.process();

        // External audio input through filter
        if (extInputLevel > 0.001f && inputLeft != nullptr)
        {
            externalFilter.setCutoff(cutoff);
            externalFilter.setResonance(reso);
            float extSample = inputLeft[sample] * extInputLevel;
            float extFiltered = 0.0f;
            for (int os = 0; os < Prophet5Voice::OVERSAMPLE_FACTOR; ++os)
                extFiltered = externalFilter.process(extSample);
            sum += extFiltered;
        }

        sum = std::tanh(sum * vol);

        float absSample = std::abs(sum);
        if (absSample > blockPeak) blockPeak = absSample;

        leftChannel[sample] = sum;
        if (rightChannel) rightChannel[sample] = sum;
    }

    // Update debug stats
    debugConsole.peakLevel.store(blockPeak);
    for (int i = 0; i < NUM_VOICES; ++i)
    {
        debugConsole.voiceStats[i].active.store(voices[i].isActive());
        debugConsole.voiceStats[i].note.store(voices[i].getCurrentNote());
        debugConsole.voiceStats[i].envLevel.store(voices[i].getAmpEnvValue());
        debugConsole.voiceStats[i].releasing.store(voices[i].isInRelease());
    }

    auto endTime = juce::Time::getHighResolutionTicks();
    double elapsedSec = juce::Time::highResolutionTicksToSeconds(endTime - startTime);
    double bufferSec = static_cast<double>(numSamples) / lastSampleRate;
    debugConsole.cpuLoad.store(static_cast<float>(elapsedSec / bufferSec * 100.0));
}

void UltimateProphetProcessor::handleMidiMessage(const juce::MidiMessage& msg)
{
    keyPriorityMode = static_cast<int>(apvts.getRawParameterValue("keyPriority")->load());

    if (msg.isNoteOn())
    {
        int note = msg.getNoteNumber();
        float vel = msg.getFloatVelocity();

        // Track held notes for priority logic
        heldNotes.push_back(note);

        if (unisonActive)
        {
            int uvc = juce::jlimit(1, NUM_VOICES,
                static_cast<int>(apvts.getRawParameterValue("unisonVoices")->load()));

            // Key priority: determine which note to play
            int playNote = note;
            if (keyPriorityMode == 0 || keyPriorityMode == 1)  // Low / Low-retrig
            {
                playNote = *std::min_element(heldNotes.begin(), heldNotes.end());
            }
            // else Last / Last-retrig: use the note that was just pressed

            // Should we retrigger envelopes?
            bool shouldRetrigger = (keyPriorityMode == 1 || keyPriorityMode == 3);  // retrig modes
            bool isNewNote = (playNote != lastUnisonNote);

            if (isNewNote || shouldRetrigger)
            {
                if (chordMemoryActive && chordNoteCount > 0)
                {
                    // Chord memory: play the stored chord transposed to the new root
                    for (int i = 0; i < juce::jmin(chordNoteCount, uvc); ++i)
                    {
                        int chordNote = juce::jlimit(0, 127, playNote + chordIntervals[i]);
                        voices[i].noteOn(chordNote, vel, ++noteCounter);
                    }
                    debugConsole.log("[MIDI] NoteOn %s%d -> CHORD (%d notes)",
                                     midiNoteName(playNote), (playNote / 12) - 1, chordNoteCount);
                }
                else
                {
                    // Normal unison: all voices on same note
                    for (int i = 0; i < uvc; ++i)
                        voices[i].noteOn(playNote, vel, ++noteCounter);
                    debugConsole.log("[MIDI] NoteOn %s%d vel=%.2f -> UNISON (%d)",
                                     midiNoteName(playNote), (playNote / 12) - 1, vel, uvc);
                }
                lastUnisonNote = playNote;
            }
            // In legato modes (Low, Last without retrig): note changes but envelopes continue
            else if (!shouldRetrigger && isNewNote)
            {
                for (int i = 0; i < uvc; ++i)
                    voices[i].noteOn(playNote, vel, ++noteCounter);
                lastUnisonNote = playNote;
            }
        }
        else
        {
            // Polyphonic mode: same-note retrigger reuses same voice
            int voiceIdx = -1;
            bool retrigger = false;

            for (int i = 0; i < NUM_VOICES; ++i)
            {
                if (voices[i].getCurrentNote() == note && voices[i].isActive())
                {
                    voiceIdx = i;
                    retrigger = true;
                    break;
                }
            }

            bool stolen = false;
            if (voiceIdx < 0)
            {
                voiceIdx = findFreeVoice();
                if (voiceIdx < 0) { voiceIdx = findVoiceToSteal(); stolen = true; }
            }

            voices[voiceIdx].noteOn(note, vel, ++noteCounter);
            debugConsole.log("[MIDI] NoteOn %s%d vel=%.2f -> voice %d%s",
                             midiNoteName(note), (note / 12) - 1, vel, voiceIdx + 1,
                             retrigger ? " (RETRIG)" : stolen ? " (STOLEN)" : "");
        }
    }
    else if (msg.isNoteOff())
    {
        int note = msg.getNoteNumber();

        // Remove from held notes
        auto it = std::find(heldNotes.begin(), heldNotes.end(), note);
        if (it != heldNotes.end())
            heldNotes.erase(it);

        if (unisonActive)
        {
            // In unison with key priority, releasing a note might mean
            // we should switch to a still-held note
            if (!heldNotes.empty())
            {
                int uvc = juce::jlimit(1, NUM_VOICES,
                    static_cast<int>(apvts.getRawParameterValue("unisonVoices")->load()));

                int newNote;
                if (keyPriorityMode == 0 || keyPriorityMode == 1)
                    newNote = *std::min_element(heldNotes.begin(), heldNotes.end());
                else
                    newNote = heldNotes.back();

                if (newNote != lastUnisonNote)
                {
                    for (int i = 0; i < uvc; ++i)
                        voices[i].noteOn(newNote, 0.8f, ++noteCounter);
                    lastUnisonNote = newNote;
                }
            }
            else
            {
                // All keys released: release all voices
                for (int i = 0; i < NUM_VOICES; ++i)
                    if (voices[i].isActive() && !voices[i].isInRelease())
                        voices[i].noteOff();
                lastUnisonNote = -1;
            }
        }
        else
        {
            for (int i = 0; i < NUM_VOICES; ++i)
            {
                if (voices[i].getCurrentNote() == note && voices[i].isActive()
                    && !voices[i].isInRelease())
                {
                    voices[i].noteOff();
                    break;
                }
            }
        }
    }
    else if (msg.isPitchWheel())
    {
        float pitchWheelRange = apvts.getRawParameterValue("pitchWheelRange")->load();
        float normalized = (msg.getPitchWheelValue() - 8192) / 8192.0f;
        currentPitchBend = normalized * pitchWheelRange;
    }
    else if (msg.isProgramChange())
    {
        // Prophet-5: Program Change 0-39 selects program within current bank
        // Bank is set via CC 32 (Bank Select)
        // Each bank has 40 programs, total 200 factory + 200 user = 400
        int prog = msg.getProgramChangeNumber();
        int patchIndex = currentBank * 40 + juce::jlimit(0, 39, prog);
        if (patchIndex < getNumLoadedPatches())
        {
            selectPatch(patchIndex);
            debugConsole.log("[MIDI] ProgramChange %d (bank %d) -> patch %d",
                             prog, currentBank + 1, patchIndex + 1);
        }
    }
    else if (msg.isChannelPressure())
    {
        currentAftertouch = msg.getChannelPressureValue() / 127.0f;
    }
    else if (msg.isController())
    {
        int cc = msg.getControllerNumber();
        int raw = msg.getControllerValue();
        float val = raw / 127.0f;
        float val120 = raw / 120.0f;  // for 0-120 range params

        if (cc == 1) { currentModWheel = val; }

        // Prophet-5 CC map (from MIDI Implementation v1.4)
        auto setP = [&](const char* id, float v) {
            if (auto* p = apvts.getParameter(id))
                p->setValueNotifyingHost(p->convertTo0to1(v));
        };
        auto setBP = [&](const char* id, bool v) {
            if (auto* p = apvts.getParameter(id))
                p->setValueNotifyingHost(v ? 1.0f : 0.0f);
        };

        switch (cc)
        {
            case 32:  currentBank = juce::jlimit(0, 9, raw - 1);  // CC 32: Bank Select (1-10 -> 0-9)
                      debugConsole.log("[MIDI] Bank Select -> %d", currentBank + 1);
                      break;
            case 3:   setP("oscAFreq", SysExLoader::nrpnToOscFreq(raw)); break;
            case 7:   setP("masterVol", juce::jlimit(0.0f, 1.0f, val120)); break;
            case 9:   setP("oscBFreq", SysExLoader::nrpnToOscFreq(raw)); break;
            case 14:  setP("oscBFineTune", SysExLoader::nrpnToFineTune(raw)); break;
            case 15:  setBP("oscASaw", raw > 63); break;
            case 20:  setBP("oscAPulse", raw > 63); break;
            case 21:  setP("oscAPW", SysExLoader::nrpnToPulseWidth(raw)); break;
            case 22:  setP("oscBPW", SysExLoader::nrpnToPulseWidth(raw)); break;
            case 23:  setBP("oscSync", raw > 63); break;
            case 24:  setBP("oscBLowFreq", raw > 63); break;
            case 25:  setBP("oscBKbd", raw > 63); break;
            case 26:  setP("glideRate", SysExLoader::nrpnToGlideRate(raw)); break;
            case 27:  setP("mixOscA", juce::jlimit(0.0f, 1.0f, val120)); break;
            case 28:  setP("mixOscB", juce::jlimit(0.0f, 1.0f, val120)); break;
            case 29:  setP("mixNoise", juce::jlimit(0.0f, 1.0f, val120)); break;
            case 30:  setBP("oscBSaw", raw > 63); break;
            case 31:  setP("filterReso", SysExLoader::nrpnToResonance(raw)); break;
            case 35:  if (auto* p = apvts.getParameter("filterKeyTrack"))
                          p->setValueNotifyingHost(juce::jlimit(0, 2, raw) / 2.0f);
                      break;
            case 41:  if (auto* p = apvts.getParameter("filterRev"))
                          p->setValueNotifyingHost(raw > 63 ? 1.0f : 0.0f);
                      break;
            case 46:  setP("lfoFreq", SysExLoader::nrpnToLFOFreq(raw)); break;
            case 47:  setP("lfoAmount", juce::jlimit(0.0f, 1.0f, val120)); break;
            case 52:  setBP("oscBTri", raw > 63); break;
            case 53:  setP("lfoSrcMix", juce::jlimit(0.0f, 1.0f, val120)); break;
            case 54:  setBP("lfoToFreqA", raw > 63); break;
            case 55:  setBP("lfoToFreqB", raw > 63); break;
            case 56:  setBP("lfoToPWA", raw > 63); break;
            case 57:  setBP("lfoToPWB", raw > 63); break;
            case 58:  setBP("lfoToFilter", raw > 63); break;
            case 59:  setP("pmodFiltEnv", juce::jlimit(0.0f, 1.0f, val)); break;
            case 60:  setP("pmodOscB", juce::jlimit(0.0f, 1.0f, val120)); break;
            case 61:  setBP("pmodToFreqA", raw > 63); break;
            case 62:  setBP("pmodToPWA", raw > 63); break;
            case 63:  setBP("pmodToFilter", raw > 63); break;
            case 70:  setP("pitchWheelRange", static_cast<float>(juce::jlimit(1, 12, raw + 1))); break;
            case 71:  if (auto* p = apvts.getParameter("keyPriority"))
                          p->setValueNotifyingHost(juce::jlimit(0, 3, raw) / 3.0f);
                      break;
            case 73:  setP("filterCutoff", SysExLoader::nrpnToCutoffHz(raw)); break;
            case 85:  setP("vintage", SysExLoader::nrpnToVintage(raw)); break;
            case 86:  setBP("atToFilter", raw > 63); break;
            case 87:  setBP("atToLFO", raw > 63); break;
            case 89:  setP("filterEnvAmt", SysExLoader::nrpnToFilterEnvAmt(raw)); break;
            case 90:  setBP("velToFilter", raw > 63); break;
            case 102: setBP("velToAmp", raw > 63); break;
            case 103: setP("filtAtk", SysExLoader::nrpnToEnvTime(raw)); break;
            case 104: setP("ampAtk", SysExLoader::nrpnToEnvTime(raw)); break;
            case 105: setP("filtDec", SysExLoader::nrpnToEnvTime(raw)); break;
            case 106: setP("ampDec", SysExLoader::nrpnToEnvTime(raw)); break;
            case 107: setP("filtSus", SysExLoader::nrpnToSustain(raw)); break;
            case 108: setP("ampSus", SysExLoader::nrpnToSustain(raw)); break;
            case 109: setP("filtRel", SysExLoader::nrpnToEnvTime(raw)); break;
            case 110: setP("ampRel", SysExLoader::nrpnToEnvTime(raw)); break;
            case 111: setBP("releaseSwitch", raw > 63); break;
            case 112: setBP("unisonOn", raw > 63); break;
            case 113: setP("unisonVoices", static_cast<float>(juce::jlimit(1, 5, raw))); break;
            case 114: setP("unisonDetune", juce::jlimit(0.0f, 1.0f, raw / 7.0f)); break;
            case 116: setBP("oscBPulse", raw > 63); break;
            case 117: setBP("lfoSaw", raw > 63); break;
            case 118: setBP("lfoTri", raw > 63); break;
            case 119: setBP("lfoSquare", raw > 63); break;
            default: break;
        }

        debugConsole.log("[MIDI] CC%d = %d", cc, raw);
    }
}

int UltimateProphetProcessor::findFreeVoice() const
{
    for (int i = 0; i < NUM_VOICES; ++i)
        if (!voices[i].isActive()) return i;
    return -1;
}

int UltimateProphetProcessor::findVoiceToSteal() const
{
    int best = 0;
    float bestScore = std::numeric_limits<float>::max();
    for (int i = 0; i < NUM_VOICES; ++i)
    {
        float envVal = voices[i].getAmpEnvValue();
        bool releasing = voices[i].isInRelease();
        float score = releasing ? envVal : (2.0f + envVal);
        if (score < bestScore) { bestScore = score; best = i; }
    }
    return best;
}

// --- Boilerplate ---
const juce::String UltimateProphetProcessor::getName() const { return JucePlugin_Name; }
bool UltimateProphetProcessor::acceptsMidi() const { return true; }
bool UltimateProphetProcessor::producesMidi() const { return false; }
bool UltimateProphetProcessor::isMidiEffect() const { return false; }
double UltimateProphetProcessor::getTailLengthSeconds() const { return 0.0; }
int UltimateProphetProcessor::getNumPrograms() { return 1; }
int UltimateProphetProcessor::getCurrentProgram() { return 0; }
void UltimateProphetProcessor::setCurrentProgram(int) {}
const juce::String UltimateProphetProcessor::getProgramName(int) { return {}; }
void UltimateProphetProcessor::changeProgramName(int, const juce::String&) {}

juce::AudioProcessorEditor* UltimateProphetProcessor::createEditor()
{
    return new UltimateProphetEditor(*this);
}

bool UltimateProphetProcessor::hasEditor() const { return true; }

void UltimateProphetProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void UltimateProphetProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

void UltimateProphetProcessor::storeChordMemory(const std::vector<int>& notes)
{
    if (notes.empty()) return;

    // Sort notes and compute intervals from lowest
    auto sorted = notes;
    std::sort(sorted.begin(), sorted.end());

    int root = sorted[0];
    chordNoteCount = juce::jmin(static_cast<int>(sorted.size()), NUM_VOICES);
    for (int i = 0; i < chordNoteCount; ++i)
        chordIntervals[i] = sorted[static_cast<size_t>(i)] - root;

    chordMemoryActive = true;
    debugConsole.log("[CHORD] Stored %d notes: %d %d %d %d %d",
                     chordNoteCount,
                     chordNoteCount > 0 ? chordIntervals[0] : 0,
                     chordNoteCount > 1 ? chordIntervals[1] : 0,
                     chordNoteCount > 2 ? chordIntervals[2] : 0,
                     chordNoteCount > 3 ? chordIntervals[3] : 0,
                     chordNoteCount > 4 ? chordIntervals[4] : 0);
}

void UltimateProphetProcessor::clearChordMemory()
{
    chordMemoryActive = false;
    chordNoteCount = 0;
    for (auto& i : chordIntervals) i = 0;
    debugConsole.log("[CHORD] Cleared");
}

void UltimateProphetProcessor::loadSysExFile(const juce::File& file)
{
    auto patches = SysExLoader::loadFile(file);
    if (!patches.empty())
    {
        loadedPatches = std::move(patches);
        debugConsole.log("[SYSEX] Loaded %d patches from %s",
                         (int)loadedPatches.size(), file.getFileName().toRawUTF8());
        selectPatch(0);
    }
    else
    {
        debugConsole.log("[SYSEX] ERROR: No patches found in %s",
                         file.getFileName().toRawUTF8());
    }
}

void UltimateProphetProcessor::selectPatch(int index)
{
    if (index < 0 || index >= static_cast<int>(loadedPatches.size()))
        return;

    currentPatchIndex = index;
    SysExLoader::applyPatchToAPVTS(loadedPatches[static_cast<size_t>(index)], apvts);

    auto& patch = loadedPatches[static_cast<size_t>(index)];
    debugConsole.log("[PATCH] #%d: \"%s\" (group %d, prog %d)",
                     index, patch.name.toRawUTF8(), patch.group, patch.program);
}

juce::String UltimateProphetProcessor::getPatchName(int index) const
{
    if (index < 0 || index >= static_cast<int>(loadedPatches.size()))
        return {};
    return loadedPatches[static_cast<size_t>(index)].name;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UltimateProphetProcessor();
}
