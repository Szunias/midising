#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "LookAndFeel.h"

// Forward declaration
class AudioEngine;

/**
 * StatusBar displays system information like CPU usage, sample rate, input level,
 * and audio dropout indicators.
 */
class StatusBar : public juce::Component,
                  public juce::Timer
{
public:
    StatusBar(juce::AudioDeviceManager& deviceManager);
    ~StatusBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Timer callback for polling CPU usage and dropout status
    void timerCallback() override;

    /** Set the input level for the meters (0.0 to 1.0 range) */
    void setInputLevel(float left, float right);

    /** Set the audio engine for dropout monitoring */
    void setAudioEngine(AudioEngine* engine);

private:
    juce::AudioDeviceManager& audioDeviceManager;
    AudioEngine* audioEngine = nullptr;
    double currentCpuUsage = 0.0;

    // Input level meter values
    float inputLevelL = 0.0f;
    float inputLevelR = 0.0f;

    // Dropout tracking
    uint64_t lastDropoutCount = 0;
    bool dropoutFlashState = false;    // For flashing the indicator on new dropouts
    int dropoutFlashCounter = 0;       // Counter for flash animation

    juce::Label cpuLabel;
    juce::Label inputLabel;
    juce::Label deviceInfoLabel;
    juce::Label dropoutLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBar)
};
