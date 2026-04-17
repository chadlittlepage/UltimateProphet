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
        juce::NormalisableRange<float>(0.0f, 120.0f, 0.1f), 60.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("oscASaw", "Osc A Saw", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("oscAPulse", "Osc A Pulse", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "oscAPW", "Osc A Pulse Width",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.5f));

    // ===== OSCILLATOR B =====
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "oscBFreq", "Osc B Frequency",
        juce::NormalisableRange<float>(0.0f, 120.0f, 0.1f), 60.0f));
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
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.5f));
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
            voice.params.releaseSwitch = releaseSwitch;
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
    if (msg.isNoteOn())
    {
        int note = msg.getNoteNumber();
        float vel = msg.getFloatVelocity();

        if (unisonActive)
        {
            // Unison: trigger ALL voices on the same note
            for (int i = 0; i < NUM_VOICES; ++i)
                voices[i].noteOn(note, vel, ++noteCounter);
            debugConsole.log("[MIDI] NoteOn %s%d vel=%.2f -> UNISON (all 5)",
                             midiNoteName(note), (note / 12) - 1, vel);
        }
        else
        {
            // Prophet-5 behavior: "if you repeatedly hit the same key,
            // it will not cycle through voices; it will retrigger the same voice"
            int voiceIdx = -1;
            bool retrigger = false;

            // First: check if any voice is already playing this note
            for (int i = 0; i < NUM_VOICES; ++i)
            {
                if (voices[i].getCurrentNote() == note && voices[i].isActive())
                {
                    voiceIdx = i;
                    retrigger = true;
                    break;
                }
            }

            // If not retriggering, find a free voice or steal
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
        if (unisonActive)
        {
            // Release all voices playing this note
            for (int i = 0; i < NUM_VOICES; ++i)
                if (voices[i].getCurrentNote() == note && voices[i].isActive()
                    && !voices[i].isInRelease())
                    voices[i].noteOff();
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
    else if (msg.isController())
    {
        int cc = msg.getControllerNumber();
        float val = msg.getControllerValue() / 127.0f;
        if (cc == 1)  // Mod wheel
            currentModWheel = val;
        debugConsole.log("[MIDI] CC%d = %d", cc, msg.getControllerValue());
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
