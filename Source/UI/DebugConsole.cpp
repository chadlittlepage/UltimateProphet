#include "DebugConsole.h"
#include <cstdarg>
#include <cstdio>

// ---------- DebugConsole (lock-free ring buffer) ----------

void DebugConsole::log(const char* fmt, ...)
{
    int pos = writePos.load(std::memory_order_relaxed);
    int next = (pos + 1) % RING_SIZE;

    // If ring is full, drop the message (never block audio thread)
    if (next == readPos.load(std::memory_order_acquire))
        return;

    auto& entry = ring[static_cast<size_t>(pos)];

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.text, MAX_MSG_LEN, fmt, args);
    va_end(args);

    entry.ready.store(true, std::memory_order_release);
    writePos.store(next, std::memory_order_release);
}

int DebugConsole::drain(juce::StringArray& out)
{
    int count = 0;
    while (readPos.load(std::memory_order_relaxed) != writePos.load(std::memory_order_acquire))
    {
        int pos = readPos.load(std::memory_order_relaxed);
        auto& entry = ring[static_cast<size_t>(pos)];

        if (entry.ready.load(std::memory_order_acquire))
        {
            out.add(juce::String::fromUTF8(entry.text));
            entry.ready.store(false, std::memory_order_release);
            readPos.store((pos + 1) % RING_SIZE, std::memory_order_release);
            ++count;
        }
        else
        {
            break;
        }
    }
    return count;
}

// ---------- DebugConsolePanel (GUI component) ----------

DebugConsolePanel::DebugConsolePanel(DebugConsole& c)
    : console(c)
{
    // Log view
    logView.setMultiLine(true);
    logView.setReadOnly(true);
    logView.setScrollbarsShown(true);
    logView.setCaretVisible(false);
    logView.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, 0));
    logView.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0A0A0E));
    logView.setColour(juce::TextEditor::textColourId, juce::Colour(0xff40FF40));
    logView.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2A2A30));
    addAndMakeVisible(logView);

    // Toggle button
    toggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1A1A1E));
    toggleButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffD4A843));
    toggleButton.onClick = [this] { toggleVisibility(); };
    addAndMakeVisible(toggleButton);

    // Status labels
    auto setupLabel = [this](juce::Label& label)
    {
        label.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, 0));
        label.setColour(juce::Label::textColourId, juce::Colour(0xff80C0FF));
        label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(label);
    };

    setupLabel(voiceStatusLabel);
    setupLabel(peakLabel);
    setupLabel(cpuLabel);
    setupLabel(sampleRateLabel);

    startTimerHz(15);  // 15fps console refresh
}

DebugConsolePanel::~DebugConsolePanel()
{
    stopTimer();
}

void DebugConsolePanel::toggleVisibility()
{
    expanded = !expanded;
    toggleButton.setButtonText(expanded ? "Console [-]" : "Console [+]");

    if (auto* parent = getParentComponent())
        parent->resized();  // trigger re-layout
}

void DebugConsolePanel::timerCallback()
{
    // Drain new log messages
    juce::StringArray newMessages;
    console.drain(newMessages);

    if (!newMessages.isEmpty())
    {
        for (auto& msg : newMessages)
        {
            logView.moveCaretToEnd();
            logView.insertTextAtCaret(msg + "\n");
        }

        // Keep log from growing unbounded
        auto fullText = logView.getText();
        int lineCount = fullText.length() - fullText.replaceCharacter('\n', ' ').length();
        if (lineCount > 500)
        {
            // Trim to last 300 lines
            int idx = 0;
            int trimLines = lineCount - 300;
            for (int i = 0; i < trimLines && idx < fullText.length(); ++i)
                idx = fullText.indexOf(idx, "\n") + 1;
            logView.setText(fullText.substring(idx));
            logView.moveCaretToEnd();
        }
    }

    updateStatusBar();
}

void DebugConsolePanel::updateStatusBar()
{
    // Voice status
    juce::String voiceStr = "Voices: ";
    for (int i = 0; i < DebugConsole::MAX_VOICES; ++i)
    {
        auto& vs = console.voiceStats[i];
        if (vs.active.load())
        {
            int note = vs.note.load();
            float env = vs.envLevel.load();
            bool rel = vs.releasing.load();
            voiceStr += juce::String(i + 1) + ":" + noteName(note)
                     + (rel ? "~" : "") + "(" + juce::String(env, 2) + ") ";
        }
        else
        {
            voiceStr += juce::String(i + 1) + ":-- ";
        }
    }
    voiceStatusLabel.setText(voiceStr.trimEnd(), juce::dontSendNotification);

    float peak = console.peakLevel.load();
    float peakDb = (peak > 0.0001f) ? 20.0f * std::log10(peak) : -96.0f;
    peakLabel.setText("Peak: " + juce::String(peakDb, 1) + " dB", juce::dontSendNotification);

    cpuLabel.setText("CPU: " + juce::String(console.cpuLoad.load(), 1) + "%",
                     juce::dontSendNotification);

    double sr = console.currentSampleRate.load();
    int bs = console.currentBlockSize.load();
    int os = console.oversampleFactor.load();
    sampleRateLabel.setText(juce::String(static_cast<int>(sr)) + " Hz / "
                           + juce::String(bs) + " smp / "
                           + juce::String(os) + "x OS",
                           juce::dontSendNotification);
}

void DebugConsolePanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0E0E12));

    // Top border
    g.setColour(juce::Colour(0xff3A3A40));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
}

void DebugConsolePanel::resized()
{
    auto bounds = getLocalBounds();

    // Toggle button in the top-left
    toggleButton.setBounds(bounds.removeFromTop(COLLAPSED_HEIGHT).reduced(2, 1));

    if (!expanded)
        return;

    // Status bar row
    auto statusRow = bounds.removeFromTop(18);
    int statusW = statusRow.getWidth() / 4;
    voiceStatusLabel.setBounds(statusRow.removeFromLeft(statusW * 2));
    peakLabel.setBounds(statusRow.removeFromLeft(statusW / 2));
    cpuLabel.setBounds(statusRow.removeFromLeft(statusW / 2));
    sampleRateLabel.setBounds(statusRow);

    // Log view fills the rest
    logView.setBounds(bounds.reduced(2, 1));
}

juce::String DebugConsolePanel::noteName(int midiNote)
{
    if (midiNote < 0 || midiNote > 127) return "--";
    static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int octave = (midiNote / 12) - 1;
    return juce::String(names[midiNote % 12]) + juce::String(octave);
}
