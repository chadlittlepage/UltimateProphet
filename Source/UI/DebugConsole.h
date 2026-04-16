#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

/**
 * Lock-free debug console for real-time audio plugin development.
 *
 * The audio thread writes short log entries into a ring buffer (no allocation,
 * no locks). The GUI timer reads them out and appends to a TextEditor.
 *
 * Also exposes atomic "live stats" that the GUI polls every frame:
 * voice activity, peak level, CPU load, sample rate, block size.
 */
class DebugConsole
{
public:
    // --- Ring buffer for log messages (audio thread -> GUI thread) ---

    static constexpr int MAX_MSG_LEN = 128;
    static constexpr int RING_SIZE = 256;

    struct LogEntry
    {
        char text[MAX_MSG_LEN] = {};
        std::atomic<bool> ready{false};
    };

    // Called from audio thread (or any thread). Non-blocking, fire-and-forget.
    void log(const char* fmt, ...);

    // Called from GUI thread. Returns number of new messages drained.
    int drain(juce::StringArray& out);

    // --- Live stats (written atomically from audio thread, read from GUI) ---

    struct VoiceStat
    {
        std::atomic<int>   note{-1};
        std::atomic<float> envLevel{0.0f};
        std::atomic<bool>  active{false};
        std::atomic<bool>  releasing{false};
    };

    static constexpr int MAX_VOICES = 5;
    VoiceStat voiceStats[MAX_VOICES];

    std::atomic<float> peakLevel{0.0f};
    std::atomic<float> cpuLoad{0.0f};
    std::atomic<double> currentSampleRate{44100.0};
    std::atomic<int>   currentBlockSize{512};
    std::atomic<int>   oversampleFactor{4};

private:
    std::array<LogEntry, RING_SIZE> ring;
    std::atomic<int> writePos{0};
    std::atomic<int> readPos{0};
};

/**
 * GUI component: collapsible console panel that renders the debug output.
 */
class DebugConsolePanel : public juce::Component,
                           public juce::Timer
{
public:
    explicit DebugConsolePanel(DebugConsole& console);
    ~DebugConsolePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    bool isConsoleVisible() const { return expanded; }
    void toggleVisibility();

    static constexpr int COLLAPSED_HEIGHT = 22;
    static constexpr int EXPANDED_HEIGHT = 200;

private:
    DebugConsole& console;
    bool expanded = true;

    juce::TextEditor logView;
    juce::TextButton toggleButton{"Console"};

    // Status bar labels
    juce::Label voiceStatusLabel;
    juce::Label peakLabel;
    juce::Label cpuLabel;
    juce::Label sampleRateLabel;

    void updateStatusBar();

    static juce::String noteName(int midiNote);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebugConsolePanel)
};
