#include "StatusBar.h"

StatusBar::StatusBar(juce::AudioDeviceManager& deviceManager)
    : audioDeviceManager(deviceManager)
{
    addAndMakeVisible(cpuLabel);
    cpuLabel.setText("CPU: 0%", juce::dontSendNotification);
    cpuLabel.setJustificationType(juce::Justification::centredLeft);
    
    addAndMakeVisible(deviceInfoLabel);
    deviceInfoLabel.setJustificationType(juce::Justification::centredRight);
    
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
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());
    
    // CPU Meter Bar (simple visual indicator)
    if (currentCpuUsage > 0.0)
    {
        g.setColour(currentCpuUsage > 0.5 ? juce::Colours::red : juce::Colours::green);
        float width = (float)getWidth() * 0.1f * (float)currentCpuUsage; // Scale slightly
        // Or maybe just a small bar next to the text
        
        juce::Rectangle<float> meterRect(60, 4, 100 * (float)currentCpuUsage, (float)getHeight() - 8);
        g.fillRect(meterRect);
    }
}

void StatusBar::resized()
{
    auto area = getLocalBounds().reduced(5, 0);
    
    cpuLabel.setBounds(area.removeFromLeft(200));
    deviceInfoLabel.setBounds(area.removeFromRight(300));
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
