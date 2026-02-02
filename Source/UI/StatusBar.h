#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "LookAndFeel.h"

/**
 * StatusBar displays system information like CPU usage and sample rate.
 */
class StatusBar : public juce::Component,
                  public juce::Timer
{
public:
    StatusBar(juce::AudioDeviceManager& deviceManager);
    ~StatusBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Timer callback for polling CPU usage
    void timerCallback() override;

private:
    juce::AudioDeviceManager& audioDeviceManager;
    double currentCpuUsage = 0.0;
    
    juce::Label cpuLabel;
    juce::Label deviceInfoLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBar)
};
