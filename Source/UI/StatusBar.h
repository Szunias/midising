#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

class Transport;

/**
 * StatusBar displays status information and a visual beat indicator.
 * The beat indicator flashes on each beat, synchronized with the metronome.
 */
class StatusBar : public juce::Component,
                  public juce::Timer
{
public:
    StatusBar(juce::AudioDeviceManager& deviceManager);
    ~StatusBar() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Timer callback for updating beat indicator
    void timerCallback() override;

    // Set transport for beat calculation
    void setTransport(Transport* transport);

private:
    // Calculate current beat state
    void updateBeatState();

    // Draw beat indicator LED
    void drawBeatIndicator(juce::Graphics& g, juce::Rectangle<int> bounds);

    juce::AudioDeviceManager& deviceManager;
    Transport* transport = nullptr;

    // Beat indicator state
    bool isBeatActive = false;
    bool isAccentBeat = false;
    int64_t lastBeatSample = -1;
    int64_t beatFlashStartTime = 0;

    // Flash duration in milliseconds
    static constexpr int64_t flashDurationMs = 75;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBar)
};
