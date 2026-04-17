#include "SysExLoader.h"
#include <cmath>

// ============================================================
//  SysEx Parsing
// ============================================================

std::vector<SysExLoader::Patch> SysExLoader::loadFile(const juce::File& file)
{
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data))
        return {};
    return loadFromData(static_cast<const uint8_t*>(data.getData()), data.getSize());
}

std::vector<SysExLoader::Patch> SysExLoader::loadFromData(const uint8_t* data, size_t size)
{
    std::vector<Patch> patches;

    size_t pos = 0;
    while (pos + 6 < size)
    {
        // Find SysEx start
        if (data[pos] != 0xF0) { pos++; continue; }

        // Check DSI manufacturer ID
        if (data[pos + 1] != 0x01) { pos++; continue; }

        // Prophet-5 (0x31) or Prophet-10 (0x32)
        uint8_t deviceId = data[pos + 2];
        if (deviceId != 0x31 && deviceId != 0x32) { pos++; continue; }

        // Program Data (0x02) or Edit Buffer (0x03)
        uint8_t msgType = data[pos + 3];

        if (msgType == 0x02)  // Program Data Dump
        {
            if (pos + MSG_SIZE > size) break;

            Patch patch;
            patch.group = data[pos + 4];
            patch.program = data[pos + 5];

            // Unpack 152 bytes starting at offset 6
            patch.params = unpackData(&data[pos + 6]);

            // Try to extract patch name from reserved bytes (65-85)
            juce::String name;
            for (int i = 65; i <= 85; ++i)
            {
                char c = static_cast<char>(patch.params[static_cast<size_t>(i)]);
                if (c >= 32 && c < 127)
                    name += juce::String::charToString(c);
            }
            patch.name = name.trim();

            patches.push_back(patch);
            pos += MSG_SIZE;
        }
        else if (msgType == 0x03)  // Edit Buffer
        {
            if (pos + MSG_SIZE - 2 > size) break;  // no group/program bytes

            Patch patch;
            patch.group = 0;
            patch.program = 0;
            patch.params = unpackData(&data[pos + 4]);

            juce::String name;
            for (int i = 65; i <= 85; ++i)
            {
                char c = static_cast<char>(patch.params[static_cast<size_t>(i)]);
                if (c >= 32 && c < 127)
                    name += juce::String::charToString(c);
            }
            patch.name = name.trim();

            patches.push_back(patch);
            pos += MSG_SIZE - 2;
        }
        else
        {
            pos++;
        }
    }

    return patches;
}

std::array<uint8_t, SysExLoader::PARAMS_PER_PATCH>
SysExLoader::unpackData(const uint8_t* packed)
{
    std::array<uint8_t, PARAMS_PER_PATCH> unpacked = {};

    // 152 packed bytes → 128 unpacked bytes
    // Every 8 packed bytes contain 7 data bytes:
    //   Byte 0: MSBs (bit 7 of bytes 1-7)
    //   Bytes 1-7: lower 7 bits of data
    int outIdx = 0;
    int inIdx = 0;

    while (outIdx < PARAMS_PER_PATCH && inIdx + 7 < PACKED_SIZE)
    {
        uint8_t msbs = packed[inIdx];
        int count = juce::jmin(7, PARAMS_PER_PATCH - outIdx);

        for (int i = 0; i < count; ++i)
        {
            uint8_t lo7 = packed[inIdx + 1 + i] & 0x7F;
            uint8_t hi1 = (msbs >> i) & 0x01;
            unpacked[static_cast<size_t>(outIdx)] = lo7 | (hi1 << 7);
            outIdx++;
        }
        inIdx += 8;
    }

    return unpacked;
}

// ============================================================
//  Value Conversion: NRPN → Internal Parameter Ranges
// ============================================================

float SysExLoader::nrpnToCutoffHz(int val)
{
    // Prophet-5 cutoff: 0-120 maps exponentially
    // At 0 = ~20 Hz, at 120 = ~20 kHz
    // 10 octaves across 120 steps = 12 steps per octave (1V/oct)
    float octaves = static_cast<float>(val) / 12.0f;
    return 20.0f * std::pow(2.0f, octaves);
}

float SysExLoader::nrpnToResonance(int val)
{
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(val) / 120.0f);
}

float SysExLoader::nrpnToEnvTime(int val)
{
    // CEM 3310: exponential timing across 4 decades
    // 0 = 1ms, 120 = 10s (confirmed from CEM 3310 datasheet)
    if (val <= 0) return 0.001f;
    float t = 0.001f * std::pow(10000.0f, static_cast<float>(val) / 120.0f);
    return juce::jlimit(0.001f, 10.0f, t);
}

float SysExLoader::nrpnToSustain(int val)
{
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(val) / 120.0f);
}

float SysExLoader::nrpnToOscFreq(int val)
{
    // 0-120 maps to MIDI note range
    return static_cast<float>(val);
}

float SysExLoader::nrpnToPulseWidth(int val)
{
    // 0-120 → 0.05-0.95
    return 0.05f + (static_cast<float>(val) / 120.0f) * 0.9f;
}

float SysExLoader::nrpnToLevel(int val)
{
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(val) / 120.0f);
}

float SysExLoader::nrpnToLFOFreq(int val)
{
    // 0-120 → 0.022-500 Hz exponential (per manual: ".022Hz to 500Hz")
    float t = static_cast<float>(val) / 120.0f;
    return 0.022f * std::pow(500.0f / 0.022f, t);
}

float SysExLoader::nrpnToFilterEnvAmt(int val)
{
    // Prophet-5 filter env amount is UNIPOLAR (0=none, 120=max)
    // The original hardware knob only goes from 0 to max.
    // Bipolar sweeps are achieved via Poly-Mod, not the env amount knob.
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(val) / 120.0f);
}

float SysExLoader::nrpnToVintage(int val)
{
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(val) / 127.0f);
}

float SysExLoader::nrpnToFineTune(int val)
{
    // 0-127 → 0.0 to 0.95 semitones (upward only, per manual)
    // "adjusts the tuning of Oscillator B upward (sharp) and has a range of nearly a semitone"
    return juce::jlimit(0.0f, 0.95f, static_cast<float>(val) / 127.0f * 0.95f);
}

float SysExLoader::nrpnToGlideRate(int val)
{
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(val) / 120.0f);
}

// ============================================================
//  Apply Patch to APVTS
// ============================================================

void SysExLoader::applyPatchToAPVTS(const Patch& patch,
                                     juce::AudioProcessorValueTreeState& apvts)
{
    auto& p = patch.params;

    auto setFloat = [&](const char* id, float val) {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(val));
    };
    auto setBool = [&](const char* id, bool val) {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(val ? 1.0f : 0.0f);
    };

    // Oscillator A
    setFloat("oscAFreq",  nrpnToOscFreq(p[OSC_A_FREQ]));
    setBool ("oscASaw",   p[OSC_A_SAW] > 0);
    setBool ("oscAPulse", p[OSC_A_PULSE] > 0);
    setFloat("oscAPW",    nrpnToPulseWidth(p[OSC_A_PW]));

    // Oscillator B
    setFloat("oscBFreq",     nrpnToOscFreq(p[OSC_B_FREQ]));
    setFloat("oscBFineTune", nrpnToFineTune(p[OSC_B_FINE]));
    setBool ("oscBSaw",      p[OSC_B_SAW] > 0);
    setBool ("oscBTri",      p[OSC_B_TRI] > 0);
    setBool ("oscBPulse",    p[OSC_B_PULSE] > 0);
    setFloat("oscBPW",       nrpnToPulseWidth(p[OSC_B_PW]));
    setBool ("oscBLowFreq",  p[OSC_B_LOW_FREQ] > 0);
    setBool ("oscBKbd",      p[OSC_B_KBD] > 0);
    setBool ("oscSync",      p[OSC_SYNC] > 0);

    // Mixer
    setFloat("mixOscA",  nrpnToLevel(p[OSC_A_LEVEL]));
    setFloat("mixOscB",  nrpnToLevel(p[OSC_B_LEVEL]));
    setFloat("mixNoise", nrpnToLevel(p[NOISE_LEVEL]));

    // Filter
    setFloat("filterCutoff",  nrpnToCutoffHz(p[CUTOFF]));
    setFloat("filterReso",    nrpnToResonance(p[RESONANCE]));
    setFloat("filterEnvAmt",  nrpnToFilterEnvAmt(p[ENV_FILTER_AMT]));

    // Filter key track: 0=Off, 1=Half, 2=Full
    if (auto* param = apvts.getParameter("filterKeyTrack"))
        param->setValueNotifyingHost(static_cast<float>(juce::jlimit(0, 2, (int)p[FILTER_KEY_TRACK])) / 2.0f);

    // Filter Rev: 0=Rev1/2 (SSM 2040), 1=Rev3 (CEM 3320)
    if (auto* param = apvts.getParameter("filterRev"))
        param->setValueNotifyingHost(static_cast<float>(juce::jlimit(0, 1, (int)p[FILTER_REV])));

    // LFO
    setFloat("lfoFreq",   nrpnToLFOFreq(p[LFO_FREQ]));
    setFloat("lfoAmount",  nrpnToLevel(p[LFO_AMOUNT]));
    setBool ("lfoSaw",    p[LFO_SAW] > 0);
    setBool ("lfoTri",    p[LFO_TRI] > 0);
    setBool ("lfoSquare", p[LFO_SQUARE] > 0);
    setBool ("lfoToFreqA",  p[LFO_FREQ_A] > 0);
    setBool ("lfoToFreqB",  p[LFO_FREQ_B] > 0);
    setBool ("lfoToPWA",    p[LFO_PW_A] > 0);
    setBool ("lfoToPWB",    p[LFO_PW_B] > 0);
    setBool ("lfoToFilter", p[LFO_FILTER] > 0);
    setFloat("lfoSrcMix",  nrpnToLevel(p[LFO_SOURCE_MIX]));

    // Poly-Mod
    setFloat("pmodFiltEnv", nrpnToLevel(p[PMOD_FILT_ENV]));  // 0-127 but treat as level
    setFloat("pmodOscB",    nrpnToLevel(p[PMOD_OSC_B]));
    setBool ("pmodToFreqA", p[PMOD_FREQ_A] > 0);
    setBool ("pmodToPWA",   p[PMOD_PW] > 0);
    setBool ("pmodToFilter", p[PMOD_FILTER] > 0);

    // Vintage
    setFloat("vintage", nrpnToVintage(p[VINTAGE]));

    // Aftertouch
    setBool("atToFilter", p[AT_FILTER] > 0);
    // AT > AMP maps to our AT > LFO (closest match)
    setBool("atToLFO",   p[AT_AMP] > 0);

    // Velocity
    setBool("velToFilter", p[VEL_FILTER] > 0);
    setBool("velToAmp",    p[VEL_AMP] > 0);

    // Filter Envelope
    setFloat("filtAtk", nrpnToEnvTime(p[ATTACK_FILTER]));
    setFloat("filtDec", nrpnToEnvTime(p[DECAY_FILTER]));
    setFloat("filtSus", nrpnToSustain(p[SUSTAIN_FILTER]));
    setFloat("filtRel", nrpnToEnvTime(p[RELEASE_FILTER]));

    // Amp Envelope
    setFloat("ampAtk", nrpnToEnvTime(p[ATTACK_VCA]));
    setFloat("ampDec", nrpnToEnvTime(p[DECAY_VCA]));
    setFloat("ampSus", nrpnToSustain(p[SUSTAIN_VCA]));
    setFloat("ampRel", nrpnToEnvTime(p[RELEASE_VCA]));

    // Release switch
    setBool("releaseSwitch", p[RELEASE_SWITCH] > 0);

    // Glide (Prophet-5: glide is always "on" when rate > 0)
    setFloat("glideRate", nrpnToGlideRate(p[GLIDE_RATE]));
    setBool ("glideOn",   p[GLIDE_RATE] > 0);

    // Unison
    setBool ("unisonOn", p[UNISON_ON] > 0);
    setFloat("unisonVoices", static_cast<float>(juce::jlimit(1, 5,
        p[UNISON_VOICES] > 0 ? static_cast<int>(p[UNISON_VOICES]) : 5)));
    setFloat("unisonDetune", static_cast<float>(juce::jlimit(0, 7,
        static_cast<int>(p[UNISON_DETUNE]))) / 7.0f);

    // Pitch wheel range
    setFloat("pitchWheelRange", static_cast<float>(juce::jlimit(1, 12, (int)p[PITCH_WHEEL_RANGE] + 1)));
}
