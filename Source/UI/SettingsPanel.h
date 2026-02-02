#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "LookAndFeel.h"

/**
 * SettingsPanel allows audio device selection (input, output, sample rate).
 */
class SettingsPanel : public juce::Component,
                      public juce::ChangeListener
{
public:
    SettingsPanel(juce::AudioDeviceManager& deviceManager);
    ~SettingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    juce::AudioDeviceManager& audioDeviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
