#include "StatusBar.h"

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
    deviceInfoLabel.setBounds(area.removeFromRight(300));

    // Input label in the center area
    inputLabel.setBounds(area.removeFromLeft(45));
    // Rest of area is used for meter drawing in paint()
}

void StatusBar::timerCallback()
{
    currentCpuUsage = audioDeviceManager.getCpuUsage();

    juce::String cpuText = juce::String::formatted("CPU: %.1f%%", currentCpuUsage * 100.0);
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
    repaint();
}

void StatusBar::setInputLevel(float left, float right)
{
    inputLevelL = juce::jlimit(0.0f, 1.0f, left);
    inputLevelR = juce::jlimit(0.0f, 1.0f, right);
    repaint();
}
