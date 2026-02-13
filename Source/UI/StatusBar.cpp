#include "StatusBar.h"
#include "../Audio/AudioEngine.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

StatusBar::StatusBar(juce::AudioDeviceManager& deviceManager)
    : audioDeviceManager(deviceManager)
{
    addAndMakeVisible(cpuLabel);
    cpuLabel.setText("CPU: 0%", juce::dontSendNotification);
    cpuLabel.setJustificationType(juce::Justification::centredLeft);
    cpuLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);

    addAndMakeVisible(inputLabel);
    inputLabel.setText("Input:", juce::dontSendNotification);
    inputLabel.setJustificationType(juce::Justification::centredRight);
    inputLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);

    addAndMakeVisible(deviceInfoLabel);
    deviceInfoLabel.setJustificationType(juce::Justification::centredRight);
    deviceInfoLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);

    // Dropout indicator label
    addAndMakeVisible(dropoutLabel);
    dropoutLabel.setText("Dropouts: 0", juce::dontSendNotification);
    dropoutLabel.setJustificationType(juce::Justification::centred);
    dropoutLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);

    // Memory usage label
    addAndMakeVisible(memoryLabel);
    memoryLabel.setText("Mem: -- MB", juce::dontSendNotification);
    memoryLabel.setJustificationType(juce::Justification::centred);
    memoryLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);

    // MIDI activity indicator
    addAndMakeVisible(midiActivityLabel);
    midiActivityLabel.setText("MIDI", juce::dontSendNotification);
    midiActivityLabel.setJustificationType(juce::Justification::centred);
    midiActivityLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

    // Auto-save indicator
    addAndMakeVisible(autoSaveLabel);
    autoSaveLabel.setText("", juce::dontSendNotification);
    autoSaveLabel.setJustificationType(juce::Justification::centred);
    autoSaveLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::playColour);

    // Update immediately
    timerCallback();

    // Poll every 500ms
    startTimer(500);
}

StatusBar::~StatusBar()
{
    stopTimer();
}

void StatusBar::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colours::black.withAlpha(0.6f));

    // Top border
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));

    // CPU Meter Bar (simple visual indicator)
    if (currentCpuUsage > 0.0)
    {
        g.setColour(currentCpuUsage > 0.5 ? juce::Colours::red : juce::Colours::green);
        juce::Rectangle<float> meterRect(60, 4, 100.0f * static_cast<float>(currentCpuUsage), static_cast<float>(getHeight()) - 8);
        g.fillRect(meterRect);
    }

    // Input level meters - positioned after the input label
    auto area = getLocalBounds().reduced(5, 0);
    area.removeFromLeft(200);  // Skip CPU area
    area.removeFromRight(300); // Skip device info area

    // Center section for input meters
    auto inputArea = area;
    auto labelWidth = 45;
    inputArea.removeFromLeft(labelWidth); // Skip "Input:" label

    // Draw stereo input meter (horizontal, stacked L/R)
    auto meterArea = inputArea.reduced(5, 4);
    int meterHeight = (meterArea.getHeight() - 2) / 2; // Split for L and R

    // Left channel meter
    auto leftMeterBounds = meterArea.removeFromTop(meterHeight);
    g.setColour(MidiSingLookAndFeel::sliderBackground);
    g.fillRect(leftMeterBounds);

    float leftWidth = static_cast<float>(leftMeterBounds.getWidth()) * inputLevelL;
    if (leftWidth > 0.0f)
    {
        auto levelColour = inputLevelL > 0.9f ? MidiSingLookAndFeel::recordColour : MidiSingLookAndFeel::playColour;
        g.setColour(levelColour);
        g.fillRect(leftMeterBounds.removeFromLeft(static_cast<int>(leftWidth)));
    }

    meterArea.removeFromTop(2); // Gap between L and R

    // Right channel meter
    auto rightMeterBounds = meterArea.removeFromTop(meterHeight);
    g.setColour(MidiSingLookAndFeel::sliderBackground);
    g.fillRect(rightMeterBounds);

    float rightWidth = static_cast<float>(rightMeterBounds.getWidth()) * inputLevelR;
    if (rightWidth > 0.0f)
    {
        auto levelColour = inputLevelR > 0.9f ? MidiSingLookAndFeel::recordColour : MidiSingLookAndFeel::playColour;
        g.setColour(levelColour);
        g.fillRect(rightMeterBounds.removeFromLeft(static_cast<int>(rightWidth)));
    }
}

void StatusBar::resized()
{
    auto area = getLocalBounds().reduced(5, 0);

    cpuLabel.setBounds(area.removeFromLeft(200));

    // Memory usage indicator after CPU
    memoryLabel.setBounds(area.removeFromLeft(90));

    // MIDI activity indicator
    midiActivityLabel.setBounds(area.removeFromLeft(40));

    // Auto-save indicator
    autoSaveLabel.setBounds(area.removeFromLeft(80));

    // Dropout indicator on the right side, before device info
    dropoutLabel.setBounds(area.removeFromRight(100));
    deviceInfoLabel.setBounds(area.removeFromRight(300));

    // Input label in the center area
    inputLabel.setBounds(area.removeFromLeft(45));
    // Rest of area is used for meter drawing in paint()
}

void StatusBar::timerCallback()
{
    currentCpuUsage = audioDeviceManager.getCpuUsage();

    juce::String cpuText = juce::String::formatted("CPU: %.1f%%", currentCpuUsage * 100.0);

    // Update memory usage statistics
    int64_t memoryUsageMB = 0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        memoryUsageMB = static_cast<int64_t>(pmc.WorkingSetSize / (1024 * 1024));
    }
#endif
    juce::String memoryText = juce::String::formatted("Mem: %lld MB", memoryUsageMB);
    memoryLabel.setText(memoryText, juce::dontSendNotification);

    // Set memory label color based on usage (warning at 1GB, critical at 2GB)
    if (memoryUsageMB > 2000)
    {
        memoryLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::recordColour);  // Red/critical
    }
    else if (memoryUsageMB > 1000)
    {
        memoryLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);  // Warning
    }
    else
    {
        memoryLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);  // Normal
    }

    if (auto* device = audioDeviceManager.getCurrentAudioDevice())
    {
        double sampleRate = device->getCurrentSampleRate();
        int bufferSize = device->getCurrentBufferSizeSamples();

        juce::String infoText = juce::String::formatted("%s | %.0f Hz | %d spls",
            device->getName().toRawUTF8(), sampleRate, bufferSize);
        deviceInfoLabel.setText(infoText, juce::dontSendNotification);
    }
    else
    {
        deviceInfoLabel.setText("No Audio Device", juce::dontSendNotification);
    }

    cpuLabel.setText(cpuText, juce::dontSendNotification);

    // Update dropout indicator if audio engine is available
    if (audioEngine != nullptr)
    {
        uint64_t currentDropoutCount = audioEngine->getDropoutCount();

        // Check if new dropouts occurred since last poll
        if (currentDropoutCount > lastDropoutCount)
        {
            // New dropouts detected - start flash animation
            dropoutFlashState = true;
            dropoutFlashCounter = 6;  // Flash for 6 timer cycles (3 seconds at 500ms interval)
        }

        lastDropoutCount = currentDropoutCount;

        // Update flash animation
        if (dropoutFlashCounter > 0)
        {
            dropoutFlashCounter--;
            dropoutFlashState = (dropoutFlashCounter % 2) == 1;  // Toggle on/off
        }
        else
        {
            dropoutFlashState = false;
        }

        // Update dropout label text and color
        juce::String dropoutText = juce::String::formatted("Dropouts: %llu", currentDropoutCount);
        dropoutLabel.setText(dropoutText, juce::dontSendNotification);

        // Set color based on dropout status
        if (currentDropoutCount == 0)
        {
            // No dropouts - green/normal
            dropoutLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::playColour);
        }
        else if (dropoutFlashState)
        {
            // Flashing red for recent dropouts
            dropoutLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::recordColour);
        }
        else
        {
            // Has dropouts but not flashing - yellow/warning
            dropoutLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
        }
    }

    // Update MIDI activity indicator
    if (midiActivityCounter > 0)
    {
        midiActivityCounter--;
        midiActivityLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::playColour);
    }
    else
    {
        midiActivityLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    }

    // Update auto-save indicator
    if (autoSaveDisplayCounter > 0)
    {
        autoSaveDisplayCounter--;
        if (autoSaveDisplayCounter == 0)
            autoSaveLabel.setText("", juce::dontSendNotification);
    }

    repaint();
}

void StatusBar::triggerMidiActivity()
{
    midiActivityCounter = 4; // Show for 2 seconds (4 x 500ms timer)
}

void StatusBar::showAutoSaveIndicator()
{
    autoSaveLabel.setText("Auto-saved", juce::dontSendNotification);
    autoSaveDisplayCounter = 6; // Show for 3 seconds (6 x 500ms timer)
}

void StatusBar::setInputLevel(float left, float right)
{
    inputLevelL = juce::jlimit(0.0f, 1.0f, left);
    inputLevelR = juce::jlimit(0.0f, 1.0f, right);
    repaint();
}

void StatusBar::setAudioEngine(AudioEngine* engine)
{
    audioEngine = engine;

    // Reset dropout tracking state
    lastDropoutCount = 0;
    dropoutFlashState = false;
    dropoutFlashCounter = 0;

    // Update display immediately if engine is valid
    if (audioEngine != nullptr)
    {
        lastDropoutCount = audioEngine->getDropoutCount();
        juce::String dropoutText = juce::String::formatted("Dropouts: %llu", lastDropoutCount);
        dropoutLabel.setText(dropoutText, juce::dontSendNotification);

        if (lastDropoutCount == 0)
        {
            dropoutLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::playColour);
        }
        else
        {
            dropoutLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
        }
    }
    else
    {
        dropoutLabel.setText("Dropouts: --", juce::dontSendNotification);
        dropoutLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    }
}
