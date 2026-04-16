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
    debugConsole.log("[INIT] UltimateProphet v0.2.0");
}

UltimateProphetProcessor::~UltimateProphetProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
UltimateProphetProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // --- Oscillator A ---
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "oscAWaveform", "Osc A Waveform",
        juce::StringArray{"Saw", "Pulse", "Triangle"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "oscALevel", "Osc A Level", 0.0f, 1.0f, 1.0f));

    // --- Oscillator B ---
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "oscBWaveform", "Osc B Waveform",
        juce::StringArray{"Saw", "Pulse", "Triangle"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "oscBLevel", "Osc B Level", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "oscBDetune", "Osc B Detune",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));

    // --- Pulse Width ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pulseWidth", "Pulse Width",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.5f));

    // --- Mixer ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "noiseLevel", "Noise Level", 0.0f, 1.0f, 0.0f));

    // --- Filter ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterCutoff", "Filter Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 10000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterResonance", "Filter Resonance", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterEnvAmount", "Filter Env Amount", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterKeyTrack", "Filter Key Track", 0.0f, 1.0f, 0.5f));

    // --- Filter Envelope ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterAttack", "Filter Attack",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterDecay", "Filter Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterSustain", "Filter Sustain", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterRelease", "Filter Release",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));

    // --- Amp Envelope ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ampAttack", "Amp Attack",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ampDecay", "Amp Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ampSustain", "Amp Sustain", 0.0f, 1.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ampRelease", "Amp Release",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));

    // --- Poly-Mod ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "polyModFiltEnvOscA", "Poly-Mod: Filt Env > Osc A", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "polyModFiltEnvFilter", "Poly-Mod: Filt Env > Filter", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "polyModOscBOscA", "Poly-Mod: Osc B > Osc A", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "polyModOscBFilter", "Poly-Mod: Osc B > Filter", -1.0f, 1.0f, 0.0f));

    // --- Misc ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "oscSync", "Osc Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "analogDrift", "Analog Drift", 0.0f, 1.0f, 0.05f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "masterVolume", "Master Volume", 0.0f, 1.0f, 0.7f));

    // --- External Input ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "extInputLevel", "Ext Input Level", 0.0f, 1.0f, 0.0f));

    return { params.begin(), params.end() };
}

void UltimateProphetProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lastSampleRate = sampleRate;
    lastBlockSize = samplesPerBlock;

    for (auto& voice : voices)
        voice.prepare(sampleRate);

    externalFilter.prepare(sampleRate, Prophet5Voice::OVERSAMPLE_FACTOR);

    // Initialize smoothed values (20ms ramp time for most, 50ms for volume)
    smoothedCutoff.reset(sampleRate, 0.02);
    smoothedResonance.reset(sampleRate, 0.02);
    smoothedPulseWidth.reset(sampleRate, 0.02);
    smoothedMasterVolume.reset(sampleRate, 0.05);
    smoothedOscALevel.reset(sampleRate, 0.02);
    smoothedOscBLevel.reset(sampleRate, 0.02);
    smoothedNoiseLevel.reset(sampleRate, 0.02);
    smoothedExtInput.reset(sampleRate, 0.02);

    debugConsole.currentSampleRate.store(sampleRate);
    debugConsole.currentBlockSize.store(samplesPerBlock);
    debugConsole.oversampleFactor.store(Prophet5Voice::OVERSAMPLE_FACTOR);
    debugConsole.log("[AUDIO] prepareToPlay: %.0f Hz, %d samples, %dx OS",
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

    // Merge keyboard MIDI into the incoming buffer
    {
        const juce::SpinLock::ScopedLockType lock(keyboardMidiLock);
        for (const auto metadata : keyboardMidiBuffer)
            midiMessages.addEvent(metadata.getMessage(), metadata.samplePosition);
        keyboardMidiBuffer.clear();
    }

    // Read input audio if available (for external filter mode)
    int numInputChannels = getTotalNumInputChannels();
    const float* inputLeft = (numInputChannels > 0) ? buffer.getReadPointer(0) : nullptr;

    buffer.clear();

    // Set smoothed parameter targets from APVTS (once per block)
    auto oscAWf = static_cast<CEM3340Oscillator::Waveform>(
        apvts.getRawParameterValue("oscAWaveform")->load());
    auto oscBWf = static_cast<CEM3340Oscillator::Waveform>(
        apvts.getRawParameterValue("oscBWaveform")->load());

    smoothedCutoff.setTargetValue(apvts.getRawParameterValue("filterCutoff")->load());
    smoothedResonance.setTargetValue(apvts.getRawParameterValue("filterResonance")->load());
    smoothedPulseWidth.setTargetValue(apvts.getRawParameterValue("pulseWidth")->load());
    smoothedMasterVolume.setTargetValue(apvts.getRawParameterValue("masterVolume")->load());
    smoothedOscALevel.setTargetValue(apvts.getRawParameterValue("oscALevel")->load());
    smoothedOscBLevel.setTargetValue(apvts.getRawParameterValue("oscBLevel")->load());
    smoothedNoiseLevel.setTargetValue(apvts.getRawParameterValue("noiseLevel")->load());
    smoothedExtInput.setTargetValue(apvts.getRawParameterValue("extInputLevel")->load());

    // Per-block params that don't need smoothing (envelopes, choices, booleans)
    float filterEnvAmount = apvts.getRawParameterValue("filterEnvAmount")->load();
    float filterKeyTrack = apvts.getRawParameterValue("filterKeyTrack")->load();
    float filterAttack = apvts.getRawParameterValue("filterAttack")->load();
    float filterDecay = apvts.getRawParameterValue("filterDecay")->load();
    float filterSustain = apvts.getRawParameterValue("filterSustain")->load();
    float filterRelease = apvts.getRawParameterValue("filterRelease")->load();
    float ampAttack = apvts.getRawParameterValue("ampAttack")->load();
    float ampDecay = apvts.getRawParameterValue("ampDecay")->load();
    float ampSustain = apvts.getRawParameterValue("ampSustain")->load();
    float ampRelease = apvts.getRawParameterValue("ampRelease")->load();
    float polyModFEOscA = apvts.getRawParameterValue("polyModFiltEnvOscA")->load();
    float polyModFEFilt = apvts.getRawParameterValue("polyModFiltEnvFilter")->load();
    float polyModOBOscA = apvts.getRawParameterValue("polyModOscBOscA")->load();
    float polyModOBFilt = apvts.getRawParameterValue("polyModOscBFilter")->load();
    bool oscSync = apvts.getRawParameterValue("oscSync")->load() > 0.5f;
    float analogDrift = apvts.getRawParameterValue("analogDrift")->load();
    float oscBDetune = apvts.getRawParameterValue("oscBDetune")->load();

    // Process MIDI
    for (const auto metadata : midiMessages)
        handleMidiMessage(metadata.getMessage());

    // Debug: periodic signal level logging
    static int debugCounter = 0;
    bool shouldLog = (++debugCounter % 1000 == 0);  // every ~1000 blocks

    // Render
    int numSamples = buffer.getNumSamples();
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    float blockPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Advance smoothed parameters per-sample
        float cutoff = smoothedCutoff.getNextValue();
        float resonance = smoothedResonance.getNextValue();
        float pw = smoothedPulseWidth.getNextValue();
        float vol = smoothedMasterVolume.getNextValue();
        float oscALvl = smoothedOscALevel.getNextValue();
        float oscBLvl = smoothedOscBLevel.getNextValue();
        float noiseLvl = smoothedNoiseLevel.getNextValue();
        float extLevel = smoothedExtInput.getNextValue();

        // Update voice params with smoothed values
        for (auto& voice : voices)
        {
            voice.params.oscAWaveform = oscAWf;
            voice.params.oscALevel = oscALvl;
            voice.params.oscBWaveform = oscBWf;
            voice.params.oscBLevel = oscBLvl;
            voice.params.oscBDetune = oscBDetune;
            voice.params.pulseWidth = pw;
            voice.params.noiseLevel = noiseLvl;
            voice.params.filterCutoff = cutoff;
            voice.params.filterResonance = resonance;
            voice.params.filterEnvAmount = filterEnvAmount;
            voice.params.filterKeyTrack = filterKeyTrack;
            voice.params.filterAttack = filterAttack;
            voice.params.filterDecay = filterDecay;
            voice.params.filterSustain = filterSustain;
            voice.params.filterRelease = filterRelease;
            voice.params.ampAttack = ampAttack;
            voice.params.ampDecay = ampDecay;
            voice.params.ampSustain = ampSustain;
            voice.params.ampRelease = ampRelease;
            voice.params.polyModFilterEnvToOscA = polyModFEOscA;
            voice.params.polyModFilterEnvToFilter = polyModFEFilt;
            voice.params.polyModOscBToOscA = polyModOBOscA;
            voice.params.polyModOscBToFilter = polyModOBFilt;
            voice.params.oscSync = oscSync;
            voice.params.analogDrift = analogDrift;
        }

        float sum = 0.0f;
        for (auto& voice : voices)
            sum += voice.process();

        // External audio input through the CEM3320 filter
        if (extLevel > 0.001f && inputLeft != nullptr)
        {
            externalFilter.setCutoff(cutoff);
            externalFilter.setResonance(resonance);

            float extSample = inputLeft[sample] * extLevel;

            // 4x oversampled filter on external input (sample-and-hold)
            float extFiltered = 0.0f;
            for (int os = 0; os < Prophet5Voice::OVERSAMPLE_FACTOR; ++os)
                extFiltered = externalFilter.process(extSample);
            sum += extFiltered;
        }

        // Debug log on first sample of periodic blocks
        if (shouldLog && sample == 0 && std::abs(sum) > 0.0001f)
        {
            debugConsole.log("[DSP] sum=%.4f vol=%.2f cutoff=%.0f reso=%.2f",
                             sum, vol, cutoff, resonance);
            shouldLog = false;
        }

        // Soft clip to prevent overs
        sum = std::tanh(sum * vol);

        float absSample = std::abs(sum);
        if (absSample > blockPeak)
            blockPeak = absSample;

        leftChannel[sample] = sum;
        if (rightChannel)
            rightChannel[sample] = sum;
    }

    // Update debug console stats (atomic, no locks)
    debugConsole.peakLevel.store(blockPeak);

    for (int i = 0; i < NUM_VOICES; ++i)
    {
        debugConsole.voiceStats[i].active.store(voices[i].isActive());
        debugConsole.voiceStats[i].note.store(voices[i].getCurrentNote());
        debugConsole.voiceStats[i].envLevel.store(voices[i].getAmpEnvValue());
        debugConsole.voiceStats[i].releasing.store(voices[i].isInRelease());
    }

    // CPU load estimate
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

        int voiceIdx = findFreeVoice();
        bool stolen = false;
        if (voiceIdx < 0)
        {
            voiceIdx = findVoiceToSteal();
            stolen = true;
        }

        voices[voiceIdx].noteOn(note, vel, ++noteCounter);

        int octave = (note / 12) - 1;
        if (stolen)
            debugConsole.log("[MIDI] NoteOn %s%d (v%d) vel=%.2f -> voice %d (STOLEN)",
                             midiNoteName(note), octave, note, vel, voiceIdx + 1);
        else
            debugConsole.log("[MIDI] NoteOn %s%d (v%d) vel=%.2f -> voice %d",
                             midiNoteName(note), octave, note, vel, voiceIdx + 1);
    }
    else if (msg.isNoteOff())
    {
        int note = msg.getNoteNumber();
        int octave = (note / 12) - 1;
        for (int i = 0; i < NUM_VOICES; ++i)
        {
            if (voices[i].getCurrentNote() == note && voices[i].isActive()
                && !voices[i].isInRelease())
            {
                voices[i].noteOff();
                debugConsole.log("[MIDI] NoteOff %s%d -> voice %d (release)",
                                 midiNoteName(note), octave, i + 1);
                break;
            }
        }
    }
    else if (msg.isPitchWheel())
    {
        debugConsole.log("[MIDI] PitchWheel: %d", msg.getPitchWheelValue());
    }
    else if (msg.isController())
    {
        debugConsole.log("[MIDI] CC%d = %d", msg.getControllerNumber(), msg.getControllerValue());
    }
}

int UltimateProphetProcessor::findFreeVoice() const
{
    for (int i = 0; i < NUM_VOICES; ++i)
    {
        if (!voices[i].isActive())
            return i;
    }
    return -1;
}

int UltimateProphetProcessor::findVoiceToSteal() const
{
    // Priority: steal voices in release first (quietest), then oldest active
    int best = 0;
    float bestScore = std::numeric_limits<float>::max();

    for (int i = 0; i < NUM_VOICES; ++i)
    {
        float envVal = voices[i].getAmpEnvValue();
        bool releasing = voices[i].isInRelease();

        // Lower score = more stealable.
        // Releasing voices get score 0..1 (their env value).
        // Active voices get score 2..3 (offset so releasing always wins).
        float score = releasing ? envVal : (2.0f + envVal);

        if (score < bestScore)
        {
            bestScore = score;
            best = i;
        }
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UltimateProphetProcessor();
}
