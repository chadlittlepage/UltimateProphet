#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>

/**
 * Prophet-5 SysEx Patch Loader
 *
 * Parses real Prophet-5/Prophet-10 SysEx patch dumps.
 * Format: F0 01 31/32 02 [group] [program] [152 packed bytes] F7
 *
 * The 152 bytes unpack to 128 raw parameter bytes following NRPN order.
 * Most params are 0-120, switches are 0-1.
 */
class SysExLoader
{
public:
    static constexpr int PARAMS_PER_PATCH = 128;
    static constexpr int PACKED_SIZE = 152;
    static constexpr int MSG_SIZE = 6 + PACKED_SIZE + 1;  // header + data + F7 = 159

    struct Patch
    {
        int group = 0;          // 0-9 (0-4 user, 5-9 factory)
        int program = 0;        // 0-39
        std::array<uint8_t, PARAMS_PER_PATCH> params = {};
        juce::String name;      // extracted from reserved bytes if present
    };

    // Load all patches from a .syx file
    static std::vector<Patch> loadFile(const juce::File& file);

    // Load from raw SysEx data
    static std::vector<Patch> loadFromData(const uint8_t* data, size_t size);

    // Unpack 152 MIDI bytes → 128 raw parameter bytes
    static std::array<uint8_t, PARAMS_PER_PATCH> unpackData(const uint8_t* packed);

    // --- NRPN Parameter Indices ---
    enum NRPN
    {
        OSC_A_FREQ = 0,
        OSC_B_FREQ = 1,
        OSC_B_FINE = 2,
        OSC_A_SAW = 3,
        OSC_A_PULSE = 4,
        OSC_B_SAW = 5,
        OSC_B_TRI = 6,
        OSC_B_PULSE = 7,
        OSC_A_PW = 8,
        OSC_B_PW = 9,
        OSC_SYNC = 10,
        OSC_B_LOW_FREQ = 11,
        OSC_B_KBD = 12,
        GLIDE_RATE = 13,
        OSC_A_LEVEL = 14,
        OSC_B_LEVEL = 15,
        NOISE_LEVEL = 16,
        CUTOFF = 17,
        RESONANCE = 18,
        FILTER_KEY_TRACK = 19,
        FILTER_REV = 20,
        LFO_FREQ = 21,
        LFO_AMOUNT = 22,
        LFO_SAW = 23,
        LFO_TRI = 24,
        LFO_SQUARE = 25,
        LFO_SOURCE_MIX = 26,
        LFO_FREQ_A = 27,
        LFO_FREQ_B = 28,
        LFO_PW_A = 29,
        LFO_PW_B = 30,
        LFO_FILTER = 31,
        PMOD_FILT_ENV = 32,
        PMOD_OSC_B = 33,
        PMOD_FREQ_A = 34,
        PMOD_PW = 35,
        PMOD_FILTER = 36,
        VINTAGE = 37,
        AT_FILTER = 38,
        AT_AMP = 39,
        ENV_FILTER_AMT = 40,
        VEL_FILTER = 41,
        VEL_AMP = 42,
        ATTACK_FILTER = 43,
        ATTACK_VCA = 44,
        DECAY_FILTER = 45,
        DECAY_VCA = 46,
        SUSTAIN_FILTER = 47,
        SUSTAIN_VCA = 48,
        RELEASE_FILTER = 49,
        RELEASE_VCA = 50,
        RELEASE_SWITCH = 51,
        UNISON_ON = 52,
        UNISON_VOICES = 53,
        UNISON_DETUNE = 54,
        // 55-64: UNISON NOTE
        // 65-85: RESERVED / patch name
        PITCH_WHEEL_RANGE = 86,
        RETRIGGER_UNISON = 87,
    };

    // --- Value Conversion Functions ---
    // Convert Prophet-5 NRPN 0-120 values to our internal parameter ranges

    // Cutoff: 0-120 → 20-20000 Hz (exponential, 1V/oct like the VCO)
    static float nrpnToCutoffHz(int val);

    // Resonance: 0-120 → 0.0-1.0
    static float nrpnToResonance(int val);

    // Envelope time: 0-120 → 0.001-10.0 seconds (CEM 3310 exponential)
    static float nrpnToEnvTime(int val);

    // Sustain level: 0-120 → 0.0-1.0
    static float nrpnToSustain(int val);

    // Osc frequency: 0-120 → MIDI note number (maps to footage)
    static float nrpnToOscFreq(int val);

    // Pulse width: 0-120 → 0.05-0.95
    static float nrpnToPulseWidth(int val);

    // Level: 0-120 → 0.0-1.0
    static float nrpnToLevel(int val);

    // LFO frequency: 0-120 → 0.1-30 Hz (exponential)
    static float nrpnToLFOFreq(int val);

    // Filter env amount: 0-120 → -1.0-1.0 (bipolar, center at 60)
    static float nrpnToFilterEnvAmt(int val);

    // Vintage: 0-127 → 0.0-1.0
    static float nrpnToVintage(int val);

    // Fine tune: 0-127 → -7.0-7.0 semitones
    static float nrpnToFineTune(int val);

    // Glide rate: 0-120 → 0.0-1.0
    static float nrpnToGlideRate(int val);

    // Apply a loaded patch to a JUCE AudioProcessorValueTreeState
    static void applyPatchToAPVTS(const Patch& patch,
                                   juce::AudioProcessorValueTreeState& apvts);
};
