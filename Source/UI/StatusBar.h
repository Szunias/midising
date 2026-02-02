#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "LookAndFeel.h"

/**
 * StatusBar displays system information like CPU usage, sample rate, and input level.
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

    /** Set the input level for the meters (0.0 to 1.0 range) */
    void setInputLevel(float left, float right);

private:
    juce::AudioDeviceManager& audioDeviceManager;
    double currentCpuUsage = 0.0;

    // Input level meter values
    float inputLevelL = 0.0f;
    float inputLevelR = 0.0f;

    juce::Label cpuLabel;
    juce::Label inputLabel;
    juce::Label deviceInfoLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBar)
};
